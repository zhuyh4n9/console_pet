#pragma once

// Mood determination.
//
// Translates the pet's raw stats into a discrete Mood used to pick ASCII art
// and color. Uses hysteresis so the mood does not flicker at thresholds: once
// in a mood, stats must overshoot the boundary by ~8 points to leave it.

#include <string>

#include "console_pet/colors.hpp"
#include "console_pet/pet_stats.hpp"

namespace pet {

enum class Mood {
    Happy,
    Content,
    Bored,
    Grumpy,
    Starving,
    Hungry,
    Sad,
    Sleepy,
    Playing,
    Hyper,
    Sick,
    Dead,
    Egg,
    Working,
};

/// Compute the mood for the given state. `prev` is the mood from the last
/// frame, used for hysteresis.
Mood compute_mood(const PetStats& stats, int sleeping_ticks, int working_ticks,
                  int frame_idx, Mood prev);

/// Upper-case label for on-screen display (e.g. "HAPPY").
const std::string& mood_display_name(Mood mood);

/// Color associated with a mood for rendering the art and label.
Color mood_color(Mood mood);

}  // namespace pet
