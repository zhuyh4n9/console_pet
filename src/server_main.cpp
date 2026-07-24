// Console Pet server: owns the pet, runs the simulation, the shop, and the LLM
// "brain"; clients connect over TCP to control it and read its state.

#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "console_pet/brain.hpp"
#include "console_pet/config.hpp"
#include "console_pet/net.hpp"
#include "console_pet/proto.hpp"
#include "console_pet/save.hpp"
#include "console_pet/shop.hpp"

namespace {

volatile std::sig_atomic_t g_stop = 0;
void on_signal(int) { g_stop = 1; }

constexpr int kTickMs = 2400;   // simulation tick period (~the original pace)
constexpr int kSaveMs = 20000;  // autosave period

}  // namespace

namespace pet {

class Server {
public:
    Server() {
        config_ = load_config();
        if (auto loaded = load_pet()) {
            pet_ = std::move(*loaded);
        } else {
            pet_ = Pet("Buddy");
            pet_.events.add(
                std::format("🥚 A new pet has hatched! Meet {}!", pet_.name),
                Color::Magenta);
        }
        brain_ = std::make_unique<Brain>(config_.llm);
        if (brain_->enabled()) {
            trigger_reaction("The user just opened the game and came to see you.");
        }
    }

    int run() {
        const int listen_fd = net_listen(config_.host, config_.port);
        if (listen_fd < 0) {
            std::cerr << "Failed to listen on " << config_.host << ":"
                      << config_.port << "\n";
            return 1;
        }
        std::cout << "Console Pet server listening on " << config_.host << ":"
                  << config_.port << "\n";

        std::vector<int> clients;
        using clock = std::chrono::steady_clock;
        auto last_tick = clock::now();
        auto last_save = clock::now();

        while (!g_stop) {
            std::vector<struct pollfd> fds;
            fds.push_back({listen_fd, POLLIN, 0});
            for (int c : clients) fds.push_back({c, POLLIN, 0});

            const auto now = clock::now();
            const int elapsed = static_cast<int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                                      last_tick)
                    .count());
            poll(fds.data(), fds.size(), std::max(0, kTickMs - elapsed));

            // New connections.
            if (fds[0].revents & POLLIN) {
                const int c = net_accept(listen_fd);
                if (c >= 0) clients.push_back(c);
            }
            // Client commands.
            for (std::size_t i = 0; i < clients.size(); ++i) {
                if (fds[i + 1].revents & (POLLIN | POLLHUP | POLLERR)) {
                    nlohmann::json cmd;
                    if (net_recv(clients[i], cmd)) {
                        const nlohmann::json resp = handle_command(cmd);
                        if (!net_send(clients[i], resp)) {
                            close(clients[i]);
                            clients[i] = -1;
                        }
                    } else {
                        close(clients[i]);  // client disconnected
                        clients[i] = -1;
                    }
                }
            }
            clients.erase(std::remove(clients.begin(), clients.end(), -1),
                          clients.end());

            // Simulation tick.
            const auto now2 = clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now2 -
                                                                      last_tick)
                    .count() >= kTickMs) {
                pet_.tick();
                last_tick = now2;
            }

            // Apply finished LLM results (reactions -> speech, chat -> log).
            LlmResult res;
            while (brain_->poll(res)) {
                if (res.kind == LlmKind::Reaction) {
                    pet_.say(res.text);
                } else {
                    chat_log_.push_back({pet_.name, res.text, true});
                    chat_pending_ = false;
                }
            }

            // Periodic autosave.
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now2 -
                                                                      last_save)
                    .count() >= kSaveMs) {
                save_pet(pet_);
                last_save = now2;
            }
        }

        save_pet(pet_);
        for (int c : clients) close(c);
        close(listen_fd);
        std::cout << "Server stopped; pet saved.\n";
        return 0;
    }

private:
    nlohmann::json handle_command(const nlohmann::json& cmd) {
        const std::string c = cmd.value("cmd", std::string("poll"));
        if (c == "feed") {
            pet_.feed();
            trigger_reaction("The user just fed you.");
        } else if (c == "snack") {
            pet_.add_snack();
            trigger_reaction("The user stocked a snack for you.");
        } else if (c == "play") {
            pet_.play();
            trigger_reaction("The user just played with you.");
        } else if (c == "sleep") {
            pet_.sleep();
            trigger_reaction("The user is sending you to sleep.");
        } else if (c == "heal") {
            pet_.heal();
            trigger_reaction("The user just gave you medicine.");
        } else if (c == "work") {
            pet_.work();
            trigger_reaction("The user just sent you to work.");
        } else if (c == "restart") {
            restart();
        } else if (c == "buy") {
            handle_buy(cmd.value("id", std::string()));
        } else if (c == "chat") {
            handle_chat(cmd.value("text", std::string()));
        }
        // "poll" and unknown commands simply return the current state.
        return make_state();
    }

    nlohmann::json make_state() {
        const Mood mood = compute_mood(pet_.stats, pet_.sleeping_ticks,
                                       pet_.working_ticks, pet_.frame_idx,
                                       pet_.prev_mood);
        pet_.prev_mood = mood;  // server owns the hysteresis state
        return {{"ok", true},
                {"state", serialize_state(pet_, mood, chat_log_, chat_pending_,
                                          shop_status_)}};
    }

    void handle_buy(const std::string& id) {
        for (const auto& item : shop_items()) {
            if (item.id == id) {
                shop_status_ = buy_item(pet_, item).message;
                return;
            }
        }
        shop_status_ = "Unknown item.";
    }

    void handle_chat(const std::string& text) {
        if (text.empty()) return;
        chat_log_.push_back({"You", text, false});
        if (brain_->enabled()) {
            std::vector<ChatMessage> messages;
            messages.push_back(
                {"system", build_system_prompt(snapshot(), config_.llm.personality)});
            const int start = std::max(0, static_cast<int>(chat_log_.size()) - 7);
            for (int i = start; i < static_cast<int>(chat_log_.size()) - 1; ++i) {
                messages.push_back(
                    {chat_log_[i].is_pet ? "assistant" : "user", chat_log_[i].text});
            }
            messages.push_back({"user", text});
            chat_pending_ = true;
            brain_->submit(std::move(messages), LlmKind::Chat);
        } else {
            chat_log_.push_back({pet_.name, fallback_reply(), true});
        }
    }

    void trigger_reaction(const std::string& event) {
        if (!brain_->enabled() || !config_.llm.react) return;
        std::vector<ChatMessage> messages;
        messages.push_back(
            {"system", build_system_prompt(snapshot(), config_.llm.personality)});
        messages.push_back(
            {"user", event + " React in one short sentence as the pet."});
        brain_->submit(std::move(messages), LlmKind::Reaction);
    }

    PetSnapshot snapshot() const {
        PetSnapshot s;
        s.name = pet_.name;
        s.stats = pet_.stats;
        s.sleeping_ticks = pet_.sleeping_ticks;
        s.working_ticks = pet_.working_ticks;
        s.frame_idx = pet_.frame_idx;
        s.prev_mood = pet_.prev_mood;
        s.coins = pet_.coins;
        return s;
    }

    std::string fallback_reply() const {
        const Mood mood = compute_mood(pet_.stats, pet_.sleeping_ticks,
                                       pet_.working_ticks, pet_.frame_idx,
                                       pet_.prev_mood);
        switch (mood) {
            case Mood::Happy:
            case Mood::Content:
            case Mood::Playing:
            case Mood::Hyper:
                return "Meow! *purrs happily* I'm having a great time! 💕";
            case Mood::Hungry:
            case Mood::Starving:
                return "Mew… my tummy is rumbling. Could I get a snack? 🥺";
            case Mood::Sleepy:
                return "*yawn* I'm so sleepy… maybe a quick nap? 💤";
            case Mood::Sad:
                return "Mew… I'm feeling a bit down. Play with me? 🥺";
            case Mood::Sick:
                return "Ugh, I don't feel so good… some medicine would help. 🤒";
            case Mood::Grumpy:
            case Mood::Bored:
                return "Hmph. I'm a little bored. Entertain me! 😾";
            case Mood::Working:
                return "Can't talk now — I'm busy earning coins! 💼";
            case Mood::Dead:
                return "…";
            default:
                return "Meow? *tilts head* 🐱";
        }
    }

    void restart() {
        delete_save();
        pet_ = Pet("Buddy");
        chat_log_.clear();
        chat_pending_ = false;
        pet_.events.add("🔄 A new pet has been born!", Color::Magenta);
    }

    Config config_;
    Pet pet_{"Buddy"};
    std::unique_ptr<Brain> brain_;
    std::vector<ChatLine> chat_log_;
    bool chat_pending_ = false;
    std::string shop_status_;
};

}  // namespace pet

int main() {
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGPIPE, SIG_IGN);  // survive a client disconnecting mid-send
    pet::Server server;
    return server.run();
}
