#pragma once

// Pet: the pet's state plus the actions the player can perform on it.
//
// Owns PetStats, an EventLog, and a few bookkeeping fields. Pure game logic
// with no UI or I/O dependencies, so it is fully testable headlessly.

#include <string>
#include <vector>

#include "console_pet/event_log.hpp"
#include "console_pet/mood.hpp"
#include "console_pet/pet_stats.hpp"

namespace pet {

struct Pet {
    std::string name;
    PetStats stats;
    EventLog events;
    int frame_idx = 0;        // animation counter
    int sleeping_ticks = 0;   // remaining ticks of sleep animation
    int working_ticks = 0;    // remaining ticks until the current job pays out
    int snacks = 0;           // treat inventory
    int coins = 0;            // currency earned by working, spent in the shop
    std::vector<std::string> owned_decorations;  // decoration ids bought
    std::string worn_decoration;                 // id currently worn ("" = none)
    std::string speech;         // current line shown in the speech bubble
    int speech_ticks = 0;       // ticks remaining before the bubble clears
    Mood prev_mood = Mood::Egg;  // last mood, for hysteresis

    explicit Pet(std::string name = "Buddy", PetStats stats = {});

    void feed();
    void play();
    void sleep();
    void heal();
    void add_snack();

    /// Send the pet to work: spends energy/hunger to earn coins.
    void work();

    /// Make the pet say something in its speech bubble for `ticks` ticks.
    void say(const std::string& text, int ticks = 16);

    /// Apply a food's stat effects (clamped) and log it. Used by the shop.
    void apply_food(const std::string& name, const std::string& icon, int hunger,
                    int happiness, int energy, int health);

    // Decoration helpers (used by the shop).
    bool owns_decoration(const std::string& id) const;
    bool is_wearing(const std::string& id) const;
    void add_decoration(const std::string& id);
    void wear_decoration(const std::string& id);  // "" clears the worn item

    /// Advance one game tick (decay, auto-eat, auto-sleep, sleep recovery).
    void tick();

private:
    void wake_if_sleeping();
    void finish_work();  // pay out coins and apply the cost of a completed job
};

}  // namespace pet
