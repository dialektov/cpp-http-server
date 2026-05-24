#include "http_prometheus.hpp"

#include <chrono>
#include <sstream>

std::string format_prometheus_metrics(const ServerStats& stats) {
    const std::uint64_t total = stats.requests_total.load(std::memory_order_relaxed);
    const std::uint64_t active =
        stats.active_connections.load(std::memory_order_relaxed);
    const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - stats.started_at).count();

    std::ostringstream out;
    out << "# HELP http_requests_total Total HTTP requests processed\n";
    out << "# TYPE http_requests_total counter\n";
    out << "http_requests_total " << total << '\n';
    out << "# HELP http_active_connections Current in-flight HTTP connections\n";
    out << "# TYPE http_active_connections gauge\n";
    out << "http_active_connections " << active << '\n';
    out << "# HELP process_uptime_seconds Process uptime in seconds\n";
    out << "# TYPE process_uptime_seconds gauge\n";
    out << "process_uptime_seconds " << uptime << '\n';
    return out.str();
}
