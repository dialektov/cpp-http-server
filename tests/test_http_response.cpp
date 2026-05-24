#include <doctest/doctest.h>

#include "http_response.hpp"

TEST_CASE("build_http_response includes status headers and body") {
    HttpResponse response;
    response.status_code = 200;
    response.status_text = "OK";
    response.content_type = "application/json; charset=utf-8";
    response.body = R"({"status":"ok"})";

    const std::string raw = build_http_response(response);
    CHECK(raw.find("HTTP/1.1 200 OK") != std::string::npos);
    CHECK(raw.find("Content-Type: application/json; charset=utf-8") != std::string::npos);
    CHECK(raw.find("Content-Length: 15") != std::string::npos);
    CHECK(raw.find("Connection: close") != std::string::npos);
    CHECK(raw.find(R"({"status":"ok"})") != std::string::npos);
}

TEST_CASE("build_http_response supports keep-alive") {
    HttpResponse response;
    response.keep_alive = true;
    response.body = "ok";

    const std::string raw = build_http_response(response);
    CHECK(raw.find("Connection: keep-alive") != std::string::npos);
}

TEST_CASE("make_json_response sets json content type") {
    const HttpResponse response = make_json_response(200, "OK", R"({"a":1})");
    CHECK(response.content_type == "application/json; charset=utf-8");
    CHECK(response.body == R"({"a":1})");
}
