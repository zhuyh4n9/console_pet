#pragma once

// ASCII art frames for each mood.
//
// Art is stored as readable raw-string literals and split into non-empty lines
// once on first use. frame_lines() picks the animation frame for a mood.

#include <string>
#include <vector>

#include "console_pet/mood.hpp"

namespace pet {

/// All animation frames for a mood; each frame is a list of screen lines.
const std::vector<std::vector<std::string>>& frames_for(Mood mood);

/// Lines of the active frame for a mood (frame_idx wraps around).
const std::vector<std::string>& frame_lines(Mood mood, int frame_idx);

}  // namespace pet
