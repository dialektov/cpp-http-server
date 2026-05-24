#pragma once

#include "middleware.hpp"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

class RateLimiter {
public:
    explicit RateLimiter(std::size_t max_requests_per_minute);

    bool allow(const std::string& client_key);

private:
    struct Window {
        std::size_t count = 0;
        std::chrono::steady_clock::time_point reset_at{};

        explicit Window() = default;
    };

    std::size_t max_requests_per_minute_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Window> windows_;
};

Middleware make_rate_limit_middleware(RateLimiter& limiter);
