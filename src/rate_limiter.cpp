#include "rate_limiter.hpp"

#include "http_response.hpp"
#include "middleware.hpp"

RateLimiter::RateLimiter(std::size_t max_requests_per_minute)
    : max_requests_per_minute_(max_requests_per_minute) {}

bool RateLimiter::allow(const std::string& client_key) {
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex_);

    Window& window = windows_[client_key];
    if (window.reset_at == std::chrono::steady_clock::time_point{}) {
        window.reset_at = now + std::chrono::minutes(1);
    }
    if (now >= window.reset_at) {
        window.count = 0;
        window.reset_at = now + std::chrono::minutes(1);
    }

    if (window.count >= max_requests_per_minute_) {
        return false;
    }

    ++window.count;
    return true;
}

Middleware make_rate_limit_middleware(RateLimiter& limiter) {
    return [&limiter](const HttpRequest& request) -> std::optional<HttpResponse> {
        if (request.path == "/health" || request.path.rfind("/metrics", 0) == 0) {
            return std::nullopt;
        }

        const std::string client_key = request.client_ip.empty() ? "unknown" : request.client_ip;
        if (!limiter.allow(client_key)) {
            return make_json_response(429, "Too Many Requests",
                                      R"({"error":"rate limit exceeded"})");
        }
        return std::nullopt;
    };
}
