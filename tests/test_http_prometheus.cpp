#include <doctest/doctest.h>

#include "http_prometheus.hpp"
#include "http_metrics.hpp"

TEST_CASE("format_prometheus_metrics renders counter and gauge") {
    ServerStats stats;
    stats.requests_total.store(7, std::memory_order_relaxed);

    const std::string body = format_prometheus_metrics(stats);
    CHECK(body.find("# HELP http_requests_total") != std::string::npos);
    CHECK(body.find("# TYPE http_requests_total counter") != std::string::npos);
    CHECK(body.find("http_requests_total 7") != std::string::npos);
    CHECK(body.find("# TYPE process_uptime_seconds gauge") != std::string::npos);
    CHECK(body.find("process_uptime_seconds ") != std::string::npos);
    CHECK(body.find("http_active_connections") != std::string::npos);
}
