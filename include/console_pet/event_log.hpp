#pragma once

// EventLog: a small ring buffer of recent, color-tagged messages.
//
// Pure data only (no rendering); the UI draws it. Keeps a bounded number of
// messages so the on-screen log never grows unbounded.

#include <deque>
#include <string>
#include <utility>

#include "console_pet/colors.hpp"

namespace pet {

class EventLog {
public:
    explicit EventLog(int max_lines = 3) : max_lines_(max_lines) {}

    void add(std::string text, Color color = Color::White) {
        messages_.emplace_back(std::move(text), color);
        while (static_cast<int>(messages_.size()) > max_lines_) {
            messages_.pop_front();
        }
    }

    const std::deque<std::pair<std::string, Color>>& messages() const {
        return messages_;
    }

private:
    std::deque<std::pair<std::string, Color>> messages_;
    int max_lines_;
};

}  // namespace pet
