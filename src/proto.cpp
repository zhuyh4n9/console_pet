#include "console_pet/proto.hpp"

#include "console_pet/colors.hpp"

namespace pet {

nlohmann::json serialize_state(const Pet& pet, Mood mood,
                               const std::vector<ChatLine>& chat,
                               bool chat_pending,
                               const std::string& shop_status) {
    nlohmann::json events = nlohmann::json::array();
    for (const auto& [text, color] : pet.events.messages()) {
        events.push_back({{"text", text}, {"color", static_cast<int>(color)}});
    }
    nlohmann::json chatj = nlohmann::json::array();
    for (const auto& c : chat) {
        chatj.push_back({{"who", c.who}, {"text", c.text}, {"is_pet", c.is_pet}});
    }
    return {
        {"name", pet.name},
        {"stats",
         {
             {"hunger", pet.stats.hunger},
             {"happiness", pet.stats.happiness},
             {"energy", pet.stats.energy},
             {"health", pet.stats.health},
             {"age", pet.stats.age},
             {"alive", pet.stats.alive},
         }},
        {"frame_idx", pet.frame_idx},
        {"sleeping_ticks", pet.sleeping_ticks},
        {"working_ticks", pet.working_ticks},
        {"snacks", pet.snacks},
        {"coins", pet.coins},
        {"owned_decorations", pet.owned_decorations},
        {"worn_decoration", pet.worn_decoration},
        {"speech", pet.speech},
        {"speech_ticks", pet.speech_ticks},
        {"prev_mood", static_cast<int>(pet.prev_mood)},
        {"mood", static_cast<int>(mood)},
        {"events", events},
        {"chat", chatj},
        {"chat_pending", chat_pending},
        {"shop_status", shop_status},
    };
}

RemoteState parse_state(const nlohmann::json& j) {
    RemoteState rs;
    rs.pet.name = j.value("name", std::string("Buddy"));

    if (const auto s = j.find("stats"); s != j.end() && s->is_object()) {
        rs.pet.stats.hunger = s->value("hunger", 80);
        rs.pet.stats.happiness = s->value("happiness", 80);
        rs.pet.stats.energy = s->value("energy", 80);
        rs.pet.stats.health = s->value("health", 80);
        rs.pet.stats.age = s->value("age", 0);
        rs.pet.stats.alive = s->value("alive", true);
    }
    rs.pet.frame_idx = j.value("frame_idx", 0);
    rs.pet.sleeping_ticks = j.value("sleeping_ticks", 0);
    rs.pet.working_ticks = j.value("working_ticks", 0);
    rs.pet.snacks = j.value("snacks", 0);
    rs.pet.coins = j.value("coins", 0);
    rs.pet.owned_decorations =
        j.value("owned_decorations", std::vector<std::string>{});
    rs.pet.worn_decoration = j.value("worn_decoration", std::string{});
    rs.pet.speech = j.value("speech", std::string{});
    rs.pet.speech_ticks = j.value("speech_ticks", 0);
    rs.pet.prev_mood = static_cast<Mood>(j.value("prev_mood", 0));
    rs.mood = static_cast<Mood>(j.value("mood", 0));

    if (const auto ev = j.find("events"); ev != j.end() && ev->is_array()) {
        for (const auto& e : *ev) {
            rs.pet.events.add(e.value("text", std::string{}),
                              static_cast<Color>(e.value("color", 1)));
        }
    }
    if (const auto ch = j.find("chat"); ch != j.end() && ch->is_array()) {
        for (const auto& c : *ch) {
            rs.chat.push_back({c.value("who", std::string{}),
                               c.value("text", std::string{}),
                               c.value("is_pet", false)});
        }
    }
    rs.chat_pending = j.value("chat_pending", false);
    rs.shop_status = j.value("shop_status", std::string{});
    return rs;
}

}  // namespace pet
