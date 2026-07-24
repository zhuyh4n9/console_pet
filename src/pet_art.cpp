#include "console_pet/pet_art.hpp"

#include <array>
#include <cstddef>
#include <sstream>

namespace pet {
namespace {

constexpr std::size_t kMoodCount = 14;
using Frames = std::vector<std::vector<std::string>>;

/// Split a raw art blob into screen lines, dropping blank/whitespace-only
/// lines (matches Python's `[l for l in art.split("\n") if l.strip()]`).
std::vector<std::string> split_art(const char* raw) {
    std::vector<std::string> lines;
    std::istringstream iss(raw);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find_first_not_of(" \t") != std::string::npos) {
            lines.push_back(line);
        }
    }
    return lines;
}

std::size_t idx(Mood m) { return static_cast<std::size_t>(m); }

// All standing-cat moods share one body: rounded torso, little paws, and a
// curled tail on the right. Moods differ by face and a few decorations, which
// keeps the pet visually consistent as its mood changes.

const std::array<Frames, kMoodCount>& all_frames() {
    static const std::array<Frames, kMoodCount> frames = [] {
        std::array<Frames, kMoodCount> f;

        f[idx(Mood::Happy)] = {
            split_art(R"ART(
   /\___/\
  (  o o  )
  (  =^=  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
            split_art(R"ART(
   /\___/\
  (  ^ ^  )
  (  =^=  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Content)] = {
            split_art(R"ART(
   /\___/\
  (  · ·  )
  (  =w=  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Bored)] = {
            split_art(R"ART(
   /\___/\
  (  ¬_¬  )
  (  ...  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Grumpy)] = {
            split_art(R"ART(
   /\___/\
  (  >_<  )
  (  ︿︿︿  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Starving)] = {
            split_art(R"ART(
   /\___/\
  (  @_@  )
  (  > <  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Hungry)] = {
            split_art(R"ART(
   /\___/\
  (  o_o  )
  (  > <  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Sad)] = {
            split_art(R"ART(
   /\___/\
  (  T_T  )
  (  ︵︵︵  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Sleepy)] = {
            split_art(R"ART(
   /\___/\
  (  -_-  )  z
  (  zzz  ) Z
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Playing)] = {
            split_art(R"ART(
   /\___/\
  (  ★_★  )
  (  =^=  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
            split_art(R"ART(
   /\___/\
  (  ★_★  )  ☆
  (  =^=  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Hyper)] = {
            split_art(R"ART(
   /\___/\
  (  ✧_✧  )
  (  =D=  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)  ★
    )ART"),
            split_art(R"ART(
   /\___/\   ✧
  (  ✧_✧  )
  (  =D=  )  ★
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Sick)] = {
            split_art(R"ART(
   /\___/\
  (  x_x  )
  (  ~~~  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Dead)] = {
            split_art(R"ART(
   /\___/\
  (  x_x  )
  (  ---  )
   (_____)
    |   |
   /|   |\
  (_|   |_)
    )ART"),
        };

        f[idx(Mood::Egg)] = {
            split_art(R"ART(
    ,---,
   /     \
  |  · ·  |
  |   ω   |
   \     /
    `---'
    )ART"),
        };

        f[idx(Mood::Working)] = {
            split_art(R"ART(
   /\___/\
  (  •_•  )
  (   ー  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_) 💼
    )ART"),
            split_art(R"ART(
   /\___/\
  (  •_•  )  💦
  (   ー  )
   (_____)
    |   |  ~
   /|   |_/
  (_|   |_)  💼
    )ART"),
        };

        return f;
    }();
    return frames;
}

}  // namespace

const std::vector<std::vector<std::string>>& frames_for(Mood mood) {
    return all_frames()[idx(mood)];
}

const std::vector<std::string>& frame_lines(Mood mood, int frame_idx) {
    const Frames& frames = frames_for(mood);
    if (frames.empty()) {
        static const std::vector<std::string> empty;
        return empty;
    }
    return frames[static_cast<std::size_t>(frame_idx) % frames.size()];
}

}  // namespace pet
