#!/usr/bin/env python3
"""
Console-based Virtual Pet (电子宠物)
A Tamagotchi-style pet you can feed, play with, and care for — all in your terminal.
Uses curses for flicker-free rendering.
"""

import os
import sys
import json
import random
import curses
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple

# ── Pet Data ───────────────────────────────────────────────────────
@dataclass
class PetStats:
    hunger: int = 80      # 0=starving, 100=full
    happiness: int = 80   # 0=sad, 100=ecstatic
    energy: int = 80      # 0=exhausted, 100=energetic
    health: int = 80      # 0=sick, 100=healthy
    age: int = 0          # ticks alive
    alive: bool = True

    def clamp(self) -> None:
        for attr in ("hunger", "happiness", "energy", "health"):
            setattr(self, attr, max(0, min(100, getattr(self, attr))))

    def decay(self) -> None:
        """Natural stat decay over time (gentle pace)."""
        self.hunger -= random.randint(0, 1)
        self.happiness -= random.randint(0, 1)
        self.energy -= random.randint(0, 1)
        # Health drops if other stats are critically low
        if self.hunger < 20 or self.energy < 20 or self.happiness < 20:
            self.health -= random.randint(1, 2)
        elif self.health < 100:
            self.health = min(100, self.health + 1)
        self.age += 1
        self.clamp()
        if self.health <= 0 or self.hunger <= 0:
            self.alive = False


# ── ASCII Art Frames ───────────────────────────────────────────────
PET_FRAMES: Dict[str, List[str]] = {
    # Happy / default / content / bored
    "happy": [
        r"""
       /\___/\
      (  o o  )
      (  =^=  )
       (______)
        |    |
       /|    |\
      / |    | \
    """,
        r"""
       /\___/\
      (  ^ ^  )
      (  =^=  )
       (______)
        |    |
       /|    |\
      / |    | \
    """,
    ],
    "content": [
        r"""
       /\___/\
      (  · ·  )
      (  =w=  )
       (______)
        |    |
    """,
    ],
    "bored": [
        r"""
       /\___/\
      (  ¬_¬  )
      (  ... )
       (______)
        |    |
    """,
    ],
    "grumpy": [
        r"""
       /\___/\
      (  >_<  )
      (  ¬¬¬  )
       (______)
        |    |\
    """,
    ],
    # Starving / hungry
    "starving": [
        r"""
       /\___/\
      (  @_@  )
      (  > <  )
       (______)
    """,
    ],
    # Hungry
    "hungry": [
        r"""
       /\___/\
      (  o_o  )
      (  > <  )
       (______)
    """,
    ],
    # Sad
    "sad": [
        r"""
       /\___/\
      (  T_T  )
      (  - -  )
       (______)
        |    |
    """,
    ],
    # Sleepy
    "sleepy": [
        r"""
       /\___/\
      (  -_-  )
      (  zzz  )
       (______)
        |    |
        Z    Z
    """,
    ],
    # Playing
    "playing": [
        r"""
       /\___/\
      (  ★★  )
      (  =^= )
       (______)
        |    |
       /☆   ☆\
      /  ☆☆   \
    """,
        r"""
       /\___/\
      (  ★★  )
      (  =^= )
       (______)
        |    |
       /  ☆☆  \
      / ☆  ☆   \
    """,
    ],
    "hyper": [
        r"""
       /\___/\
      (  ✧✧  )
      (  =D= )
       (______)
        |    |
       /☆   ☆\
      ★  ☆☆  ★
    """,
        r"""
       /\___/\  ☆
      (  ✧✧  )
      (  =D= ) ★
       (______)
       ★|    |☆
       /  ☆☆  \
    """,
    ],
    # Sick
    "sick": [
        r"""
       /\___/\
      (  X_X  )
      (  ~~~  )
       (______)
        |    |
       ~      ~
    """,
    ],
    # Dead
    "dead": [
        r"""
       /\___/\
      (  X_X  )
      (  ---  )
       (______)
        |    |
      RIP...
    """,
    ],
    # Egg (baby stage)
    "egg": [
        r"""
          ___
         /   \
        |     |
        |  ·  |
         \___/
    """,
    ],
}


def get_pet_mood(stats: PetStats, sleeping_ticks: int = 0, frame_idx: int = 0,
                  prev_mood: str = "egg") -> str:
    """Determine the pet's mood with hysteresis at boundaries.
    Once in a mood, stats must overshoot the threshold by ~8 points to switch,
    preventing rapid toggling at boundaries."""
    # Stable jitter: drifts slowly, ±4 range
    _seed = (stats.hunger * 31 + stats.happiness * 37 + stats.energy * 41
             + stats.health * 43 + stats.age * 47 + frame_idx // 30)
    jit = (_seed % 9) - 4
    j = lambda v: v + jit

    # Hysteresis: when leaving current mood, require stats to overshoot
    # "soft" = wider threshold for leaving (harder to leave)
    h = lambda v, margin=8: v - margin if prev_mood else v + margin

    if not stats.alive:
        return "dead"
    if sleeping_ticks > 0:
        return "sleepy"

    # Each check: use normal threshold for entering, wider for leaving
    def check(mood, threshold, cmp_lt=True):
        """Returns True if mood should be active.
        cmp_lt=True: mood triggers when stat drops below threshold.
        cmp_lt=False: mood triggers when stat exceeds threshold."""
        if cmp_lt:
            # Enter when below threshold, leave when above threshold+hysteresis
            if prev_mood == mood:
                return threshold + 8  # leave only if well above
            return threshold
        else:
            if prev_mood == mood:
                return threshold - 8  # leave only if well below
            return threshold

    # Priority-ordered mood checks with hysteresis
    if stats.health < (check("sick", j(25)) if prev_mood == "sick" else j(25)):
        return "sick"
    if stats.hunger < (j(15) + 8 if prev_mood == "starving" else j(15)):
        return "starving"
    if stats.hunger < (j(35) + 8 if prev_mood == "hungry" else j(35)):
        return "hungry"
    if stats.energy < (j(20) + 8 if prev_mood == "sleepy" else j(20)):
        # low energy sleepy (different from sleeping_ticks)
        return "sleepy"
    if stats.happiness < (j(25) + 8 if prev_mood == "sad" else j(25)):
        return "sad"
    if stats.happiness < (j(45) + 8 if prev_mood == "grumpy" else j(45)):
        return "grumpy"
    if stats.age < j(5):
        return "egg"

    # Positive moods (trigger when stat exceeds threshold)
    if stats.energy > (j(85) - 8 if prev_mood == "hyper" else j(85)) and stats.happiness > j(55):
        return "hyper"
    if stats.happiness > (j(75) - 8 if prev_mood == "playing" else j(75)) and stats.energy > j(55):
        return "playing"
    if stats.hunger > (j(90) - 8 if prev_mood == "content" else j(90)) and stats.happiness > j(55):
        return "content"
    if stats.happiness < (j(55) + 8 if prev_mood == "bored" else j(55)):
        return "bored"
    return "happy"


def render_pet(stats: PetStats, frame_idx: int, sleeping_ticks: int = 0,
               prev_mood: str = "egg") -> str:
    """Render the pet in its current mood."""
    mood = get_pet_mood(stats, sleeping_ticks, frame_idx, prev_mood)
    frames = PET_FRAMES.get(mood, PET_FRAMES["happy"])
    return frames[frame_idx % len(frames)]


# ── Curses Color Setup ─────────────────────────────────────────────
# Color pair IDs
(CP_WHITE, CP_RED, CP_GREEN, CP_YELLOW, CP_BLUE, CP_MAGENTA, CP_CYAN, CP_GRAY,
 CP_RED_BOLD, CP_GREEN_BOLD, CP_YELLOW_BOLD, CP_CYAN_BOLD, CP_MAGENTA_BOLD) = range(1, 14)


def init_colors() -> None:
    """Initialize curses color pairs."""
    curses.start_color()
    curses.use_default_colors()
    curses.init_pair(CP_WHITE, curses.COLOR_WHITE, -1)
    curses.init_pair(CP_RED, curses.COLOR_RED, -1)
    curses.init_pair(CP_GREEN, curses.COLOR_GREEN, -1)
    curses.init_pair(CP_YELLOW, curses.COLOR_YELLOW, -1)
    curses.init_pair(CP_BLUE, curses.COLOR_BLUE, -1)
    curses.init_pair(CP_MAGENTA, curses.COLOR_MAGENTA, -1)
    curses.init_pair(CP_CYAN, curses.COLOR_CYAN, -1)
    curses.init_pair(CP_GRAY, 8, -1)  # bright black = gray
    curses.init_pair(CP_RED_BOLD, curses.COLOR_RED, -1)
    curses.init_pair(CP_GREEN_BOLD, curses.COLOR_GREEN, -1)
    curses.init_pair(CP_YELLOW_BOLD, curses.COLOR_YELLOW, -1)
    curses.init_pair(CP_CYAN_BOLD, curses.COLOR_CYAN, -1)
    curses.init_pair(CP_MAGENTA_BOLD, curses.COLOR_MAGENTA, -1)
    # Enable bold on these pairs
    for cp in (CP_RED_BOLD, CP_GREEN_BOLD, CP_YELLOW_BOLD, CP_CYAN_BOLD, CP_MAGENTA_BOLD):
        pass  # we'll use A_BOLD at draw time instead


# ── Event Log ──────────────────────────────────────────────────────
class EventLog:
    """Curses-aware event log."""

    def __init__(self, max_lines: int = 3):
        self.messages: List[Tuple[str, int]] = []  # (text, curses_color_pair)
        self.max_lines = max_lines

    def add(self, text: str, color_pair: int = CP_WHITE) -> None:
        self.messages.append((text, color_pair))
        if len(self.messages) > self.max_lines:
            self.messages.pop(0)

    def draw(self, win, y: int, x: int) -> int:
        """Draw messages starting at (y, x). Returns number of lines drawn."""
        attr = curses.color_pair(CP_GRAY)
        win.addstr(y, x, "│ ", attr)
        if not self.messages:
            win.addstr("(no events yet)", attr)
            return 1
        for i, (msg, cp) in enumerate(self.messages):
            win.addstr(y + i, x, "│ ", attr)
            win.addstr(msg, curses.color_pair(cp))
        return len(self.messages)


# ── Status Bar ─────────────────────────────────────────────────────
def bar_text(value: int, width: int = 20) -> Tuple[str, int]:
    """Return (bar_string, color_pair) for a stat value."""
    filled = int(value / 100 * width)
    block = "█" * filled + "░" * (width - filled)
    label = f"{block} {value:3d}%"

    if value > 60:
        return label, CP_GREEN
    elif value > 30:
        return label, CP_YELLOW
    return label, CP_RED


def draw_stats(win, stats: PetStats, y: int, x: int, snacks: int = 0) -> int:
    """Draw stats panel. Returns number of lines drawn."""
    cyan = curses.color_pair(CP_CYAN_BOLD) | curses.A_BOLD
    cyan_dim = curses.color_pair(CP_CYAN)

    win.addstr(y, x, "╭─── Pet Status ───╮", cyan)
    y += 1

    for i, (icon, label, val) in enumerate([
        ("🍔", "Hunger:    ", stats.hunger),
        ("😊", "Happy:     ", stats.happiness),
        ("⚡", "Energy:    ", stats.energy),
        ("❤️ ", "Health:    ", stats.health),
    ]):
        win.addstr(y + i, x, "│ ", cyan_dim)
        win.addstr(f"{icon} {label}")
        bar_s, bar_cp = bar_text(val)
        win.addstr(bar_s, curses.color_pair(bar_cp))
        win.addstr(" │", cyan_dim)

    win.addstr(y + 4, x, f"│ 🎂 Age:       {stats.age:5d} ticks  │", cyan_dim)
    snack_str = f"🍪 Snacks:     {snacks:2d}" if snacks > 0 else "🍪 Snacks:      0"
    win.addstr(y + 5, x, f"│ {snack_str:<23}│", cyan_dim)
    win.addstr(y + 6, x, "╰─────────────────╯", cyan_dim)
    return 7
class Pet:
    def __init__(self, name: str = "Buddy", stats: Optional[PetStats] = None):
        self.name = name
        self.stats = stats or PetStats()
        self.frame_idx = 0
        self.events = EventLog()
        self.sleeping_ticks = 0   # remaining ticks to show sleep animation
        self.snacks = 0           # treat inventory
        self.prev_mood = "egg"    # tracks last mood for hysteresis

    def _wake_if_sleeping(self) -> None:
        """Wake the pet if currently sleeping."""
        if self.sleeping_ticks > 0:
            self.sleeping_ticks = 0
            self.events.add(f"⏰ {self.name} was woken up!", CP_YELLOW)

    def feed(self) -> None:
        if not self.stats.alive:
            self.events.add(f"{self.name} has passed away... 💀", CP_RED)
            return
        self._wake_if_sleeping()
        old_hunger = self.stats.hunger
        amount = random.randint(15, 30)
        self.stats.hunger = min(100, self.stats.hunger + amount)
        self.stats.happiness = min(100, self.stats.happiness + 5)

        # Overfeeding: the fuller the pet, the higher the risk of a stomach ache
        if old_hunger > 70:
            overfeed_chance = (old_hunger - 70) / 30 * 0.85  # 0% at 70, ~85% at 100
            if random.random() < overfeed_chance:
                dmg = random.randint(3, 10)
                self.stats.health = max(0, self.stats.health - dmg)
                self.events.add(
                    f"🍽️  You fed {self.name}! (+{amount} hunger) — but they ate too much! (-{dmg} health 🤢)",
                    CP_YELLOW,
                )
                return

        self.events.add(f"🍽️  You fed {self.name}! (+{amount} hunger)", CP_GREEN)

    def play(self) -> None:
        if not self.stats.alive:
            self.events.add(f"{self.name} has passed away... 💀", CP_RED)
            return
        self._wake_if_sleeping()
        if self.stats.energy < 15:
            self.events.add(f"😴 {self.name} is too tired to play...", CP_YELLOW)
            return
        amount = random.randint(15, 25)
        self.stats.happiness = min(100, self.stats.happiness + amount)
        self.stats.energy -= random.randint(8, 15)
        self.stats.hunger -= random.randint(5, 10)
        self.stats.clamp()
        self.events.add(f"🎾 You played with {self.name}! (+{amount} happiness)", CP_MAGENTA)

    def sleep(self) -> None:
        if not self.stats.alive:
            self.events.add(f"{self.name} has passed away... 💀", CP_RED)
            return
        if self.sleeping_ticks > 0:
            self.events.add(f"😴 {self.name} is already sleeping...", CP_YELLOW)
            return
        self.sleeping_ticks = random.randint(8, 14)  # recover gradually over ~8-14 ticks
        self.events.add(f"💤 {self.name} went to sleep...", CP_CYAN)

    def heal(self) -> None:
        if not self.stats.alive:
            self.events.add(f"{self.name} has passed away... 💀", CP_RED)
            return
        self._wake_if_sleeping()
        amount = random.randint(20, 35)
        self.stats.health = min(100, self.stats.health + amount)
        self.events.add(f"💊 You gave medicine to {self.name}! (+{amount} health)", CP_GREEN)

    def add_snack(self) -> None:
        """Stock up on treats for the pet."""
        if not self.stats.alive:
            self.events.add(f"{self.name} has passed away... 💀", CP_RED)
            return
        self.snacks += 1
        self.events.add(f"🍪 You stocked a snack! ({self.snacks} total)", CP_YELLOW)

    def tick(self) -> None:
        """Advance one game tick."""
        # Auto-sleep: tired pets fall asleep on their own
        auto_sleeping = False
        if self.sleeping_ticks > 0:
            # Currently sleeping — recover energy
            self.stats.energy = min(100, self.stats.energy + random.randint(2, 4))
            self.stats.health = min(100, self.stats.health + 1)
            self.sleeping_ticks -= 1
            auto_sleeping = True
            if self.sleeping_ticks == 0:
                self.events.add(f"⏰ {self.name} woke up!", CP_CYAN)

        if not auto_sleeping:
            # Normal decay when awake
            self.stats.decay()

            # Auto-eat snacks when hungry (probability scales with hunger level)
            if self.snacks > 0 and self.stats.hunger < 80:
                # Hungrier → more likely to eat: ~5% at hunger=79, ~95% at hunger=0
                chance = (80 - self.stats.hunger) / 80 * 0.9 + 0.05
                if random.random() < chance:
                    self.snacks -= 1
                    self.stats.hunger = min(100, self.stats.hunger + random.randint(5, 12))
                    self.stats.happiness = min(100, self.stats.happiness + random.randint(1, 3))
                    msg = f"🍪 {self.name} grabbed a snack! ({self.snacks} left)"
                    # Low chance of side effect (junk food is not always healthy)
                    if random.random() < 0.2:
                        if random.random() < 0.5:
                            self.stats.energy = max(0, self.stats.energy - random.randint(3, 8))
                            msg += " (sugar crash 💫)"
                        else:
                            self.stats.health = max(0, self.stats.health - random.randint(3, 6))
                            msg += " (stomach ache 🤢)"
                    self.events.add(msg, CP_YELLOW)

            # Fall asleep automatically when exhausted
            if self.stats.energy <= 15 and self.stats.alive:
                self.sleeping_ticks = random.randint(8, 14)
                self.events.add(f"😴 {self.name} fell asleep on their own...", CP_CYAN)

        self.frame_idx += 1
        if not self.stats.alive:
            self.events.add(f"💀 {self.name} has died... Press R to restart.", CP_RED)

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "stats": {
                "hunger": self.stats.hunger,
                "happiness": self.stats.happiness,
                "energy": self.stats.energy,
                "health": self.stats.health,
                "age": self.stats.age,
                "alive": self.stats.alive,
            },
            "sleeping_ticks": self.sleeping_ticks,
            "snacks": self.snacks,
        }

    @classmethod
    def from_dict(cls, data: dict) -> "Pet":
        s = data["stats"]
        pet = cls(
            name=data["name"],
            stats=PetStats(
                hunger=s["hunger"],
                happiness=s["happiness"],
                energy=s["energy"],
                health=s["health"],
                age=s["age"],
                alive=s["alive"],
            ),
        )
        pet.sleeping_ticks = data.get("sleeping_ticks", 0)
        pet.snacks = data.get("snacks", 0)
        return pet


# ── Save / Load ────────────────────────────────────────────────────
SAVE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "pet_save.json")


def save_pet(pet: Pet) -> None:
    try:
        with open(SAVE_FILE, "w") as f:
            json.dump(pet.to_dict(), f, indent=2)
    except IOError:
        pass


def load_pet() -> Optional[Pet]:
    try:
        with open(SAVE_FILE, "r") as f:
            data = json.load(f)
        return Pet.from_dict(data)
    except (IOError, json.JSONDecodeError, KeyError):
        return None


def delete_save() -> None:
    try:
        os.remove(SAVE_FILE)
    except OSError:
        pass


# ── Curses Drawing ─────────────────────────────────────────────────
def draw_pet(win, stats: PetStats, frame_idx: int, y: int, x: int, sleeping_ticks: int = 0,
             prev_mood: str = "egg") -> int:
    """Draw the pet ASCII art. Returns number of lines drawn."""
    mood = get_pet_mood(stats, sleeping_ticks, frame_idx, prev_mood)
    mood_colors = {
        "dead": CP_RED, "sick": CP_RED, "starving": CP_RED,
        "hungry": CP_YELLOW, "grumpy": CP_YELLOW, "sad": CP_YELLOW,
        "sleepy": CP_CYAN, "egg": CP_CYAN,
        "bored": CP_GRAY, "content": CP_GREEN_BOLD,
        "playing": CP_MAGENTA, "hyper": CP_MAGENTA_BOLD,
        "happy": CP_GREEN,
    }
    mood_cp = mood_colors.get(mood, CP_GREEN)
    attr = curses.color_pair(mood_cp)
    if mood != "dead" and mood != "bored":
        attr |= curses.A_BOLD

    art = render_pet(stats, frame_idx)
    lines = [l for l in art.split("\n") if l.strip()]
    for i, line in enumerate(lines):
        win.addstr(y + i, x, line, attr)
    return len(lines)


def draw_header(win, y: int) -> int:
    """Draw the title header. Returns lines drawn."""
    cyan = curses.color_pair(CP_CYAN) | curses.A_BOLD
    win.addstr(y, 2, "╔══════════════════════════════════════╗", cyan)
    win.addstr(y + 1, 2, "║        ", cyan)
    win.addstr("🐾 Console Pet 🐾", curses.A_BOLD)
    win.addstr("           ║", cyan)
    win.addstr(y + 2, 2, "╚══════════════════════════════════════╝", cyan)
    return 3


def draw_commands(win, y: int, x: int, highlight: int = -1) -> int:
    """Draw clickable command buttons. Returns (lines_drawn, button_regions).
    highlight: index of button under mouse (-1 = none)."""
    gray = curses.color_pair(CP_GRAY)
    win.addstr(y, x, "Commands:  (click or press key)", gray)
    y += 1

    buttons = [
        ("🍔 Feed ", "f"), ("🍪 Snack", "t"), ("🎾 Play ", "p"), ("💤 Sleep", "s"),
        ("💊 Heal ", "h"), ("🚪 Quit ", "q"), ("🔄 Restart", "r"),
    ]
    regions = []  # (y1, x1, y2, x2, action_key)
    bx = x
    for i, (label, key) in enumerate(buttons):
        # Draw button with optional highlight
        if i == highlight:
            attr = curses.A_REVERSE | curses.A_BOLD
        else:
            attr = curses.A_BOLD
        btn_text = f" {label} "
        win.addstr(y, bx, btn_text, attr)
        w = len(btn_text)
        regions.append((y, bx, y, bx + w - 1, key))
        bx += w

    return 2, regions


def draw_all(stdscr, pet: Pet, highlight_btn: int = -1) -> dict:
    """Render the entire screen. Returns layout regions for mouse hit detection."""
    stdscr.erase()
    h, w = stdscr.getmaxyx()
    cx = 2  # left margin
    y = 0
    regions = {}  # "pet": (y1, x1, y2, x2), "buttons": [...]

    # Check minimum size
    if h < 24 or w < 50:
        stdscr.addstr(0, 0, f"Terminal too small ({w}x{h}). Need 50x24+.", curses.A_BOLD)
        return regions

    y += draw_header(stdscr, y)

    # Pet art — track clickable zone
    pet_y, pet_x = y, 7
    art_lines = draw_pet(stdscr, pet.stats, pet.frame_idx, pet_y, pet_x, pet.sleeping_ticks, pet.prev_mood)
    y += art_lines

    # Name & mood
    mood = get_pet_mood(pet.stats, pet.sleeping_ticks, pet.frame_idx, pet.prev_mood)
    pet.prev_mood = mood
    mood_colors = {
        "dead": CP_RED, "sick": CP_RED, "starving": CP_RED,
        "hungry": CP_YELLOW, "grumpy": CP_YELLOW, "sad": CP_YELLOW,
        "sleepy": CP_CYAN, "egg": CP_CYAN,
        "bored": CP_GRAY, "content": CP_GREEN_BOLD,
        "playing": CP_MAGENTA, "hyper": CP_MAGENTA_BOLD,
        "happy": CP_GREEN,
    }
    mood_cp = mood_colors.get(mood, CP_GREEN)
    mood_attr = curses.color_pair(mood_cp) | curses.A_BOLD
    stdscr.addstr(y, 7, pet.name, curses.A_BOLD)
    stdscr.addstr("  —  ")
    stdscr.addstr(mood.upper(), mood_attr)

    # Store pet clickable region (art + name line)
    regions["pet"] = (pet_y, pet_x, y, pet_x + 18)

    # Status bars
    y += draw_stats(stdscr, pet.stats, y + 1, cx, pet.snacks)
    y += 1

    # Event log
    cyan = curses.color_pair(CP_CYAN_BOLD) | curses.A_BOLD
    stdscr.addstr(y, cx, "╭─── Events ───╮", cyan)
    y += 1
    y += pet.events.draw(stdscr, y, cx)
    stdscr.addstr(y, cx, "╰──────────────╯", cyan)

    # Commands
    _, btn_regions = draw_commands(stdscr, y + 1, cx, highlight_btn)
    regions["buttons"] = btn_regions

    return regions


# ── Main Game Loop ─────────────────────────────────────────────────
def run_curses(stdscr) -> None:
    """Main game loop (curses wrapper)."""
    init_colors()
    curses.curs_set(0)          # hide cursor
    stdscr.nodelay(True)        # non-blocking input
    curses.raw()                # raw input (no line buffering)
    curses.mousemask(curses.ALL_MOUSE_EVENTS | curses.REPORT_MOUSE_POSITION)

    # Try to load saved pet
    pet = load_pet()
    if pet:
        pet.events.add(f"👋 Welcome back, {pet.name}!", CP_CYAN)
    else:
        pet = Pet(name="Buddy")
        pet.events.add(f"🥚 A new pet has hatched! Meet {pet.name}!", CP_MAGENTA)

    tick_counter = 0
    TICK_INTERVAL = 30
    running = True
    highlight_btn = -1  # which button is hovered

    # Build mapping from key char to action
    key_actions = {
        "f": "feed", "t": "add_snack", "p": "play", "s": "sleep",
        "h": "heal", "r": "restart",
    }

    while running:
        regions = draw_all(stdscr, pet, highlight_btn)
        stdscr.refresh()

        try:
            key = stdscr.getch()
        except Exception:
            key = -1

        if key == curses.KEY_MOUSE:
            try:
                _, mx, my, _, bstate = curses.getmouse()
            except Exception:
                mx = my = -1
                bstate = 0

            if bstate & curses.BUTTON1_CLICKED:
                # Check pet click
                if "pet" in regions:
                    py1, px1, py2, px2 = regions["pet"]
                    if py1 <= my <= py2 and px1 <= mx <= px2:
                        pet.play()
                        highlight_btn = -1
                        continue

                # Check button clicks
                for (by1, bx1, by2, bx2, action_key) in regions.get("buttons", []):
                    if by1 <= my <= by2 and bx1 <= mx <= bx2:
                        if action_key == "q":
                            running = False
                        elif action_key == "f":
                            pet.feed()
                        elif action_key == "t":
                            pet.add_snack()
                        elif action_key == "p":
                            pet.play()
                        elif action_key == "s":
                            pet.sleep()
                        elif action_key == "h":
                            pet.heal()
                        elif action_key == "r":
                            delete_save()
                            pet = Pet(name="Buddy")
                            pet.events.add("🔄 A new pet has been born!", CP_MAGENTA)
                        break

            elif bstate & curses.REPORT_MOUSE_POSITION:
                # Hover highlight: check button regions
                highlight_btn = -1
                for i, (by1, bx1, by2, bx2, _) in enumerate(regions.get("buttons", [])):
                    if by1 <= my <= by2 and bx1 <= mx <= bx2:
                        highlight_btn = i
                        break

        elif key != -1:
            ch = chr(key).lower() if 32 <= key < 127 else ""
            if key == ord("q") or key == 27:
                running = False
            elif ch in key_actions:
                action = key_actions[ch]
                if action == "restart":
                    delete_save()
                    pet = Pet(name="Buddy")
                    pet.events.add("🔄 A new pet has been born!", CP_MAGENTA)
                elif action == "add_snack":
                    pet.add_snack()
                else:
                    getattr(pet, action)()
            highlight_btn = -1

        tick_counter += 1
        if tick_counter >= TICK_INTERVAL:
            tick_counter = 0
            pet.tick()

        if pet.stats.alive and pet.stats.age > 0 and pet.stats.age % 10 == 0 and tick_counter == 0:
            save_pet(pet)

        curses.napms(80)

    # Cleanup
    if pet.stats.alive:
        save_pet(pet)


def run():
    """Entry point. Wraps curses with proper error handling."""
    try:
        curses.wrapper(run_curses)
    except KeyboardInterrupt:
        pass
    finally:
        # Try to load and show exit message
        pet = load_pet()
        if pet and pet.stats.alive:
            print(f"\n💾 Pet saved! See you later!")
        else:
            delete_save()
            print(f"\n💀 Your pet has passed away...")


if __name__ == "__main__":
    run()
