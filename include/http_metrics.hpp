#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

struct ServerStats {
    std::atomic<std::uint64_t> requests_total{0};
    std::atomic<std::uint64_t> active_connections{0};
    std::chrono::steady_clock::time_point started_at{std::chrono::steady_clock::now()};
};
