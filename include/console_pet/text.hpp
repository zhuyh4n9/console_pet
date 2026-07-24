#pragma once

// UTF-8 text helpers.
//
// ncursesw renders wide characters, so all on-screen strings (ASCII art, emoji,
// box-drawing glyphs) are converted from UTF-8 to wide strings before drawing.
// Keeping the conversion in one place means the rest of the codebase can store
// readable UTF-8 string literals.

#include <string>
#include <string_view>

namespace pet {

/// Decode a UTF-8 string into a wide string (one element per code point on
/// 32-bit wchar_t platforms; UTF-16 surrogate pairs on 16-bit platforms).
std::wstring to_wide(std::string_view utf8);

/// Encode a single wide code point back to UTF-8.
std::string to_utf8(wchar_t ch);

/// Number of Unicode code points in a UTF-8 string (independent of wchar_t
/// width). Used to reproduce Python's len() for layout/hit-region math.
std::size_t codepoint_count(std::string_view utf8);

/// Display width in terminal cells (sum of wcwidth; non-spacing and control
/// characters contribute 0). Matches how ncurses advances the cursor.
int display_width(std::string_view utf8);

/// Return the string padded with trailing spaces so its display width reaches
/// at least `target` cells. Used to align columns regardless of glyph width.
std::string pad_cells(std::string_view utf8, int target);

/// Remove the last UTF-8 code point from a string (for backspace handling).
void pop_codepoint(std::string& s);

}  // namespace pet
