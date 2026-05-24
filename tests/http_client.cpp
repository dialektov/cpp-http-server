#include "http_client.hpp"

#include <array>
#include <sstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
constexpr socket_t kInvalidSocket = INVALID_SOCKET;
inline int close_socket(socket_t socket) { return closesocket(socket); }
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
inline int close_socket(socket_t socket) { return close(socket); }
#endif

namespace {

class ClientSocketLibrary {
public:
    ClientSocketLibrary() {
#ifdef _WIN32
        WSADATA wsa_data{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            throw std::runtime_error("WSAStartup failed in test client");
        }
#endif
    }

    ~ClientSocketLibrary() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

ClientSocketLibrary g_client_socket_library;

HttpClientResponse send_request(const std::string& host, std::uint16_t port,
                                const std::string& request) {
    socket_t client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == kInvalidSocket) {
        throw std::runtime_error("Failed to create client socket");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
        close_socket(client_socket);
        throw std::runtime_error("Invalid host for test client: " + host);
    }

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(client_socket);
        throw std::runtime_error("Failed to connect to " + host + ':' + std::to_string(port));
    }

    send(client_socket, request.data(), static_cast<int>(request.size()), 0);

    std::string raw;
    std::array<char, 4096> chunk{};
    while (true) {
        const int received = recv(client_socket, chunk.data(), static_cast<int>(chunk.size()), 0);
        if (received <= 0) {
            break;
        }
        raw.append(chunk.data(), received);
    }
    close_socket(client_socket);

    HttpClientResponse response;
    response.raw = raw;

    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return response;
    }

    std::istringstream status_line(raw.substr(0, raw.find("\r\n")));
    std::string http_version;
    std::string status_text;
    status_line >> http_version >> response.status_code >> status_text;
    response.body = raw.substr(header_end + 4);
    return response;
}

}  // namespace

HttpClientResponse http_get(const std::string& host, std::uint16_t port, const std::string& path) {
    return http_request(host, port, "GET", path);
}

HttpClientResponse http_request(const std::string& host, std::uint16_t port, const std::string& method,
                                const std::string& path, const std::string& body,
                                const std::string& content_type, const std::string& api_key) {
    std::string request = method + " " + path + " HTTP/1.1\r\nHost: " + host + "\r\n";
    if (!api_key.empty()) {
        request += "X-API-Key: " + api_key + "\r\n";
    }
    if (!body.empty()) {
        request += "Content-Type: " + content_type + "\r\n";
        request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    }
    request += "Connection: close\r\n\r\n";
    request += body;
    return send_request(host, port, request);
}

HttpClientResponse http_post(const std::string& host, std::uint16_t port, const std::string& path,
                             const std::string& body, const std::string& content_type) {
    return http_request(host, port, "POST", path, body, content_type);
}
