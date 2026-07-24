#include "console_pet/llm.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>

#include <nlohmann/json.hpp>

namespace pet {
namespace {

/// Create a mode-600 temp file and return its fd/path (path buffer is mutated
/// by mkstemp). Returns -1 on failure.
int make_temp(char* path_template, std::string& out_path) {
    const int fd = mkstemp(path_template);
    if (fd < 0) return -1;
    fchmod(fd, 0600);
    out_path = path_template;
    return fd;
}

}  // namespace

std::string llm_chat(const LlmConfig& cfg,
                     const std::vector<ChatMessage>& messages) {
    if (!cfg.enabled) return "";

    nlohmann::json msgs = nlohmann::json::array();
    for (const auto& m : messages) {
        msgs.push_back({{"role", m.role}, {"content", m.content}});
    }
    const nlohmann::json body = {
        {"model", cfg.model},
        {"messages", msgs},
        {"max_tokens", 150},
        {"temperature", 0.8},
    };
    const std::string body_str = body.dump();

    // Endpoint: <api_base>/chat/completions
    std::string url = cfg.api_base;
    while (!url.empty() && url.back() == '/') url.pop_back();
    url += "/chat/completions";

    // Write the request body and a curl config (holding the auth header) to
    // mode-600 temp files so the API key never appears in the process list.
    std::string body_path, cfg_path;
    char body_tpl[] = "/tmp/pet_llm_body_XXXXXX";
    char cfg_tpl[] = "/tmp/pet_llm_cfg_XXXXXX";
    const int bfd = make_temp(body_tpl, body_path);
    const int cfd = make_temp(cfg_tpl, cfg_path);
    if (bfd < 0 || cfd < 0) {
        if (bfd >= 0) { close(bfd); unlink(body_path.c_str()); }
        if (cfd >= 0) { close(cfd); unlink(cfg_path.c_str()); }
        return "";
    }
    if (write(bfd, body_str.data(), body_str.size()) < 0) {
        close(bfd); close(cfd);
        unlink(body_path.c_str()); unlink(cfg_path.c_str());
        return "";
    }
    close(bfd);
    {
        std::ofstream cf(cfg_path);
        cf << "url = \"" << url << "\"\n";
        cf << "header = \"Content-Type: application/json\"\n";
        if (!cfg.api_key.empty()) {
            cf << "header = \"Authorization: Bearer " << cfg.api_key << "\"\n";
        }
        cf << "data = @" << body_path << "\n";
    }
    close(cfd);

    const std::string cmd = "curl -s --max-time " +
                            std::to_string(cfg.timeout_sec) + " -K '" +
                            cfg_path + "' 2>/dev/null";
    std::string response;
    if (FILE* pipe = popen(cmd.c_str(), "r")) {
        std::array<char, 4096> buf{};
        std::size_t n = 0;
        while ((n = fread(buf.data(), 1, buf.size(), pipe)) > 0) {
            response.append(buf.data(), n);
        }
        pclose(pipe);
    }
    unlink(body_path.c_str());
    unlink(cfg_path.c_str());

    try {
        const auto j = nlohmann::json::parse(response);
        if (const auto ch = j.find("choices");
            ch != j.end() && ch->is_array() && !ch->empty()) {
            const auto& msg = (*ch)[0]["message"]["content"];
            if (msg.is_string()) return msg.get<std::string>();
        }
    } catch (const nlohmann::json::exception&) {
        // Non-JSON / error response: fall through.
    }
    return "";
}

}  // namespace pet
