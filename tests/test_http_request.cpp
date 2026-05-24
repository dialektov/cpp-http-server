#include <doctest/doctest.h>

#include "http_request.hpp"

TEST_CASE("parse_http_request parses method path and version") {
    const std::string raw =
        "GET /health HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "\r\n";

    const auto request = parse_http_request(raw);
    REQUIRE(request.has_value());
    CHECK(request->method == "GET");
    CHECK(request->path == "/health");
    CHECK(request->version == "HTTP/1.1");
}

TEST_CASE("parse_http_request parses headers case-insensitively") {
    const std::string raw =
        "GET /api/time HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "X-Custom-Header: test\r\n"
        "\r\n";

    const auto request = parse_http_request(raw);
    REQUIRE(request.has_value());
    CHECK(request->headers.at("content-type") == "application/json");
    CHECK(request->headers.at("x-custom-header") == "test");
}

TEST_CASE("parse_http_request rejects malformed input") {
    CHECK_FALSE(parse_http_request("").has_value());
    CHECK_FALSE(parse_http_request("INVALID REQUEST\r\n\r\n").has_value());
    CHECK_FALSE(parse_http_request("GET /only-two-parts\r\n\r\n").has_value());
}

TEST_CASE("parse_http_request reads JSON body using Content-Length") {
    const std::string raw =
        "POST /api/echo HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 12\r\n"
        "\r\n"
        R"({"msg":"hi"})";

    const auto request = parse_http_request(raw);
    REQUIRE(request.has_value());
    CHECK(request->method == "POST");
    CHECK(request->body == R"({"msg":"hi"})");
}

TEST_CASE("read_content_length extracts header value") {
    const std::string raw =
        "POST /api/echo HTTP/1.1\r\n"
        "Content-Length: 4\r\n"
        "\r\n"
        "test";

    const auto length = read_content_length(raw);
    REQUIRE(length.has_value());
    CHECK(*length == 4);
}
