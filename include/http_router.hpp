#pragma once

#include "http_metrics.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "middleware.hpp"
#include "note_store.hpp"
#include "rate_limiter.hpp"
#include "server_config.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>

class HttpRouter {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    HttpRouter(std::filesystem::path public_dir, NoteStore& note_store, const ServerConfig& config,
               ServerStats* stats = nullptr);

    HttpResponse route(const HttpRequest& request) const;

private:
    std::filesystem::path public_dir_;
    NoteStore& note_store_;
    ServerConfig config_;
    ServerStats* stats_;
    RateLimiter rate_limiter_;
    MiddlewareChain middlewares_;
    std::unordered_map<std::string, Handler> handlers_;

    void register_routes();
    void register_middlewares();

    static std::string route_key(const std::string& method, const std::string& path);

    HttpResponse handle_health(const HttpRequest& request) const;
    HttpResponse handle_metrics(const HttpRequest& request) const;
    HttpResponse handle_prometheus_metrics(const HttpRequest& request) const;
    HttpResponse handle_time(const HttpRequest& request) const;
    HttpResponse handle_echo(const HttpRequest& request) const;
    HttpResponse handle_me(const HttpRequest& request) const;
    HttpResponse handle_notes_collection(const HttpRequest& request) const;
    HttpResponse handle_note_item(const HttpRequest& request, long long note_id) const;
    HttpResponse serve_static_file(const std::string& path) const;
    HttpResponse metrics_response() const;
    bool is_api_key_valid(const HttpRequest& request) const;
    static std::string guess_content_type(const std::filesystem::path& path);
};
