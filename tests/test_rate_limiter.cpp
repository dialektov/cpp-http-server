#include <doctest/doctest.h>

#include "http_request.hpp"
#include "rate_limiter.hpp"

TEST_CASE("RateLimiter blocks after max requests per minute") {
    RateLimiter limiter(2);
    CHECK(limiter.allow("127.0.0.1"));
    CHECK(limiter.allow("127.0.0.1"));
    CHECK_FALSE(limiter.allow("127.0.0.1"));
    CHECK(limiter.allow("127.0.0.2"));
}

TEST_CASE("rate limit middleware returns 429") {
    RateLimiter limiter(1);
    const Middleware middleware = make_rate_limit_middleware(limiter);

    HttpRequest request;
    request.client_ip = "127.0.0.1";
    CHECK_FALSE(middleware(request).has_value());
    const auto blocked = middleware(request);
    REQUIRE(blocked.has_value());
    CHECK(blocked->status_code == 429);
}
