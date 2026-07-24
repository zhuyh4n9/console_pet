#include "console_pet/shop.hpp"

#include <algorithm>
#include <format>

namespace pet {

const std::vector<ShopItem>& shop_items() {
    static const std::vector<ShopItem> items = {
        // Foods: id, name, icon, price, is_decoration, hunger, happy, energy, health
        {"apple", "Apple", "🍎", 5, false, 15, 2, 0, 0},
        {"coffee", "Coffee", "☕", 8, false, 0, 3, 25, 0},
        {"salad", "Salad", "🥗", 10, false, 20, 2, 0, 8},
        {"fish", "Fish", "🐟", 15, false, 30, 5, 0, 3},
        {"cake", "Cake", "🍰", 20, false, 20, 15, 0, 0},
        {"sushi", "Sushi", "🍣", 30, false, 40, 10, 5, 5},
        // Decorations: cosmetic items the pet can wear.
        {"bow", "Bow", "🎀", 40, true},
        {"scarf", "Scarf", "🧣", 50, true},
        {"hat", "Hat", "🎩", 60, true},
        {"glasses", "Glasses", "🕶️", 70, true},
        {"crown", "Crown", "👑", 100, true},
    };
    return items;
}

std::string decoration_icon(const std::string& id) {
    for (const auto& item : shop_items()) {
        if (item.is_decoration && item.id == id) return item.icon;
    }
    return "";
}

std::string food_effect_text(const ShopItem& item) {
    std::string s;
    auto add = [&](int v, const char* icon) {
        if (v == 0) return;
        if (!s.empty()) s += ' ';
        s += std::format("+{}{}", v, icon);
    };
    add(item.hunger, "🍔");
    add(item.happiness, "😊");
    add(item.energy, "🔋");
    add(item.health, "💗");
    return s;
}

BuyResult buy_item(Pet& pet, const ShopItem& item) {
    if (item.is_decoration) {
        // Owned decorations can be equipped or removed for free.
        if (pet.owns_decoration(item.id)) {
            if (pet.is_wearing(item.id)) {
                pet.wear_decoration("");
                pet.events.add(
                    std::format("{} took off the {}.", pet.name, item.name),
                    Color::Cyan);
                return {true, std::format("Took off the {}.", item.name)};
            }
            pet.wear_decoration(item.id);
            pet.events.add(std::format("{} {} put on the {}!", item.icon,
                                       pet.name, item.name),
                           Color::Cyan);
            return {true,
                    std::format("Now wearing the {} {}.", item.icon, item.name)};
        }
        if (pet.coins < item.price) {
            return {false, std::format("Not enough coins for the {} (need {}💰).",
                                       item.name, item.price)};
        }
        pet.coins -= item.price;
        pet.add_decoration(item.id);
        pet.wear_decoration(item.id);
        // A new toy cheers the pet up a little.
        pet.stats.happiness = std::min(100, pet.stats.happiness + 5);
        pet.events.add(std::format("{} Bought the {}! (-{}💰)", item.icon,
                                   item.name, item.price),
                       Color::Green);
        return {true, std::format("Bought the {} {}! (-{}💰)", item.icon,
                                  item.name, item.price)};
    }

    // Food is eaten immediately.
    if (!pet.stats.alive) {
        return {false, std::format("{} has passed away...", pet.name)};
    }
    if (pet.coins < item.price) {
        return {false, std::format("Not enough coins for the {} (need {}💰).",
                                   item.name, item.price)};
    }
    pet.coins -= item.price;
    pet.apply_food(item.name, item.icon, item.hunger, item.happiness,
                   item.energy, item.health);
    return {true,
            std::format("Bought {} {}! (-{}💰)", item.icon, item.name, item.price)};
}

}  // namespace pet
