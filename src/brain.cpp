#include "console_pet/brain.hpp"

#include <format>
#include <utility>

namespace pet {

std::string build_system_prompt(const PetSnapshot& s,
                                const std::string& personality) {
    const Mood mood = compute_mood(s.stats, s.sleeping_ticks, s.working_ticks,
                                   s.frame_idx, s.prev_mood);
    return std::format(
        "You are {}, a virtual pet cat living in a terminal game. Your "
        "personality: {}. Current state — mood: {}, hunger {}/100, happiness "
        "{}/100, energy {}/100, health {}/100, age {} ticks, {} coins. Speak in "
        "character as the pet in one short sentence, reacting to your current "
        "mood and needs. Be cute and pet-like.",
        s.name, personality, mood_display_name(mood), s.stats.hunger,
        s.stats.happiness, s.stats.energy, s.stats.health, s.stats.age, s.coins);
}

Brain::Brain(LlmConfig cfg) : cfg_(std::move(cfg)) {
    enabled_ = cfg_.enabled;
    // Background calls are short and best-effort; cap their timeout so shutting
    // down (which joins the worker) never waits long on a slow request.
    if (cfg_.timeout_sec > 15) cfg_.timeout_sec = 15;
    if (enabled_) {
        worker_ = std::thread(&Brain::worker_loop, this);
    }
}

Brain::~Brain() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void Brain::submit(std::vector<ChatMessage> messages, LlmKind kind) {
    if (!enabled_) return;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        tasks_.push_back(std::move(messages));
        task_kinds_.push_back(kind);
        // Keep the queue short so the pet reacts to recent events, not a backlog.
        while (tasks_.size() > 3) {
            tasks_.pop_front();
            task_kinds_.pop_front();
        }
    }
    cv_.notify_one();
}

bool Brain::poll(LlmResult& out) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (results_.empty()) return false;
    out = std::move(results_.front());
    results_.pop_front();
    return true;
}

void Brain::worker_loop() {
    while (true) {
        std::vector<ChatMessage> messages;
        LlmKind kind = LlmKind::Reaction;
        {
            std::unique_lock<std::mutex> lk(mtx_);
            cv_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
            // On shutdown, exit promptly and drop queued tasks; only the
            // currently in-flight request (if any) is allowed to finish.
            if (stop_) return;
            messages = std::move(tasks_.front());
            tasks_.pop_front();
            kind = task_kinds_.front();
            task_kinds_.pop_front();
        }

        const std::string reply = llm_chat(cfg_, messages);
        if (!reply.empty()) {
            std::lock_guard<std::mutex> lk(mtx_);
            results_.push_back({reply, kind});
        }
    }
}

}  // namespace pet
