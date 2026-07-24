#pragma once

// Curses rendering.
//
// Everything that touches ncurses lives here. draw_all() renders the whole
// screen and reports the clickable regions so the client can do mouse hit
// detection without knowing pixel/cell layout details. The mood is computed by
// the server and passed in, so rendering is a pure function of the state.

#include <string>
#include <vector>

#include <ncurses.h>

#include "console_pet/mood.hpp"
#include "console_pet/pet.hpp"
#include "console_pet/proto.hpp"

namespace pet {

/// Clickable command button region (inclusive cell bounds) and its hotkey.
struct ButtonRegion {
    int y1, x1, y2, x2;
    char key;
};

/// Screen layout produced by draw_all(), used for mouse hit detection.
struct Layout {
    bool valid = false;  // false when the terminal is too small
    int pet_y1 = 0, pet_x1 = 0, pet_y2 = 0, pet_x2 = 0;
    std::vector<ButtonRegion> buttons;
};

/// Initialize curses color pairs. Call once after initscr/start_color.
void init_colors();

/// Render the full screen for the pet and return clickable regions.
Layout draw_all(WINDOW* scr, const Pet& pet, Mood mood, int highlight_btn = -1);

/// Render the shop screen (item list, coins, status message, help line).
void draw_shop(WINDOW* scr, const Pet& pet, int selected,
               const std::string& status);

/// Render the chat screen (transcript, input line, help line).
void draw_chat(WINDOW* scr, const Pet& pet, Mood mood,
               const std::vector<ChatLine>& log, const std::string& input,
               bool thinking);

}  // namespace pet
