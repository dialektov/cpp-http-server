#pragma once

#include <optional>
#include <string>
#include <unordered_map>

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::string body;
    std::string client_ip;
    std::unordered_map<std::string, std::string> headers;
};

std::optional<HttpRequest> parse_http_request(const std::string& raw);
std::optional<std::size_t> read_content_length(const std::string& raw_headers);
