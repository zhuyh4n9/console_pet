# Copilot Instructions — console_pet

A Tamagotchi-style terminal pet in C++20 (ncursesw), built as a **client/server**
system. This is a C++ rewrite of the original `pet.py` (kept for reference).

## Build

CMake + C++20 + wide-character ncurses (`ncursesw`, found via pkg-config) +
pthreads. nlohmann/json is vendored under `third_party/`.

```sh
cmake -S . -B build
cmake --build build -j        # produces console_pet_server + console_pet_client
```

The build uses `-Wall -Wextra`; keep it warning-free.

## Testing & linting

There is **no test framework, linter, or CI** in this repo. Don't add one unless
asked. Verify changes manually:

- **Protocol/server**: run `console_pet_server`, then talk to it over TCP with
  newline-delimited JSON, e.g. `echo '{"cmd":"feed"}' | nc 127.0.0.1 50777`.
  Every command replies `{"ok":true,"state":{...}}`.
- **UI**: drive `console_pet_client` under a pty (Python `pty.fork`) and
  reconstruct the grid. Note ncurses does differential redraws, so mode switches
  (which `werase` the window) give the cleanest full-frame captures.
- **LLM path**: point `api_base` at a local mock OpenAI server
  (`POST /chat/completions` → `{"choices":[{"message":{"content":...}}]}`) so you
  don't burn a real API key/quota.

## Architecture

Client/server over TCP (default `127.0.0.1:50777`), newline-delimited JSON.

- **Server** (`src/server_main.cpp`): owns the `Pet`, runs the simulation tick,
  shop, and LLM `Brain`. `poll()`-based event loop (`poll()` on listen fd +
  client fds + tick timer). **Every command replies with the full serialized
  state**, so clients stay trivial. Autosaves periodically and on shutdown.
- **Client** (`src/client_main.cpp`): thin curses UI. Each frame: send one
  command → receive state → render. No game logic lives here.
- **Protocol** (`src/proto.cpp`): `serialize_state`/`parse_state` are the single
  source of truth for the wire format. Client→server:
  `{"cmd":"poll|feed|snack|play|sleep|heal|work|restart"}`,
  `{"cmd":"buy","id":…}`, `{"cmd":"chat","text":…}`.
- The **LLM is just another client**: chat flows client→server→`Brain`→LLM→
  transcript; the server also generates in-character "reactions" (speech bubble)
  on actions. `Brain` runs LLM calls on a background thread so the server never
  blocks; it drops queued tasks on shutdown to exit promptly.

Dependency flow is one-directional and most modules are **pure logic with no
ncurses dependency** (so they're headlessly testable):
`pet_stats → mood → pet_art`; `pet` builds on those + `event_log`;
`shop`/`config`/`llm` build on `pet`; `brain` builds on `llm`; `proto`/`net` are
transport; only `ui.cpp` and the two `*_main.cpp` touch ncurses.

**Rendering is a pure function of state**: the server computes `Mood`
(`compute_mood`, with hysteresis via `prev_mood`) and passes it into the draw
functions — the UI never recomputes mood or mutates state.

## Key conventions

### ncurses / Unicode (hard-won)
- **Always `keypad(scr, TRUE)`** after creating a window, or mouse/arrow escape
  sequences leak a bare `ESC` (27) that triggers quit.
- **Emoji are 2 cells wide.** Use `getcurx(w)` (real cursor column) to compute
  clickable button regions, and `display_width`/`pad_cells` (wcwidth-based, in
  `text.cpp`) for column alignment — never `std::string::size()`.
- **Multi-byte `char` literals are invalid** (e.g. `'─'` won't compile). Build
  repeated lines by appending the string in a loop.
- Use `wget_wch` for wide-char text input (returns `OK` vs `KEY_CODE_YES`);
  handle UTF-8 backspace with `pop_codepoint` (walks back over `10xxxxxx`
  continuation bytes). Append a typed codepoint via `to_utf8((wchar_t)wch)`.
- Keep the layout **fixed** (constant art height with a reserved headgear line,
  fixed event-log rows) so nothing shifts as mood/events change.

### Config & secrets
- Config resolution: `PET_CONFIG` env → `$XDG_CONFIG_HOME/console_pet/config.json`
  (default `~/.config/...`) → `./pet_config.json` fallback. Server `host`/`port`
  live under a `"server"` key; override with `PET_HOST`/`PET_PORT`.
- API key resolution: `PET_API_KEY` env → system keyring (`secret-tool`) →
  config file. **Never commit keys.** `pet_config.json` at the repo root is the
  user's real (gitignored) config — do not delete or overwrite it.
- Non-secret env overrides: `PET_API_BASE`, `PET_LLM_MODEL`, `PET_PERSONALITY`.

### LLM client
- `libcurl` is **not installed**; `llm.cpp` shells out to the `curl` CLI via
  `popen`, writing the request body and a mode-600 curl config (holding the
  `Authorization` header) to temp files so the key never appears on argv.
- On any failure/disabled LLM, fall back to mood-based canned replies — chat must
  always work offline.

### Environment constraints
- No root/sudo (can't install packages). `libcurl` and `secret-tool` are absent —
  the code degrades gracefully without them.
- `pet_save.json` is **cwd-relative live game data** (modified externally while
  the pet "lives"). Don't corrupt or needlessly rewrite it; run test servers from
  a temp dir to avoid touching the real save.

### Build detail
- `third_party` is a **PUBLIC** include dir on `console_pet_core` because the
  public header `proto.hpp` includes `<nlohmann/json.hpp>`; keep it public.
