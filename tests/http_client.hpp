#pragma once

#include <cstdint>
#include <string>

struct HttpClientResponse {
    int status_code = 0;
    std::string body;
    std::string raw;
};

HttpClientResponse http_get(const std::string& host, std::uint16_t port, const std::string& path);
HttpClientResponse http_request(const std::string& host, std::uint16_t port, const std::string& method,
                                const std::string& path, const std::string& body = "",
                                const std::string& content_type = "application/json",
                                const std::string& api_key = "");
HttpClientResponse http_post(const std::string& host, std::uint16_t port, const std::string& path,
                             const std::string& body,
                             const std::string& content_type = "application/json");
