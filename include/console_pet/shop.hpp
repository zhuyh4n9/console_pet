#pragma once

// Shop: the catalog of purchasable items (foods and decorations) and the logic
// for buying/equipping them. Pure game logic — no UI or I/O.

#include <string>
#include <vector>

#include "console_pet/pet.hpp"

namespace pet {

struct ShopItem {
    std::string id;
    std::string name;
    std::string icon;
    int price;
    bool is_decoration;
    // Food effects (zero for decorations).
    int hunger = 0;
    int happiness = 0;
    int energy = 0;
    int health = 0;
};

/// The full shop catalog (foods first, then decorations).
const std::vector<ShopItem>& shop_items();

/// Outcome of a buy/equip attempt.
struct BuyResult {
    bool ok = false;
    std::string message;
};

/// Buy an item for the pet (or toggle wearing an owned decoration). Updates the
/// pet and logs an event; the returned message is for immediate shop feedback.
BuyResult buy_item(Pet& pet, const ShopItem& item);

/// Icon for a decoration id (for showing the worn item); "" if unknown.
std::string decoration_icon(const std::string& id);

/// Short effect summary for a food (e.g. "+15🍔 +2😊"); "" for decorations.
std::string food_effect_text(const ShopItem& item);

}  // namespace pet
