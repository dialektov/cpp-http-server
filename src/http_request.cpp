#include "http_request.hpp"

#include <sstream>

namespace {

std::string trim(std::string value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string to_lower(std::string value) {
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

}  // namespace

std::optional<std::size_t> read_content_length(const std::string& raw_headers) {
    const auto header_end = raw_headers.find("\r\n\r\n");
    const std::string headers =
        header_end == std::string::npos ? raw_headers : raw_headers.substr(0, header_end);

    std::istringstream stream(headers);
    std::string line;
    std::getline(stream, line);

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        const std::string key = to_lower(trim(line.substr(0, colon)));
        if (key != "content-length") {
            continue;
        }

        const std::string value = trim(line.substr(colon + 1));
        if (value.empty()) {
            return std::nullopt;
        }

        try {
            const auto length = std::stoull(value);
            return static_cast<std::size_t>(length);
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<HttpRequest> parse_http_request(const std::string& raw) {
    const auto header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return std::nullopt;
    }

    std::istringstream stream(raw.substr(0, header_end));
    HttpRequest request;

    std::string request_line;
    if (!std::getline(stream, request_line)) {
        return std::nullopt;
    }

    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    {
        std::istringstream line_stream(request_line);
        if (!(line_stream >> request.method >> request.path >> request.version)) {
            return std::nullopt;
        }
    }

    std::string header_line;
    while (std::getline(stream, header_line)) {
        if (!header_line.empty() && header_line.back() == '\r') {
            header_line.pop_back();
        }
        if (header_line.empty()) {
            continue;
        }

        const auto colon = header_line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        std::string key = to_lower(trim(header_line.substr(0, colon)));
        std::string value = trim(header_line.substr(colon + 1));
        request.headers[key] = value;
    }

    const auto content_length = read_content_length(raw);
    if (content_length.has_value()) {
        const std::size_t body_start = header_end + 4;
        if (raw.size() < body_start + *content_length) {
            return std::nullopt;
        }
        request.body = raw.substr(body_start, *content_length);
    }

    return request;
}
