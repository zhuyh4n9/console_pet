#include "console_pet/pet_stats.hpp"

#include <algorithm>

#include "console_pet/rng.hpp"

namespace pet {

void PetStats::clamp() {
    auto clamp_stat = [](int& v) { v = std::clamp(v, 0, 100); };
    clamp_stat(hunger);
    clamp_stat(happiness);
    clamp_stat(energy);
    clamp_stat(health);
}

void PetStats::decay() {
    hunger -= rng().rand_int(0, 1);
    happiness -= rng().rand_int(0, 1);
    energy -= rng().rand_int(0, 1);

    // Health drops when other stats are critically low; otherwise it slowly
    // recovers toward full.
    if (hunger < 20 || energy < 20 || happiness < 20) {
        health -= rng().rand_int(1, 2);
    } else if (health < 100) {
        health = std::min(100, health + 1);
    }

    age += 1;
    clamp();

    if (health <= 0 || hunger <= 0) {
        alive = false;
    }
}

}  // namespace pet
