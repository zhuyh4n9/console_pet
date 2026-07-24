#pragma once

// Minimal client for an OpenAI-compatible chat completions endpoint.
//
// Uses the system `curl` binary (via a mode-600 temp config so the API key
// never appears on the command line). Returns the assistant reply, or an empty
// string on any failure so the caller can fall back to a canned response.

#include <string>
#include <vector>

#include "console_pet/config.hpp"

namespace pet {

struct ChatMessage {
    std::string role;     // "system" | "user" | "assistant"
    std::string content;
};

/// Send `messages` to the configured endpoint and return the assistant's reply
/// ("" on failure or when disabled).
std::string llm_chat(const LlmConfig& cfg,
                     const std::vector<ChatMessage>& messages);

}  // namespace pet
