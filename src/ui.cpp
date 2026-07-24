#include "console_pet/ui.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>

#include "console_pet/mood.hpp"
#include "console_pet/pet_art.hpp"
#include "console_pet/shop.hpp"
#include "console_pet/text.hpp"

namespace pet {
namespace {

// Fixed vertical extents so the layout never shifts as the mood (art height)
// or the number of logged events changes.
constexpr int kPetArtHeight = 8;  // 1 headgear line + 7 lines of cat art
constexpr int kEventRows = 3;     // EventLog capacity

// Minimum terminal size that fits the fixed layout (the command buttons are
// the widest element; the last used row is the second button row).
constexpr int kMinWidth = 52;
constexpr int kMinHeight = 27;

attr_t color_attr(Color c) { return COLOR_PAIR(static_cast<int>(c)); }

/// Draw a UTF-8 string at (y, x) with the given attribute.
void put(WINDOW* w, int y, int x, const std::string& s, attr_t attr) {
    wattron(w, attr);
    mvwaddwstr(w, y, x, to_wide(s).c_str());
    wattroff(w, attr);
}

/// Continue drawing at the current cursor position.
void put_cont(WINDOW* w, const std::string& s, attr_t attr) {
    wattron(w, attr);
    waddwstr(w, to_wide(s).c_str());
    wattroff(w, attr);
}

/// Total display width of every line in the Pet Status box, so the borders and
/// rows all line up. Content = "│ " (2) + icon/label (14) + bar (25) + " │" (2).
constexpr int kBoxWidth = 43;

/// Top border with an embedded title, padded to kBoxWidth cells.
std::string box_top(const std::string& title) {
    std::string s = "╭─── " + title + " ";
    for (int w = display_width(s); w < kBoxWidth - 1; ++w) s += "─";
    s += "╮";
    return s;
}

/// Bottom border, padded to kBoxWidth cells.
std::string box_bottom() {
    std::string s = "╰";
    for (int i = 1; i < kBoxWidth - 1; ++i) s += "─";
    s += "╯";
    return s;
}

/// A box body row: "│ " + content padded to (kBoxWidth - 4) cells + " │".
std::string box_row(const std::string& content) {
    return "│ " + pad_cells(content, kBoxWidth - 4) + " │";
}

/// Bar string + color for a 0..100 stat value.
std::pair<std::string, Color> bar_text(int value, int width = 20) {
    const int filled = static_cast<int>(value / 100.0 * width);
    std::string block;
    for (int i = 0; i < width; ++i) block += (i < filled ? "█" : "░");
    std::string label = std::format("{} {:3d}%", block, value);
    if (value > 60) return {label, Color::Green};
    if (value > 30) return {label, Color::Yellow};
    return {label, Color::Red};
}

int draw_header(WINDOW* w, int y) {
    const attr_t cyan = color_attr(Color::Cyan) | A_BOLD;
    put(w, y, 2, "╔══════════════════════════════════════╗", cyan);
    put(w, y + 1, 2, "║        ", cyan);
    put_cont(w, "🐾 Console Pet 🐾", A_BOLD);
    put_cont(w, "           ║", cyan);
    put(w, y + 2, 2, "╚══════════════════════════════════════╝", cyan);
    return 3;
}

/// Overlay the worn decoration onto the pet art. Positions are relative to the
/// art-area top-left (y, x): line 0 is the headgear row above the ears, and the
/// cat itself occupies lines 1..7.
void draw_decoration(WINDOW* w, const std::string& deco_id, Mood mood, int y,
                     int x) {
    if (deco_id.empty() || mood == Mood::Egg) return;  // eggs don't dress up
    const std::string icon = decoration_icon(deco_id);
    if (icon.empty()) return;

    int line = -1, col = -1;
    if (deco_id == "hat" || deco_id == "crown") {
        line = 0; col = 5;  // centered above the head
    } else if (deco_id == "bow") {
        line = 1; col = 10;  // by the right ear
    } else if (deco_id == "glasses") {
        line = 2; col = 4;  // over the eyes
    } else if (deco_id == "scarf") {
        line = 4; col = 4;  // around the neck
    }
    if (line < 0) return;
    put(w, y + line, x + col, icon, A_BOLD);
}

int draw_pet(WINDOW* w, const Pet& pet, Mood mood, int y, int x) {
    attr_t attr = color_attr(mood_color(mood));
    if (mood != Mood::Dead && mood != Mood::Bored) attr |= A_BOLD;

    // The cat art (up to 7 lines) is drawn below the reserved headgear line so
    // a hat/crown can sit on top without shifting the pet.
    const int art_top = y + 1;
    const auto& lines = frame_lines(mood, pet.frame_idx);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        put(w, art_top + static_cast<int>(i), x, lines[i], attr);
    }
    draw_decoration(w, pet.worn_decoration, mood, y, x);
    return kPetArtHeight;
}

int draw_stats(WINDOW* w, const PetStats& stats, int y, int x, int snacks) {
    const attr_t cyan = color_attr(Color::CyanBold) | A_BOLD;
    const attr_t cyan_dim = color_attr(Color::Cyan);

    put(w, y, x, box_top("Pet Status"), cyan);
    y += 1;

    // Icons are all unambiguous double-width glyphs so every row aligns; the
    // icon field is padded to 2 cells and the label is a fixed 11 chars.
    struct Row {
        const char* icon;
        const char* label;
        int val;
    };
    const Row rows[] = {
        {"🍔", "Hunger:    ", stats.hunger},
        {"😊", "Happy:     ", stats.happiness},
        {"🔋", "Energy:    ", stats.energy},
        {"💗", "Health:    ", stats.health},
    };
    for (int i = 0; i < 4; ++i) {
        put(w, y + i, x, "│ ", cyan_dim);
        put_cont(w, pad_cells(rows[i].icon, 2) + " " + rows[i].label, A_NORMAL);
        const auto [bar_s, bar_cp] = bar_text(rows[i].val);
        put_cont(w, bar_s, color_attr(bar_cp));
        put_cont(w, " │", cyan_dim);
    }

    put(w, y + 4, x, box_row(std::format("🎂 Age: {:>10} ticks", stats.age)),
        cyan_dim);
    put(w, y + 5, x, box_row(std::format("🍪 Snacks: {:>8}", snacks)), cyan_dim);
    put(w, y + 6, x, box_bottom(), cyan_dim);
    return 8;  // top border + 4 stats + age + snacks + bottom border
}

int draw_event_log(WINDOW* w, const EventLog& log, int y, int x) {
    const attr_t gray = color_attr(Color::Gray);
    put(w, y, x, "│ ", gray);
    const auto& msgs = log.messages();
    if (msgs.empty()) {
        put_cont(w, "(no events yet)", gray);
        return 1;
    }
    int i = 0;
    for (const auto& [msg, color] : msgs) {
        put(w, y + i, x, "│ ", gray);
        put_cont(w, msg, color_attr(color));
        ++i;
    }
    return static_cast<int>(msgs.size());
}

int draw_commands(WINDOW* w, int y, int x, int highlight,
                  std::vector<ButtonRegion>& regions) {
    struct Cmd {
        const char* label;
        char key;
    };
    // Two rows of buttons; the highlight index is global across both rows
    // (row 1 first, then row 2), matching the order regions are appended.
    static const Cmd row1[] = {
        {"🍔 Feed", 'f'}, {"🍪 Snack", 't'}, {"🎾 Play", 'p'},
        {"💤 Sleep", 's'}, {"💊 Heal", 'h'},
    };
    static const Cmd row2[] = {
        {"💼 Work", 'w'}, {"🛒 Shop", 'b'}, {"💬 Chat", 'c'},
        {"🔄 Restart", 'r'}, {"🚪 Quit", 'q'},
    };

    regions.clear();
    auto draw_row = [&](const Cmd* btns, std::size_t n, int by, int base_idx) {
        int bx = x;
        for (std::size_t i = 0; i < n; ++i) {
            const int idx = base_idx + static_cast<int>(i);
            const attr_t attr =
                (idx == highlight) ? (A_REVERSE | A_BOLD) : A_BOLD;
            const std::string btn_text = std::format(" {} ", btns[i].label);
            put(w, by, bx, btn_text, attr);
            // Use the real cursor column (in cells) so the clickable region
            // matches the rendered button (emoji occupy two cells).
            const int end_x = getcurx(w);
            regions.push_back({by, bx, by, end_x - 1, btns[i].key});
            bx = end_x;
        }
    };

    draw_row(row1, std::size(row1), y, 0);
    draw_row(row2, std::size(row2), y + 1, static_cast<int>(std::size(row1)));
    return 2;
}

/// Wrap text to a maximum display width, breaking at code-point boundaries.
std::vector<std::string> wrap_text(const std::string& text, int max_width) {
    std::vector<std::string> lines;
    std::string cur;
    int cur_w = 0;
    for (wchar_t ch : to_wide(text)) {
        if (ch == L'\n') {
            lines.push_back(cur);
            cur.clear();
            cur_w = 0;
            continue;
        }
        const std::string s = to_utf8(ch);
        const int cw = display_width(s);
        if (cur_w + cw > max_width && !cur.empty()) {
            lines.push_back(cur);
            cur.clear();
            cur_w = 0;
        }
        cur += s;
        cur_w += cw;
    }
    lines.push_back(cur);
    return lines;
}

/// Draw the pet's speech bubble (a rounded box of wrapped text), sized to fit
/// its content up to `max_width` cells.
void draw_speech(WINDOW* w, const std::string& text, int y, int x, int max_width) {
    if (text.empty() || max_width < 10) return;
    const attr_t border = color_attr(Color::Cyan);
    const attr_t body = color_attr(Color::White) | A_BOLD;

    const int max_text_w = max_width - 4;  // side borders + one space each side
    auto lines = wrap_text(text, max_text_w);
    if (lines.size() > 4) lines.resize(4);  // keep the bubble compact

    // Size the box to the longest line so short messages get a small bubble.
    int text_w = 0;
    for (const auto& l : lines) text_w = std::max(text_w, display_width(l));
    text_w = std::max(text_w, 1);
    const int inner = text_w + 2;  // + one space of padding each side

    std::string hline;
    for (int i = 0; i < inner; ++i) hline += "─";

    put(w, y, x, "╭" + hline + "╮", border);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        put(w, y + 1 + static_cast<int>(i), x, "│ ", border);
        put_cont(w, pad_cells(lines[i], text_w), body);
        put_cont(w, " │", border);
    }
    put(w, y + 1 + static_cast<int>(lines.size()), x, "╰" + hline + "╯", border);
}

}  // namespace

void init_colors() {
    start_color();
    use_default_colors();
    init_pair(static_cast<short>(Color::White), COLOR_WHITE, -1);
    init_pair(static_cast<short>(Color::Red), COLOR_RED, -1);
    init_pair(static_cast<short>(Color::Green), COLOR_GREEN, -1);
    init_pair(static_cast<short>(Color::Yellow), COLOR_YELLOW, -1);
    init_pair(static_cast<short>(Color::Blue), COLOR_BLUE, -1);
    init_pair(static_cast<short>(Color::Magenta), COLOR_MAGENTA, -1);
    init_pair(static_cast<short>(Color::Cyan), COLOR_CYAN, -1);
    init_pair(static_cast<short>(Color::Gray), 8, -1);  // bright black
    init_pair(static_cast<short>(Color::RedBold), COLOR_RED, -1);
    init_pair(static_cast<short>(Color::GreenBold), COLOR_GREEN, -1);
    init_pair(static_cast<short>(Color::YellowBold), COLOR_YELLOW, -1);
    init_pair(static_cast<short>(Color::CyanBold), COLOR_CYAN, -1);
    init_pair(static_cast<short>(Color::MagentaBold), COLOR_MAGENTA, -1);
}

Layout draw_all(WINDOW* scr, const Pet& pet, Mood mood, int highlight_btn) {
    Layout layout;
    werase(scr);

    int h = 0, w = 0;
    getmaxyx(scr, h, w);
    const int cx = 2;
    int y = 0;

    if (h < kMinHeight || w < kMinWidth) {
        put(scr, 0, 0,
            std::format("Terminal too small ({}x{}). Need {}x{}+.", w, h,
                        kMinWidth, kMinHeight),
            A_BOLD);
        return layout;  // valid == false
    }
    layout.valid = true;

    y += draw_header(scr, y);

    // Pet art: drawn inside a fixed-height area (with a headgear line on top)
    // so the name line and everything below stay put regardless of the mood's
    // art height. The worn decoration is rendered on the pet itself.
    const int pet_y = y, pet_x = 7;
    draw_pet(scr, pet, mood, pet_y, pet_x);
    y += kPetArtHeight;

    // Name, mood label, and coins.
    const attr_t mood_attr = color_attr(mood_color(mood)) | A_BOLD;
    put(scr, y, 7, pet.name, A_BOLD);
    put_cont(scr, "  —  ", A_NORMAL);
    put_cont(scr, mood_display_name(mood), mood_attr);
    put_cont(scr, std::format("   💰 {}", pet.coins),
             color_attr(Color::Yellow) | A_BOLD);

    layout.pet_y1 = pet_y;
    layout.pet_x1 = pet_x;
    layout.pet_y2 = y;
    layout.pet_x2 = pet_x + 18;

    // Speech bubble (what the pet is saying, e.g. an AI reaction), to the right
    // of the pet art.
    if (!pet.speech.empty()) {
        const int bx = 30;
        draw_speech(scr, pet.speech, pet_y + 1, bx, w - bx - 1);
    }

    // Status bars.
    y += draw_stats(scr, pet.stats, y + 1, cx, pet.snacks);
    y += 1;

    // Event log: a fixed number of rows are reserved so the footer and the
    // command buttons never move as messages come and go.
    const attr_t cyan = color_attr(Color::CyanBold) | A_BOLD;
    put(scr, y, cx, "╭─── Events ───╮", cyan);
    y += 1;
    draw_event_log(scr, pet.events, y, cx);
    y += kEventRows;
    put(scr, y, cx, "╰──────────────╯", cyan);

    // Commands.
    draw_commands(scr, y + 1, cx, highlight_btn, layout.buttons);
    return layout;
}

void draw_shop(WINDOW* scr, const Pet& pet, int selected,
               const std::string& status) {
    werase(scr);

    int h = 0, w = 0;
    getmaxyx(scr, h, w);
    if (h < kMinHeight || w < kMinWidth) {
        put(scr, 0, 0,
            std::format("Terminal too small ({}x{}). Need {}x{}+.", w, h,
                        kMinWidth, kMinHeight),
            A_BOLD);
        return;
    }

    const int cx = 4;
    const attr_t cyan = color_attr(Color::CyanBold) | A_BOLD;
    const attr_t yellow = color_attr(Color::Yellow) | A_BOLD;

    // Header box (middle line padded to the box width so the sides line up).
    const std::string title = "🛒 Pet Shop 🛒";
    constexpr int kInner = 38;
    const int tw = display_width(title);
    const int left = (kInner - tw) / 2;
    const int right = kInner - tw - left;
    put(scr, 0, 2, "╔══════════════════════════════════════╗", cyan);
    put(scr, 1, 2,
        "║" + std::string(left, ' ') + title + std::string(right, ' ') + "║",
        cyan);
    put(scr, 2, 2, "╚══════════════════════════════════════╝", cyan);

    put(scr, 3, cx, std::format("💰 Your coins: {}", pet.coins), yellow);

    constexpr int kLineW = 44;  // uniform width so the selection highlight is even
    int y = 5;
    const auto& items = shop_items();
    bool deco_header = false;
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        if (i == 0) {
            put(scr, y, cx, "🍽️  Food", cyan);
            ++y;
        }
        if (item.is_decoration && !deco_header) {
            ++y;  // spacer between sections
            put(scr, y, cx, "🎀  Decorations", cyan);
            ++y;
            deco_header = true;
        }

        const bool sel = (static_cast<int>(i) == selected);
        const std::string cursor = sel ? ">" : " ";
        std::string line;
        if (item.is_decoration) {
            std::string tag;
            if (pet.owns_decoration(item.id)) {
                tag = pet.is_wearing(item.id) ? "  (wearing)" : "  (owned)";
            }
            line = std::format("{} {} {:<9} {:>4}💰{}", cursor, item.icon,
                               item.name, item.price, tag);
        } else {
            line = std::format("{} {} {:<9} {:>4}💰  {}", cursor, item.icon,
                               item.name, item.price, food_effect_text(item));
        }
        put(scr, y, cx, pad_cells(line, kLineW), sel ? (A_REVERSE | A_BOLD) : A_NORMAL);
        ++y;
    }

    // Feedback from the last buy/equip attempt.
    ++y;
    if (!status.empty()) {
        put(scr, y, cx, status, yellow);
    }

    put(scr, h - 1, 2, "↑/↓ select    Enter/x buy    Esc/q back",
        color_attr(Color::Gray));
}

void draw_chat(WINDOW* scr, const Pet& pet, Mood mood,
               const std::vector<ChatLine>& log, const std::string& input,
               bool thinking) {
    werase(scr);

    int h = 0, w = 0;
    getmaxyx(scr, h, w);
    if (h < kMinHeight || w < kMinWidth) {
        put(scr, 0, 0,
            std::format("Terminal too small ({}x{}). Need {}x{}+.", w, h,
                        kMinWidth, kMinHeight),
            A_BOLD);
        return;
    }

    const int cx = 2;
    const attr_t cyan = color_attr(Color::CyanBold) | A_BOLD;

    // Title with the pet's current mood.
    put(scr, 0, cx, "💬 Chat with " + pet.name, cyan);
    put_cont(scr, "  (feeling " + mood_display_name(mood) + ")",
             color_attr(mood_color(mood)) | A_BOLD);

    const int input_row = h - 2;
    const int msg_top = 2;
    const int msg_bottom = input_row - 1;
    const int wrap_w = w - cx - 2;

    // Flatten the transcript into wrapped screen lines (newest at the bottom).
    std::vector<std::pair<std::string, bool>> rendered;  // (line, is_pet)
    for (const auto& line : log) {
        for (auto& wl : wrap_text(line.who + ": " + line.text, wrap_w)) {
            rendered.emplace_back(std::move(wl), line.is_pet);
        }
    }
    if (thinking) {
        rendered.emplace_back(pet.name + " is thinking…", true);
    }

    const int max_lines = msg_bottom - msg_top + 1;
    const int start = static_cast<int>(rendered.size()) > max_lines
                          ? static_cast<int>(rendered.size()) - max_lines
                          : 0;
    int y = msg_top;
    for (int i = start; i < static_cast<int>(rendered.size()) && y <= msg_bottom;
         ++i, ++y) {
        const attr_t a = rendered[i].second ? color_attr(Color::Green)
                                            : color_attr(Color::Cyan);
        put(scr, y, cx, rendered[i].first, a);
    }

    // Input line with a simple cursor.
    put(scr, input_row, cx, "> ", A_BOLD);
    put_cont(scr, input + "_", A_NORMAL);

    put(scr, h - 1, cx, "Enter send    Esc back to pet",
        color_attr(Color::Gray));
}

}  // namespace pet
