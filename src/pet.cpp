#include "console_pet/pet.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "console_pet/rng.hpp"

namespace pet {

Pet::Pet(std::string name, PetStats stats)
    : name(std::move(name)), stats(stats) {}

namespace {
/// Log the "passed away" notice and report whether the pet is dead, so action
/// methods can early-out without repeating the guard.
bool report_if_dead(Pet& pet) {
    if (!pet.stats.alive) {
        pet.events.add(std::format("{} has passed away... 💀", pet.name),
                       Color::Red);
        return true;
    }
    return false;
}

/// Log a "busy working" notice and report whether the pet is mid-job, so action
/// methods can early-out while the pet is away working.
bool report_if_working(Pet& pet) {
    if (pet.working_ticks > 0) {
        pet.events.add(std::format("💼 {} is busy working...", pet.name),
                       Color::Yellow);
        return true;
    }
    return false;
}
}  // namespace

void Pet::wake_if_sleeping() {
    if (sleeping_ticks > 0) {
        sleeping_ticks = 0;
        events.add(std::format("⏰ {} was woken up!", name), Color::Yellow);
    }
}

void Pet::feed() {
    if (report_if_dead(*this)) return;
    if (report_if_working(*this)) return;
    wake_if_sleeping();

    const int old_hunger = stats.hunger;
    const int amount = rng().rand_int(15, 30);
    stats.hunger = std::min(100, stats.hunger + amount);
    stats.happiness = std::min(100, stats.happiness + 5);

    // Overfeeding: the fuller the pet, the higher the risk of a stomach ache
    // (0% at hunger 70, ~85% at 100).
    if (old_hunger > 70) {
        const double overfeed_chance = (old_hunger - 70) / 30.0 * 0.85;
        if (rng().rand_real() < overfeed_chance) {
            const int dmg = rng().rand_int(3, 10);
            stats.health = std::max(0, stats.health - dmg);
            events.add(std::format("🍽️  You fed {}! (+{} hunger) — but they ate "
                                   "too much! (-{} health 🤢)",
                                   name, amount, dmg),
                       Color::Yellow);
            return;
        }
    }

    events.add(std::format("🍽️  You fed {}! (+{} hunger)", name, amount),
               Color::Green);
}

void Pet::play() {
    if (report_if_dead(*this)) return;
    if (report_if_working(*this)) return;
    wake_if_sleeping();

    if (stats.energy < 15) {
        events.add(std::format("😴 {} is too tired to play...", name),
                   Color::Yellow);
        return;
    }

    const int amount = rng().rand_int(15, 25);
    stats.happiness = std::min(100, stats.happiness + amount);
    stats.energy -= rng().rand_int(8, 15);
    stats.hunger -= rng().rand_int(5, 10);
    stats.clamp();
    events.add(std::format("🎾 You played with {}! (+{} happiness)", name, amount),
               Color::Magenta);
}

void Pet::sleep() {
    if (report_if_dead(*this)) return;
    if (report_if_working(*this)) return;
    if (sleeping_ticks > 0) {
        events.add(std::format("😴 {} is already sleeping...", name),
                   Color::Yellow);
        return;
    }
    sleeping_ticks = rng().rand_int(8, 14);  // recover over ~8-14 ticks
    events.add(std::format("💤 {} went to sleep...", name), Color::Cyan);
}

void Pet::heal() {
    if (report_if_dead(*this)) return;
    if (report_if_working(*this)) return;
    wake_if_sleeping();

    const int amount = rng().rand_int(20, 35);
    stats.health = std::min(100, stats.health + amount);
    events.add(std::format("💊 You gave medicine to {}! (+{} health)", name, amount),
               Color::Green);
}

void Pet::add_snack() {
    if (report_if_dead(*this)) return;
    if (report_if_working(*this)) return;
    snacks += 1;
    events.add(std::format("🍪 You stocked a snack! ({} total)", snacks),
               Color::Yellow);
}

void Pet::work() {
    if (report_if_dead(*this)) return;
    if (working_ticks > 0) {
        events.add(std::format("💼 {} is already working...", name), Color::Yellow);
        return;
    }
    wake_if_sleeping();
    if (stats.energy < 20) {
        events.add(std::format("😴 {} is too tired to work...", name),
                   Color::Yellow);
        return;
    }
    // The job takes a random amount of time; it pays out when it finishes (see
    // finish_work(), called from tick()).
    working_ticks = rng().rand_int(4, 8);
    events.add(std::format("💼 {} went to work... (back in a bit)", name),
               Color::Cyan);
}

void Pet::finish_work() {
    // A happier pet is more productive.
    int earned = rng().rand_int(8, 15);
    if (stats.happiness > 70) earned += rng().rand_int(2, 5);
    coins += earned;

    // Work is tiring, makes the pet hungry, and is a bit of a drag.
    stats.energy -= rng().rand_int(12, 20);
    stats.hunger -= rng().rand_int(3, 6);
    stats.happiness -= rng().rand_int(2, 5);
    stats.clamp();

    events.add(std::format("💼 {} finished work! Earned {} coins! (💰 {} total)",
                           name, earned, coins),
               Color::Green);
}

void Pet::apply_food(const std::string& food_name, const std::string& icon,
                     int hunger, int happiness, int energy, int health) {
    stats.hunger = std::min(100, stats.hunger + hunger);
    stats.happiness = std::min(100, stats.happiness + happiness);
    stats.energy = std::min(100, stats.energy + energy);
    stats.health = std::min(100, stats.health + health);
    stats.clamp();
    events.add(std::format("{} {} ate the {}! Yum!", icon, name, food_name),
               Color::Green);
}

bool Pet::owns_decoration(const std::string& id) const {
    return std::find(owned_decorations.begin(), owned_decorations.end(), id) !=
           owned_decorations.end();
}

bool Pet::is_wearing(const std::string& id) const {
    return !id.empty() && worn_decoration == id;
}

void Pet::add_decoration(const std::string& id) {
    if (!owns_decoration(id)) owned_decorations.push_back(id);
}

void Pet::wear_decoration(const std::string& id) { worn_decoration = id; }

void Pet::say(const std::string& text, int ticks) {
    speech = text;
    speech_ticks = ticks;
}

void Pet::tick() {
    bool busy = false;

    if (sleeping_ticks > 0) {
        // Currently sleeping — recover energy and a little health.
        stats.energy = std::min(100, stats.energy + rng().rand_int(2, 4));
        stats.health = std::min(100, stats.health + 1);
        sleeping_ticks -= 1;
        busy = true;
        if (sleeping_ticks == 0) {
            events.add(std::format("⏰ {} woke up!", name), Color::Cyan);
        }
    } else if (working_ticks > 0) {
        // Currently working — count down and pay out when the job is done.
        working_ticks -= 1;
        if (working_ticks == 0) {
            finish_work();
        }
        busy = true;
    }

    if (!busy) {
        stats.decay();

        // Auto-eat snacks when hungry (probability scales with hunger).
        if (snacks > 0 && stats.hunger < 80) {
            // Hungrier -> more likely: ~5% at hunger 79, ~95% at hunger 0.
            const double chance = (80 - stats.hunger) / 80.0 * 0.9 + 0.05;
            if (rng().rand_real() < chance) {
                snacks -= 1;
                stats.hunger = std::min(100, stats.hunger + rng().rand_int(5, 12));
                stats.happiness =
                    std::min(100, stats.happiness + rng().rand_int(1, 3));
                std::string msg =
                    std::format("🍪 {} grabbed a snack! ({} left)", name, snacks);
                // Low chance of a side effect (junk food is not always healthy).
                if (rng().rand_real() < 0.2) {
                    if (rng().rand_real() < 0.5) {
                        stats.energy =
                            std::max(0, stats.energy - rng().rand_int(3, 8));
                        msg += " (sugar crash 💫)";
                    } else {
                        stats.health =
                            std::max(0, stats.health - rng().rand_int(3, 6));
                        msg += " (stomach ache 🤢)";
                    }
                }
                events.add(msg, Color::Yellow);
            }
        }

        // Fall asleep automatically when exhausted.
        if (stats.energy <= 15 && stats.alive) {
            sleeping_ticks = rng().rand_int(8, 14);
            events.add(std::format("😴 {} fell asleep on their own...", name),
                       Color::Cyan);
        }
    }

    frame_idx += 1;

    // Age the speech bubble.
    if (speech_ticks > 0) {
        speech_ticks -= 1;
        if (speech_ticks == 0) speech.clear();
    }

    if (!stats.alive) {
        events.add(std::format("💀 {} has died... Press R to restart.", name),
                   Color::Red);
    }
}

}  // namespace pet
