#pragma once

// Brain: runs LLM requests on a background thread so the server never blocks.
//
// The server submits fully-built message lists tagged with a "kind"
// (reaction vs. chat); the worker calls the LLM and stores the reply, which the
// server polls and applies (speech bubble or chat transcript).

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "console_pet/config.hpp"
#include "console_pet/llm.hpp"
#include "console_pet/mood.hpp"
#include "console_pet/pet_stats.hpp"

namespace pet {

/// Kinds of LLM tasks the server can submit.
enum class LlmKind { Reaction = 0, Chat = 1 };

/// A lightweight snapshot of the pet, used to build a mood-aware prompt.
struct PetSnapshot {
    std::string name;
    PetStats stats;
    int sleeping_ticks = 0;
    int working_ticks = 0;
    int frame_idx = 0;
    Mood prev_mood = Mood::Egg;
    int coins = 0;
};

/// Build the system prompt describing the pet's identity, personality, and
/// current state.
std::string build_system_prompt(const PetSnapshot& snap,
                                const std::string& personality);

/// Result of a completed LLM task.
struct LlmResult {
    std::string text;
    LlmKind kind = LlmKind::Reaction;
};

/// Non-blocking background LLM worker.
class Brain {
public:
    explicit Brain(LlmConfig cfg);
    ~Brain();

    Brain(const Brain&) = delete;
    Brain& operator=(const Brain&) = delete;

    /// True when the LLM is enabled (reactions/chat will be generated).
    bool enabled() const { return enabled_; }

    /// Queue an LLM request. Non-blocking; the queue is kept short so replies
    /// stay fresh.
    void submit(std::vector<ChatMessage> messages, LlmKind kind);

    /// Fetch one completed result (returns false if none ready).
    bool poll(LlmResult& out);

private:
    void worker_loop();

    LlmConfig cfg_;
    bool enabled_ = false;

    std::mutex mtx_;
    std::condition_variable cv_;
    std::deque<std::vector<ChatMessage>> tasks_;
    std::deque<LlmKind> task_kinds_;
    std::deque<LlmResult> results_;
    bool stop_ = false;

    std::thread worker_;
};

}  // namespace pet
