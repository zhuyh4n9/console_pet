// Console Pet client: a thin terminal UI that connects to the server, polls its
// state, renders it, and forwards the user's commands. All game logic lives in
// the server; the client only renders RemoteState and sends commands.

#include <ncurses.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "console_pet/config.hpp"
#include "console_pet/net.hpp"
#include "console_pet/proto.hpp"
#include "console_pet/shop.hpp"
#include "console_pet/text.hpp"
#include "console_pet/ui.hpp"

namespace pet {
namespace {

enum class Mode { Main, Shop, Chat };

bool rect_contains(const ButtonRegion& r, int x, int y) {
    return y >= r.y1 && y <= r.y2 && x >= r.x1 && x <= r.x2;
}

// Send one command and read back the resulting state. Returns false on a broken
// connection or a protocol error.
bool round_trip(int fd, const nlohmann::json& cmd, RemoteState& rs) {
    if (!net_send(fd, cmd)) return false;
    nlohmann::json resp;
    if (!net_recv(fd, resp)) return false;
    if (!resp.value("ok", false)) return false;
    rs = parse_state(resp.value("state", nlohmann::json::object()));
    return true;
}

WINDOW* make_screen() {
    WINDOW* scr = newwin(LINES, COLS, 0, 0);
    keypad(scr, TRUE);
    wtimeout(scr, 0);
    return scr;
}

}  // namespace
}  // namespace pet

int main() {
    using namespace pet;
    const Config config = load_config();
    const int fd = net_connect(config.host, config.port);
    if (fd < 0) {
        std::cerr << "Cannot connect to the Console Pet server at "
                  << config.host << ":" << config.port << ".\n"
                  << "Start it first with: console_pet_server\n";
        return 1;
    }

    initscr();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    if (has_colors()) init_colors();
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);

    WINDOW* scr = make_screen();

    Mode mode = Mode::Main;
    int highlight = 0;       // highlighted command button (main screen)
    int shop_selected = 0;   // highlighted shop row
    std::string chat_input;  // in-progress chat message
    RemoteState rs;
    bool ok = round_trip(fd, {{"cmd", "poll"}}, rs);

    // Map a command-button hotkey to a server command / mode change / quit.
    auto dispatch = [&](char key, nlohmann::json& next_cmd, bool& quit) {
        switch (key) {
            case 'f': next_cmd = {{"cmd", "feed"}}; break;
            case 't': next_cmd = {{"cmd", "snack"}}; break;
            case 'p': next_cmd = {{"cmd", "play"}}; break;
            case 's': next_cmd = {{"cmd", "sleep"}}; break;
            case 'h': next_cmd = {{"cmd", "heal"}}; break;
            case 'w': next_cmd = {{"cmd", "work"}}; break;
            case 'b': mode = Mode::Shop; break;
            case 'c': mode = Mode::Chat; break;
            case 'r': next_cmd = {{"cmd", "restart"}}; break;
            case 'q': quit = true; break;
            default: break;
        }
    };

    while (ok) {
        // Render the current state.
        Layout layout;
        werase(scr);
        if (mode == Mode::Chat) {
            draw_chat(scr, rs.pet, rs.mood, rs.chat, chat_input, rs.chat_pending);
        } else if (mode == Mode::Shop) {
            draw_shop(scr, rs.pet, shop_selected, rs.shop_status);
        } else {
            layout = draw_all(scr, rs.pet, rs.mood, highlight);
        }
        wrefresh(scr);

        napms(80);

        nlohmann::json next_cmd = {{"cmd", "poll"}};
        bool quit = false;

        if (mode == Mode::Chat) {
            wint_t wch = 0;
            const int rc = wget_wch(scr, &wch);
            if (rc == OK) {
                if (wch == '\n' || wch == '\r') {
                    if (!chat_input.empty()) {
                        next_cmd = {{"cmd", "chat"}, {"text", chat_input}};
                        chat_input.clear();
                    }
                } else if (wch == 127 || wch == 8) {
                    pop_codepoint(chat_input);
                } else if (wch == 27) {
                    mode = Mode::Main;
                } else if (wch >= 32) {
                    chat_input += to_utf8(static_cast<wchar_t>(wch));
                }
            } else if (rc == KEY_CODE_YES) {
                if (wch == KEY_BACKSPACE) {
                    pop_codepoint(chat_input);
                } else if (wch == KEY_RESIZE) {
                    delwin(scr);
                    endwin();
                    refresh();
                    scr = make_screen();
                }
            }
        } else if (mode == Mode::Shop) {
            const int n = static_cast<int>(shop_items().size());
            const int ch = wgetch(scr);
            if (ch == 'j' || ch == KEY_DOWN) {
                shop_selected = (shop_selected + 1) % n;
            } else if (ch == 'k' || ch == KEY_UP) {
                shop_selected = (shop_selected - 1 + n) % n;
            } else if (ch == '\n' || ch == '\r' || ch == 'x') {
                next_cmd = {{"cmd", "buy"}, {"id", shop_items()[shop_selected].id}};
            } else if (ch == 27 || ch == 'q' || ch == 'b') {
                mode = Mode::Main;
            } else if (ch == KEY_RESIZE) {
                delwin(scr);
                endwin();
                refresh();
                scr = make_screen();
            }
        } else {  // Main
            const int ch = wgetch(scr);
            if (ch == KEY_MOUSE) {
                MEVENT ev{};
                if (getmouse(&ev) == OK && layout.valid) {
                    bool hit = false;
                    for (std::size_t i = 0; i < layout.buttons.size(); ++i) {
                        if (rect_contains(layout.buttons[i], ev.x, ev.y)) {
                            highlight = static_cast<int>(i);
                            dispatch(layout.buttons[i].key, next_cmd, quit);
                            hit = true;
                        }
                    }
                    // Clicking the pet itself plays with it.
                    if (!hit && ev.x >= layout.pet_x1 && ev.x <= layout.pet_x2 &&
                        ev.y >= layout.pet_y1 && ev.y <= layout.pet_y2) {
                        next_cmd = {{"cmd", "play"}};
                    }
                }
            } else if (ch == KEY_RESIZE) {
                delwin(scr);
                endwin();
                refresh();
                scr = make_screen();
            } else if (ch == KEY_LEFT || ch == KEY_RIGHT || ch == KEY_UP ||
                       ch == KEY_DOWN) {
                const int col = highlight % 5;
                const int row = highlight / 5;
                int nc = col, nr = row;
                if (ch == KEY_LEFT) nc = (col + 4) % 5;
                else if (ch == KEY_RIGHT) nc = (col + 1) % 5;
                else nr = (row + 1) % 2;  // UP/DOWN toggle the two rows
                highlight = nr * 5 + nc;
            } else if (ch == '\n' || ch == '\r') {
                if (layout.valid &&
                    highlight < static_cast<int>(layout.buttons.size())) {
                    dispatch(layout.buttons[highlight].key, next_cmd, quit);
                }
            } else if (ch != ERR) {
                dispatch(static_cast<char>(ch), next_cmd, quit);
            }
        }

        if (quit) break;
        ok = round_trip(fd, next_cmd, rs);
    }

    delwin(scr);
    endwin();
    close(fd);
    if (!ok) {
        std::cerr << "Lost connection to the Console Pet server.\n";
        return 1;
    }
    return 0;
}
