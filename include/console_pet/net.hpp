#pragma once

// Networking: minimal TCP helpers plus newline-delimited JSON messaging used by
// the client/server protocol. Sockets are blocking; callers use poll() to wait
// for readiness before reading a whole message.

#include <string>

#include <nlohmann/json.hpp>

namespace pet {

/// Create a listening TCP socket bound to host:port ("" host = all interfaces).
/// Returns the fd, or -1 on error.
int net_listen(const std::string& host, int port);

/// Accept a connection on a listening fd. Returns the client fd, or -1.
int net_accept(int listen_fd);

/// Connect to host:port. Returns the fd, or -1 on error.
int net_connect(const std::string& host, int port);

/// Send a JSON message followed by a newline. Returns true on success.
bool net_send(int fd, const nlohmann::json& msg);

/// Receive one newline-delimited JSON message. Returns false on EOF/error.
bool net_recv(int fd, nlohmann::json& out);

}  // namespace pet
