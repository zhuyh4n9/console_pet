#pragma once

// PetStats: the raw numerical state of the pet.
//
// Pure data + behaviour, no UI or I/O dependencies, so it can be unit tested
// without a terminal.

namespace pet {

struct PetStats {
    int hunger = 80;      // 0 = starving, 100 = full
    int happiness = 80;   // 0 = sad, 100 = ecstatic
    int energy = 80;      // 0 = exhausted, 100 = energetic
    int health = 80;      // 0 = sick, 100 = healthy
    int age = 0;          // ticks alive
    bool alive = true;

    /// Clamp the four 0..100 stats into range.
    void clamp();

    /// Apply one tick of natural stat decay (gentle pace). May kill the pet.
    void decay();
};

}  // namespace pet
