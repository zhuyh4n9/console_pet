#include "console_pet/save.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace pet {

namespace {

nlohmann::json to_json(const Pet& pet) {
    return nlohmann::json{
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
        {"sleeping_ticks", pet.sleeping_ticks},
        {"working_ticks", pet.working_ticks},
        {"snacks", pet.snacks},
        {"coins", pet.coins},
        {"owned_decorations", pet.owned_decorations},
        {"worn_decoration", pet.worn_decoration},
    };
}

Pet from_json(const nlohmann::json& j) {
    const auto& s = j.at("stats");
    PetStats stats;
    stats.hunger = s.at("hunger").get<int>();
    stats.happiness = s.at("happiness").get<int>();
    stats.energy = s.at("energy").get<int>();
    stats.health = s.at("health").get<int>();
    stats.age = s.at("age").get<int>();
    stats.alive = s.at("alive").get<bool>();

    Pet pet(j.at("name").get<std::string>(), stats);
    pet.sleeping_ticks = j.value("sleeping_ticks", 0);
    pet.working_ticks = j.value("working_ticks", 0);
    pet.snacks = j.value("snacks", 0);
    // Economy fields default gracefully for saves created before they existed.
    pet.coins = j.value("coins", 0);
    pet.owned_decorations =
        j.value("owned_decorations", std::vector<std::string>{});
    pet.worn_decoration = j.value("worn_decoration", std::string{});
    return pet;
}

}  // namespace

void save_pet(const Pet& pet, const std::string& path) {
    std::ofstream out(path);
    if (!out) return;
    out << to_json(pet).dump(2) << '\n';
}

std::optional<Pet> load_pet(const std::string& path) {
    std::ifstream in(path);
    if (!in) return std::nullopt;
    try {
        return from_json(nlohmann::json::parse(in));
    } catch (const nlohmann::json::exception&) {
        return std::nullopt;
    }
}

void delete_save(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

}  // namespace pet
