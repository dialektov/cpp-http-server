#include "http_router.hpp"

#include "http_prometheus.hpp"
#include "json_utils.hpp"

#include <chrono>
#include <fstream>
#include <sstream>

namespace {

std::string extension_to_mime(const std::string& extension) {
    if (extension == ".html" || extension == ".htm") return "text/html; charset=utf-8";
    if (extension == ".css") return "text/css; charset=utf-8";
    if (extension == ".js") return "application/javascript; charset=utf-8";
    if (extension == ".json") return "application/json; charset=utf-8";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".txt") return "text/plain; charset=utf-8";
    return "application/octet-stream";
}

bool header_contains_json(const HttpRequest& request) {
    const auto it = request.headers.find("content-type");
    if (it == request.headers.end()) {
        return false;
    }
    return it->second.find("application/json") != std::string::npos;
}

}  // namespace

HttpRouter::HttpRouter(std::filesystem::path public_dir, NoteStore& note_store,
                       const ServerConfig& config, ServerStats* stats)
    : public_dir_(std::move(public_dir)),
      note_store_(note_store),
      config_(config),
      stats_(stats),
      rate_limiter_(config.rate_limit_rpm) {
    register_middlewares();
    register_routes();
}

std::string HttpRouter::route_key(const std::string& method, const std::string& path) {
    return method + ' ' + path;
}

void HttpRouter::register_middlewares() {
    middlewares_.use(make_rate_limit_middleware(rate_limiter_));
}

void HttpRouter::register_routes() {
    handlers_[route_key("GET", "/health")] =
        [this](const HttpRequest& request) { return handle_health(request); };
    handlers_[route_key("GET", "/metrics")] =
        [this](const HttpRequest& request) { return handle_metrics(request); };
    handlers_[route_key("GET", "/metrics/prometheus")] =
        [this](const HttpRequest& request) { return handle_prometheus_metrics(request); };
    handlers_[route_key("GET", "/api/time")] =
        [this](const HttpRequest& request) { return handle_time(request); };
    handlers_[route_key("GET", "/api/me")] =
        [this](const HttpRequest& request) { return handle_me(request); };
    handlers_[route_key("POST", "/api/echo")] =
        [this](const HttpRequest& request) { return handle_echo(request); };
    handlers_[route_key("GET", "/api/notes")] =
        [this](const HttpRequest& request) { return handle_notes_collection(request); };
    handlers_[route_key("POST", "/api/notes")] =
        [this](const HttpRequest& request) { return handle_notes_collection(request); };
}

HttpResponse HttpRouter::route(const HttpRequest& request) const {
    if (stats_) {
        stats_->requests_total.fetch_add(1, std::memory_order_relaxed);
    }

    if (auto blocked = middlewares_.run(request)) {
        return *blocked;
    }

    const auto exact = handlers_.find(route_key(request.method, request.path));
    if (exact != handlers_.end()) {
        return exact->second(request);
    }

    if (const auto note_id = parse_note_id(request.path)) {
        return handle_note_item(request, *note_id);
    }

    if (request.method == "GET") {
        std::string path = request.path;
        if (path == "/") {
            path = "/index.html";
        }
        return serve_static_file(path);
    }

    return make_text_response(404, "Not Found", "Route not found");
}

bool HttpRouter::is_api_key_valid(const HttpRequest& request) const {
    const auto api_key_header = request.headers.find("x-api-key");
    if (api_key_header != request.headers.end() && api_key_header->second == config_.api_key) {
        return true;
    }

    const auto authorization = request.headers.find("authorization");
    if (authorization != request.headers.end()) {
        const std::string prefix = "Bearer ";
        if (authorization->second.rfind(prefix, 0) == 0 &&
            authorization->second.substr(prefix.size()) == config_.api_key) {
            return true;
        }
    }

    return false;
}

HttpResponse HttpRouter::handle_health(const HttpRequest&) const {
    if (!note_store_.ping()) {
        return make_json_response(503, "Service Unavailable",
                                  R"({"status":"degraded","database":"unavailable"})");
    }
    return make_json_response(200, "OK", R"({"status":"ok","database":"ok"})");
}

HttpResponse HttpRouter::handle_metrics(const HttpRequest&) const {
    return metrics_response();
}

HttpResponse HttpRouter::handle_prometheus_metrics(const HttpRequest&) const {
    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.content_type = "text/plain; version=0.0.4; charset=utf-8";
    response.body = stats_ ? format_prometheus_metrics(*stats_) : format_prometheus_metrics({});
    return response;
}

HttpResponse HttpRouter::handle_time(const HttpRequest&) const {
    const auto now = std::chrono::system_clock::now();
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    return make_json_response(200, "OK", R"({"unix_time":)" + std::to_string(seconds) + "}");
}

HttpResponse HttpRouter::handle_me(const HttpRequest& request) const {
    if (!is_api_key_valid(request)) {
        return make_json_response(401, "Unauthorized", R"({"error":"invalid api key"})");
    }

    return make_json_response(200, "OK", R"({"service":"cpp-http-server","authenticated":true})");
}

HttpResponse HttpRouter::handle_echo(const HttpRequest& request) const {
    if (!header_contains_json(request)) {
        return make_json_response(415, "Unsupported Media Type",
                                  R"({"error":"Content-Type must be application/json"})");
    }
    if (request.body.empty()) {
        return make_json_response(400, "Bad Request", R"({"error":"empty body"})");
    }

    return make_json_response(200, "OK", R"({"echo":)" + request.body + "}");
}

HttpResponse HttpRouter::handle_notes_collection(const HttpRequest& request) const {
    if (request.method == "GET") {
        return make_json_response(200, "OK", notes_to_json(note_store_.list_notes()));
    }

    if (request.method != "POST") {
        return make_text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (!header_contains_json(request)) {
        return make_json_response(415, "Unsupported Media Type",
                                  R"({"error":"Content-Type must be application/json"})");
    }

    const auto title = json_get_string(request.body, "title");
    if (!title.has_value() || title->empty()) {
        return make_json_response(400, "Bad Request", R"({"error":"title is required"})");
    }

    const auto content = json_get_string(request.body, "content").value_or("");
    const Note created = note_store_.create_note(*title, content);
    return make_json_response(201, "Created", note_to_json(created));
}

HttpResponse HttpRouter::handle_note_item(const HttpRequest& request, long long note_id) const {
    if (request.method == "GET") {
        const auto note = note_store_.get_note(note_id);
        if (!note.has_value()) {
            return make_json_response(404, "Not Found", R"({"error":"note not found"})");
        }
        return make_json_response(200, "OK", note_to_json(*note));
    }

    if (request.method == "PATCH") {
        if (!header_contains_json(request)) {
            return make_json_response(415, "Unsupported Media Type",
                                      R"({"error":"Content-Type must be application/json"})");
        }

        const auto title = json_get_string(request.body, "title");
        const auto content = json_get_string(request.body, "content");
        if (!title.has_value() && !content.has_value()) {
            return make_json_response(400, "Bad Request", R"({"error":"nothing to update"})");
        }

        const auto updated = note_store_.update_note(note_id, title, content);
        if (!updated.has_value()) {
            return make_json_response(404, "Not Found", R"({"error":"note not found"})");
        }
        return make_json_response(200, "OK", note_to_json(*updated));
    }

    if (request.method == "DELETE") {
        if (!note_store_.delete_note(note_id)) {
            return make_json_response(404, "Not Found", R"({"error":"note not found"})");
        }
        return make_json_response(200, "OK", R"({"deleted":true})");
    }

    return make_text_response(405, "Method Not Allowed", "Method not allowed");
}

HttpResponse HttpRouter::metrics_response() const {
    const std::uint64_t total = stats_ ? stats_->requests_total.load(std::memory_order_relaxed) : 0;
    const std::uint64_t active =
        stats_ ? stats_->active_connections.load(std::memory_order_relaxed) : 0;
    const auto uptime = stats_
        ? std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - stats_->started_at).count()
        : 0;

    const std::string body = R"({"requests_total":)" + std::to_string(total) +
                             R"(,"active_connections":)" + std::to_string(active) +
                             R"(,"uptime_seconds":)" + std::to_string(uptime) + "}";
    return make_json_response(200, "OK", body);
}

HttpResponse HttpRouter::serve_static_file(const std::string& path) const {
    if (path.find("..") != std::string::npos) {
        return make_text_response(403, "Forbidden", "Path traversal is not allowed");
    }

    const std::filesystem::path file_path = public_dir_ / std::filesystem::path(path.substr(1));
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        return make_text_response(404, "Not Found", "File not found");
    }

    std::ifstream input(file_path, std::ios::binary);
    if (!input) {
        return make_text_response(500, "Internal Server Error", "Failed to read file");
    }

    std::ostringstream contents;
    contents << input.rdbuf();

    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.content_type = guess_content_type(file_path);
    response.body = contents.str();
    return response;
}

std::string HttpRouter::guess_content_type(const std::filesystem::path& path) {
    return extension_to_mime(path.extension().string());
}
