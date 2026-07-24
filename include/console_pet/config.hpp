#pragma once

// Configuration for optional features (currently: LLM chat).
//
// The config file lives in the user's config directory (not the project), and
// the API key is resolved from, in order: the PET_API_KEY environment variable,
// the system keyring (via secret-tool), then the config file. This keeps
// secrets out of the repo/working directory.

#include <string>

namespace pet {

struct LlmConfig {
    bool enabled = false;
    std::string api_base = "https://api.openai.com/v1";  // OpenAI-compatible
    std::string api_key;
    std::string model = "gpt-4o-mini";
    std::string personality = "a cheerful, curious, and playful cat";
    int timeout_sec = 30;
    bool react = true;  // pet reacts in-character to interactions (needs LLM)
};

struct Config {
    LlmConfig llm;
    std::string host = "127.0.0.1";  // server bind / client connect host
    int port = 50777;                // server bind / client connect port
};

/// Primary user config path, e.g. ~/.config/console_pet/config.json.
std::string user_config_path();

/// Load configuration.
///
/// Reads the first existing file among: an explicit `path` (if given), the
/// PET_CONFIG env var, the user config path, then ./pet_config.json. Non-secret
/// fields can be overridden by env (PET_API_BASE, PET_LLM_MODEL,
/// PET_PERSONALITY). The API key is resolved from PET_API_KEY, then the
/// keyring, then the file.
Config load_config(const std::string& path = "");

/// Look up the API key in the system keyring ("" if unavailable/unset).
std::string keyring_api_key();

}  // namespace pet
