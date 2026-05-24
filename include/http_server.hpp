#pragma once

#include "http_metrics.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "http_router.hpp"
#include "note_store.hpp"
#include "server_config.hpp"
#include "thread_pool.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class HttpServer {
public:
    explicit HttpServer(ServerConfig config);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    void run();
    void serve(std::size_t max_connections);

private:
    ServerConfig config_;
    ServerStats stats_;
    NoteStore note_store_;
    HttpRouter router_;
    std::unique_ptr<ThreadPool> thread_pool_;
    int listen_socket_ = -1;

    void setup_socket();
    void accept_loop();
    void handle_client(int client_socket, const std::string& client_ip);
    static std::string read_request(int client_socket);
    static void send_response(int client_socket, const HttpResponse& response);
    static bool request_wants_keep_alive(const HttpRequest& request);
};
