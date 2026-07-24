#include "console_pet/config.hpp"

#include <sys/wait.h>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

namespace pet {
namespace {

const char* env_or(const char* name, const char* fallback = nullptr) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}

/// Run a command, capture stdout, and return it ("" on failure). The exit
/// status is reported through `ok`.
std::string run_capture(const std::string& cmd, bool& ok) {
    ok = false;
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return out;
    std::array<char, 1024> buf{};
    std::size_t n = 0;
    while ((n = fread(buf.data(), 1, buf.size(), pipe)) > 0) {
        out.append(buf.data(), n);
    }
    const int status = pclose(pipe);
    ok = (status != -1 && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return out;
}

void parse_llm(const nlohmann::json& j, LlmConfig& llm) {
    const auto l = j.find("llm");
    if (l == j.end() || !l->is_object()) return;
    llm.enabled = l->value("enabled", llm.enabled);
    llm.api_base = l->value("api_base", llm.api_base);
    llm.api_key = l->value("api_key", llm.api_key);
    llm.model = l->value("model", llm.model);
    llm.personality = l->value("personality", llm.personality);
    llm.timeout_sec = l->value("timeout_sec", llm.timeout_sec);
    llm.react = l->value("react", llm.react);
}

/// Candidate config files in priority order.
std::vector<std::string> config_candidates(const std::string& explicit_path) {
    std::vector<std::string> paths;
    if (!explicit_path.empty()) paths.push_back(explicit_path);
    if (const char* p = env_or("PET_CONFIG")) paths.push_back(p);
    paths.push_back(user_config_path());
    paths.push_back("pet_config.json");  // backward-compat / convenience
    return paths;
}

}  // namespace

std::string user_config_path() {
    const char* home = env_or("HOME", "");
    const char* xdg = env_or("XDG_CONFIG_HOME");
    std::string base = xdg ? xdg : (std::string(home) + "/.config");
    return base + "/console_pet/config.json";
}

std::string keyring_api_key() {
    // Freedesktop Secret Service (GNOME Keyring / KWallet) via secret-tool.
    bool ok = false;
    std::string key =
        run_capture("secret-tool lookup application console_pet 2>/dev/null", ok);
    if (!ok) return "";
    while (!key.empty() && (key.back() == '\n' || key.back() == '\r'))
        key.pop_back();
    return key;
}

Config load_config(const std::string& path) {
    Config cfg;

    // Load the first existing candidate file.
    for (const auto& candidate : config_candidates(path)) {
        std::ifstream in(candidate);
        if (!in) continue;
        try {
            const auto j = nlohmann::json::parse(in);
            parse_llm(j, cfg.llm);
            if (const auto s = j.find("server"); s != j.end() && s->is_object()) {
                cfg.host = s->value("host", cfg.host);
                cfg.port = s->value("port", cfg.port);
            }
        } catch (const nlohmann::json::exception&) {
            // Malformed file: keep whatever was parsed / defaults.
        }
        break;
    }

    // Non-secret env overrides.
    if (const char* v = env_or("PET_API_BASE")) cfg.llm.api_base = v;
    if (const char* v = env_or("PET_LLM_MODEL")) cfg.llm.model = v;
    if (const char* v = env_or("PET_PERSONALITY")) cfg.llm.personality = v;
    if (const char* v = env_or("PET_HOST")) cfg.host = v;
    if (const char* v = env_or("PET_PORT")) cfg.port = std::atoi(v);

    // Secret resolution: env -> keyring -> file.
    if (const char* v = env_or("PET_API_KEY")) {
        cfg.llm.api_key = v;
    } else if (std::string kr = keyring_api_key(); !kr.empty()) {
        cfg.llm.api_key = kr;
    }

    return cfg;
}

}  // namespace pet
