#pragma once

// Logical color identifiers used across the UI and event log.
//
// The underlying integer values double as ncurses color-pair IDs (1..13), which
// keeps the mapping to curses trivial in ui.cpp. This header itself has no
// dependency on ncurses so it can be included by pure-logic modules (EventLog).

namespace pet {

enum class Color : short {
    White = 1,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    Gray,        // bright black
    RedBold,
    GreenBold,
    YellowBold,
    CyanBold,
    MagentaBold,
};

}  // namespace pet
