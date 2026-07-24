#include "console_pet/mood.hpp"

namespace pet {

Mood compute_mood(const PetStats& s, int sleeping_ticks, int working_ticks,
                  int frame_idx, Mood prev) {
    // Stable jitter: drifts slowly within +/-4 so the face feels alive without
    // being random every frame.
    const int seed = s.hunger * 31 + s.happiness * 37 + s.energy * 41 +
                     s.health * 43 + s.age * 47 + frame_idx / 30;
    const int jit = (seed % 9) - 4;
    auto j = [jit](int v) { return v + jit; };

    if (!s.alive) return Mood::Dead;
    if (sleeping_ticks > 0) return Mood::Sleepy;
    if (working_ticks > 0) return Mood::Working;

    // Hysteresis: widen the "stay" band by 8 while already in a mood so stats
    // must overshoot the boundary to trigger a switch.
    auto below = [&](int threshold, Mood m) {  // mood active when stat < value
        return threshold + (prev == m ? 8 : 0);
    };
    auto above = [&](int threshold, Mood m) {  // mood active when stat > value
        return threshold - (prev == m ? 8 : 0);
    };

    // Negative moods, priority ordered.
    if (s.health < below(j(25), Mood::Sick)) return Mood::Sick;
    if (s.hunger < below(j(15), Mood::Starving)) return Mood::Starving;
    if (s.hunger < below(j(35), Mood::Hungry)) return Mood::Hungry;
    if (s.energy < below(j(20), Mood::Sleepy)) return Mood::Sleepy;  // low energy
    if (s.happiness < below(j(25), Mood::Sad)) return Mood::Sad;
    if (s.happiness < below(j(45), Mood::Grumpy)) return Mood::Grumpy;
    if (s.age < j(5)) return Mood::Egg;

    // Positive moods.
    if (s.energy > above(j(85), Mood::Hyper) && s.happiness > j(55))
        return Mood::Hyper;
    if (s.happiness > above(j(75), Mood::Playing) && s.energy > j(55))
        return Mood::Playing;
    if (s.hunger > above(j(90), Mood::Content) && s.happiness > j(55))
        return Mood::Content;
    if (s.happiness < below(j(55), Mood::Bored)) return Mood::Bored;
    return Mood::Happy;
}

const std::string& mood_display_name(Mood mood) {
    static const std::string names[] = {
        "HAPPY",   "CONTENT", "BORED",  "GRUMPY", "STARVING", "HUNGRY",
        "SAD",     "SLEEPY",  "PLAYING", "HYPER", "SICK",     "DEAD",
        "EGG",     "WORKING",
    };
    return names[static_cast<std::size_t>(mood)];
}

Color mood_color(Mood mood) {
    switch (mood) {
        case Mood::Dead:
        case Mood::Sick:
        case Mood::Starving:
            return Color::Red;
        case Mood::Hungry:
        case Mood::Grumpy:
        case Mood::Sad:
            return Color::Yellow;
        case Mood::Sleepy:
        case Mood::Egg:
            return Color::Cyan;
        case Mood::Working:
            return Color::Yellow;
        case Mood::Bored:
            return Color::Gray;
        case Mood::Content:
            return Color::GreenBold;
        case Mood::Playing:
            return Color::Magenta;
        case Mood::Hyper:
            return Color::MagentaBold;
        case Mood::Happy:
        default:
            return Color::Green;
    }
}

}  // namespace pet
