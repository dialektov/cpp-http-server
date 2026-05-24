#include "http_server.hpp"

#include "http_logger.hpp"
#include "shutdown.hpp"

#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

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
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
constexpr socket_t kInvalidSocket = -1;
inline int close_socket(socket_t socket) { return close(socket); }
#endif

namespace {

class SocketLibrary {
public:
    SocketLibrary() {
#ifdef _WIN32
        WSADATA wsa_data{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~SocketLibrary() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

SocketLibrary g_socket_library;

std::string client_ip_from_address(const sockaddr_in& address) {
    char buffer[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET, const_cast<in_addr*>(&address.sin_addr), buffer, sizeof(buffer));
    return buffer;
}

}  // namespace

HttpServer::HttpServer(ServerConfig config)
    : config_(std::move(config)),
      note_store_(config_.database_url),
      router_(config_.public_dir, note_store_, config_, &stats_),
      thread_pool_(std::make_unique<ThreadPool>(config_.thread_pool_size)) {}

HttpServer::~HttpServer() {
    if (listen_socket_ != -1) {
        close_socket(static_cast<socket_t>(listen_socket_));
        listen_socket_ = -1;
    }
}

void HttpServer::setup_socket() {
    listen_socket_ = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (listen_socket_ == static_cast<int>(kInvalidSocket)) {
        throw std::runtime_error("Failed to create socket");
    }

    int reuse = 1;
    setsockopt(static_cast<socket_t>(listen_socket_), SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(config_.port);

    if (config_.host == "0.0.0.0" || config_.host.empty()) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, config_.host.c_str(), &address.sin_addr) != 1) {
        throw std::runtime_error("Invalid host address: " + config_.host);
    }

    if (bind(static_cast<socket_t>(listen_socket_),
             reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        throw std::runtime_error("Failed to bind socket to port " + std::to_string(config_.port));
    }

    if (listen(static_cast<socket_t>(listen_socket_), SOMAXCONN) != 0) {
        throw std::runtime_error("Failed to listen on socket");
    }
}

void HttpServer::run() {
    if (!std::filesystem::exists(config_.public_dir)) {
        throw std::runtime_error("Public directory does not exist: " + config_.public_dir.string());
    }

    setup_socket();
    install_shutdown_handlers();
    std::cout << "Server listening on http://" << config_.host << ':' << config_.port << '\n';
    std::cout << "Thread pool size: " << config_.thread_pool_size << '\n';
    std::cout << "Press Ctrl+C to stop gracefully\n";
    accept_loop();
}

void HttpServer::serve(std::size_t max_connections) {
    if (!std::filesystem::exists(config_.public_dir)) {
        throw std::runtime_error("Public directory does not exist: " + config_.public_dir.string());
    }

    setup_socket();

    for (std::size_t i = 0; i < max_connections; ++i) {
        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);
        socket_t client_socket = accept(static_cast<socket_t>(listen_socket_),
                                        reinterpret_cast<sockaddr*>(&client_address),
                                        &client_length);
        if (client_socket == kInvalidSocket) {
            throw std::runtime_error("accept() failed during test serve");
        }
        const std::string client_ip = client_ip_from_address(client_address);
        handle_client(static_cast<int>(client_socket), client_ip);
    }
}

void HttpServer::accept_loop() {
    const socket_t listen_socket = static_cast<socket_t>(listen_socket_);

    while (!shutdown_requested()) {
        fd_set read_set{};
        FD_ZERO(&read_set);
        FD_SET(listen_socket, &read_set);

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        const int ready = select(static_cast<int>(listen_socket + 1), &read_set, nullptr, nullptr, &timeout);
        if (ready < 0) {
            if (shutdown_requested()) {
                break;
            }
            std::cerr << "select() failed\n";
            continue;
        }
        if (ready == 0 || shutdown_requested()) {
            continue;
        }

        sockaddr_in client_address{};
        socklen_t client_length = sizeof(client_address);
        socket_t client_socket =
            accept(listen_socket, reinterpret_cast<sockaddr*>(&client_address), &client_length);
        if (client_socket == kInvalidSocket) {
            if (shutdown_requested()) {
                break;
            }
            std::cerr << "accept() failed\n";
            continue;
        }

        const std::string client_ip = client_ip_from_address(client_address);

        if (shutdown_requested()) {
            close_socket(client_socket);
            break;
        }

        thread_pool_->enqueue([this, client_socket = static_cast<int>(client_socket), client_ip]() {
            handle_client(client_socket, client_ip);
        });
    }

    std::cout << "Shutting down, waiting for pending requests...\n";
    thread_pool_.reset();
    std::cout << "Server stopped.\n";
}

std::string HttpServer::read_request(int client_socket) {
    std::string buffer;
    std::array<char, 4096> chunk{};

    while (buffer.find("\r\n\r\n") == std::string::npos) {
        const int received = recv(static_cast<socket_t>(client_socket), chunk.data(),
                                  static_cast<int>(chunk.size()), 0);
        if (received <= 0) {
            break;
        }
        buffer.append(chunk.data(), received);
        if (buffer.size() > 65536) {
            break;
        }
    }

    const auto content_length = read_content_length(buffer);
    if (content_length.has_value()) {
        const auto header_end = buffer.find("\r\n\r\n");
        const std::size_t body_start = header_end + 4;
        while (buffer.size() - body_start < *content_length) {
            const int received = recv(static_cast<socket_t>(client_socket), chunk.data(),
                                      static_cast<int>(chunk.size()), 0);
            if (received <= 0) {
                break;
            }
            buffer.append(chunk.data(), received);
            if (buffer.size() > 65536) {
                break;
            }
        }
    }

    return buffer;
}

void HttpServer::send_response(int client_socket, const HttpResponse& response) {
    const std::string raw = build_http_response(response);
    send(static_cast<socket_t>(client_socket), raw.data(), static_cast<int>(raw.size()), 0);
}

bool HttpServer::request_wants_keep_alive(const HttpRequest& request) {
    const auto connection = request.headers.find("connection");
    if (connection == request.headers.end()) {
        return false;
    }

    std::string value = connection->second;
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value.find("keep-alive") != std::string::npos;
}

void HttpServer::handle_client(int client_socket, const std::string& client_ip) {
    const socket_t socket = static_cast<socket_t>(client_socket);
    stats_.active_connections.fetch_add(1, std::memory_order_relaxed);

    while (true) {
        const auto started_at = std::chrono::steady_clock::now();
        const std::string raw_request = read_request(client_socket);
        if (raw_request.empty()) {
            break;
        }

        HttpResponse response = make_text_response(400, "Bad Request", "Invalid HTTP request");
        std::string method = "-";
        std::string path = "-";
        bool keep_alive = false;

        if (auto request = parse_http_request(raw_request)) {
            request->client_ip = client_ip;
            method = request->method;
            path = request->path;
            keep_alive = request_wants_keep_alive(*request);
            response = router_.route(*request);
            response.keep_alive = keep_alive;
        }

        send_response(client_socket, response);

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started_at);
        log_request(method, path, response.status_code, elapsed, config_.structured_logs);

        if (!response.keep_alive || shutdown_requested()) {
            break;
        }
    }

    close_socket(socket);
    stats_.active_connections.fetch_sub(1, std::memory_order_relaxed);
}
