# console_pet

A Tamagotchi-style virtual pet that lives in your terminal. Feed it, play with
it, give it medicine, and keep it alive — rendered with flicker-free ncurses
and full-color ASCII art.

This is a C++ rewrite of the original `pet.py`, structured for maintainability:
the game logic is pure and UI-free, so it builds fast and is easy to test.

It runs as a **client/server** system. A headless **server** owns the pet, the
simulation, the shop, and the LLM "brain"; a thin **client** is the curses UI
that connects over TCP to control the pet and read its state. Because the model
talks to the server through the same protocol as the UI, an LLM (or any other
program) can drive the pet over the network.

## Features

- Four stats (hunger, happiness, energy, health) that decay over time
- 14 animated moods with hysteresis so the face doesn't flicker at thresholds
- Actions: feed, snack, play, sleep, heal, work, chat, restart
- **Economy**: send the pet to **work** to earn 🪙 coins — a job takes a random
  amount of time (the pet shows a working pose and can't do other things until
  it's done), costs energy and a little happiness, and a happier pet earns more
- **Shop**: buy foods with different effects (apple, coffee, salad, fish, cake,
  sushi) and cosmetic **decorations** (bow, scarf, hat, glasses, crown) that
  render right on the pet (a hat sits on its head, a scarf on its neck, …)
- **AI pet (optional LLM)**: hook the pet up to any OpenAI-compatible API and
  it comes alive — it **reacts in character** to what you do (feeding, playing,
  working…) with a speech bubble, and you can open a free-form **chat** (`c`).
  Replies are aware of its personality and current mood, and are generated in
  the background so the UI never stalls. Without an API it falls back to cute
  mood-based canned replies
- Automatic behaviors: pets eat stocked snacks when hungry and fall asleep when
  exhausted; overfeeding can cause a stomach ache
- Mouse support: click the pet to play, or click the command buttons
- Auto-save to `pet_save.json` (compatible with the original Python format;
  coins, decorations, and work state are added on top and default gracefully
  for old saves)

## Building

Requirements: a C++20 compiler, CMake ≥ 3.16, and wide-character ncurses
(`ncursesw`). On Debian/Ubuntu: `sudo apt install libncursesw5-dev`.

```sh
cmake -S . -B build
cmake --build build -j
```

Two binaries are produced:

- `build/console_pet_server` — the headless server (owns the pet + simulation).
- `build/console_pet_client` — the terminal UI.

## Running

Start the server, then connect the client:

```sh
./build/console_pet_server &   # owns the pet; keeps running in the background
./build/console_pet_client     # the UI; connect/disconnect freely
```

The server listens on `127.0.0.1:50777` by default (see the `server` config
below). Run the server from the directory where you want `pet_save.json` to
live. The client is stateless — you can quit and reconnect without losing the
pet, and the simulation keeps running while no client is attached. A UTF-8
locale and a terminal of at least 52×27 are recommended so the emoji and
box-drawing glyphs render correctly.

## Controls

| Key | Action      |
|-----|-------------|
| `f` | Feed        |
| `t` | Stock snack |
| `p` | Play        |
| `s` | Sleep       |
| `h` | Heal        |
| `w` | Work (earn coins) |
| `b` | Open shop   |
| `c` | Chat with pet |
| `r` | Restart     |
| `q` / `Esc` | Quit |

You can also click the on-screen buttons, or click the pet directly to play.

### In the shop

| Key | Action |
|-----|--------|
| `↑` / `↓` (or `k` / `j`) | Move selection |
| `Enter` / `x` | Buy item (or equip/unequip an owned decoration) |
| `Esc` / `q` | Back to the pet |

### In the chat

| Key | Action |
|-----|--------|
| type | Compose a message (UTF-8 supported) |
| `Enter` | Send |
| `Backspace` | Delete |
| `Esc` | Back to the pet |

## Talking with an LLM

The pet can react and talk back using any OpenAI-compatible chat API. When it's
enabled, the pet comments on what you do (a speech bubble pops up when you feed,
play with, or work it) and you can open a free-form chat with `c`.

### Config location

Config lives in your user config directory — **not** the project folder — so it
won't be committed by accident. Copy `pet_config.example.json` there:

```sh
mkdir -p ~/.config/console_pet
cp pet_config.example.json ~/.config/console_pet/config.json
```

(The path is `$XDG_CONFIG_HOME/console_pet/config.json`, defaulting to
`~/.config/console_pet/config.json`. You can point elsewhere with the
`PET_CONFIG` environment variable; a `./pet_config.json` is still read as a
last-resort fallback.)

```json
{
  "llm": {
    "enabled": true,
    "api_base": "https://api.openai.com/v1",
    "model": "gpt-4o-mini",
    "personality": "a cheerful, curious, and playful cat",
    "timeout_sec": 30,
    "react": true
  },
  "server": {
    "host": "127.0.0.1",
    "port": 50777
  }
}
```

- `server.host` / `server.port` — where the server listens and the client
  connects (override with the `PET_HOST` / `PET_PORT` env vars). Use
  `0.0.0.0` to allow connections from other machines.
- `api_base` — any OpenAI-compatible endpoint (OpenAI, Azure, a local Ollama /
  llama.cpp server, …); the client calls `<api_base>/chat/completions`.
- `personality` — folded into the system prompt along with the pet's live mood
  and stats, so replies reflect how it's feeling.
- `react` — whether the pet reacts in character to interactions (speech bubble).

### The API key

Prefer not to store the key in the file. It's resolved in this order:

1. `PET_API_KEY` environment variable.
2. The system **keyring** (GNOME Keyring / KWallet via `secret-tool`):
   ```sh
   secret-tool store --label="Console Pet API key" application console_pet
   ```
3. The `api_key` field in the config file (least preferred).

Other settings can also be overridden by env: `PET_API_BASE`, `PET_LLM_MODEL`,
`PET_PERSONALITY`.

If the LLM is disabled or a request fails, the pet answers with a cute
mood-based canned reply, so interaction always works. Reactions run on a
background thread (the UI never blocks), and requests go through the system
`curl` using a mode-600 temp config so the key never appears on the command
line.

## Architecture

The project is a client/server system built on a shared core library.

```
                 TCP (newline-delimited JSON)
  ┌──────────┐   {"cmd":"feed"} / {"ok":true,"state":{...}}   ┌──────────┐
  │  client  │ ─────────────────────────────────────────────▶ │  server  │
  │ (curses) │ ◀───────────────────────────────────────────── │ (owns    │
  └──────────┘        full state on every reply               │   the    │
        ▲                                                     │   pet)   │
        │ same protocol                                       └────┬─────┘
  ┌──────────┐                                                     │
  │ LLM / any│ ────────────────────────────────────────────────────┘
  │  client  │   (the "model" is just another client)
  └──────────┘
```

- **Server** (`server_main.cpp`): owns the `Pet`, runs the simulation tick,
  the shop, and the LLM `Brain`; serves any number of clients with `poll()`.
  Every command replies with the full serialized state, so clients are trivial.
- **Client** (`client_main.cpp`): a thin curses UI. Each frame it sends one
  command (`poll` or an action), receives the state, and renders it. All game
  logic lives in the server.
- **Protocol** (`proto.cpp`): newline-delimited JSON. Client → server:
  `{"cmd":"poll|feed|snack|play|sleep|heal|work|restart"}`,
  `{"cmd":"buy","id":…}`, `{"cmd":"chat","text":…}`. Server → client:
  `{"ok":true,"state":{…}}`. The state carries stats, mood, coins, decorations,
  speech bubble, events, and the chat transcript.

The code is split into small, single-responsibility modules. Everything except
`ui` and the two `*_main` entry points is pure logic with no ncurses dependency,
so it compiles quickly and can be unit-tested without a terminal.

```
include/console_pet/   public headers
src/                   implementations
  text.cpp       UTF-8 ↔ wide-string helpers for ncursesw rendering
  pet_stats.cpp  the four stats + natural decay
  mood.cpp       stats → discrete Mood (with hysteresis)
  pet_art.cpp    ASCII-art frames per mood
  event_log.cpp  bounded, color-tagged message ring buffer
  pet.cpp        pet state + player actions + per-tick simulation
  shop.cpp       shop catalog + buy/equip logic
  config.cpp     user-dir config + env/keyring loading (LLM + server settings)
  llm.cpp        OpenAI-compatible chat client (via curl)
  brain.cpp      background thread generating reactions + chat replies
  save.cpp       JSON save/load (nlohmann/json)
  proto.cpp      state (de)serialization shared by server and client
  net.cpp        TCP + newline-delimited JSON messaging
  ui.cpp         all ncurses rendering (main, shop, chat screens)
  server_main.cpp  server entry point + event loop + command dispatch
  client_main.cpp  client entry point + UI loop (poll/render/input)
third_party/nlohmann/json.hpp   vendored JSON library
```

Dependency flow is one-directional:
`pet_stats → mood → pet_art`, and `pet` builds on `pet_stats`/`event_log`/`mood`;
`shop`/`config`/`llm` build on `pet`; `brain` builds on `llm`; `proto`/`net`
are transport; `ui` and the two entry points sit on top and are the only modules
that touch ncurses.

## Save format

`pet_save.json` keeps the original Python fields (so existing saves carry over)
and adds economy/decoration/work state, which default gracefully when absent:

```json
{
  "name": "Buddy",
  "stats": { "hunger": 80, "happiness": 80, "energy": 80, "health": 80, "age": 0, "alive": true },
  "sleeping_ticks": 0,
  "working_ticks": 0,
  "snacks": 0,
  "coins": 0,
  "owned_decorations": [],
  "worn_decoration": ""
}
```

## Notes

- `pet.py` is kept for reference; this C++ project supersedes it.
- The Windows binary under `win/` predates this rewrite. Rebuilding for Windows
  would require a curses port such as PDCurses.
