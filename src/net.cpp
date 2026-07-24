#include "console_pet/net.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

namespace pet {

int net_listen(const std::string& host, int port) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo* res = nullptr;
    const std::string port_str = std::to_string(port);
    if (getaddrinfo(host.empty() ? nullptr : host.c_str(), port_str.c_str(),
                    &hints, &res) != 0) {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        if (bind(fd, p->ai_addr, p->ai_addrlen) == 0 && listen(fd, 8) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

int net_accept(int listen_fd) { return accept(listen_fd, nullptr, nullptr); }

int net_connect(const std::string& host, int port) {
    struct addrinfo hints {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* res = nullptr;
    const std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0) {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo* p = res; p != nullptr; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

bool net_send(int fd, const nlohmann::json& msg) {
    const std::string data = msg.dump() + "\n";
    std::size_t off = 0;
    while (off < data.size()) {
        const ssize_t n = write(fd, data.data() + off, data.size() - off);
        if (n <= 0) {
            if (errno == EINTR) continue;
            return false;
        }
        off += static_cast<std::size_t>(n);
    }
    return true;
}

bool net_recv(int fd, nlohmann::json& out) {
    std::string line;
    char c = 0;
    while (true) {
        const ssize_t n = read(fd, &c, 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;  // peer closed
        if (c == '\n') break;
        line += c;
    }
    if (line.empty()) return false;
    try {
        out = nlohmann::json::parse(line);
    } catch (const nlohmann::json::exception&) {
        return false;
    }
    return true;
}

}  // namespace pet
