#include "json_utils.hpp"

#include <cctype>
#include <stdexcept>

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '"':
                escaped += "\\\"";
                break;
            case '\\':
                escaped += "\\\\";
                break;
            case '\n':
                escaped += "\\n";
                break;
            case '\r':
                escaped += "\\r";
                break;
            case '\t':
                escaped += "\\t";
                break;
            default:
                escaped += ch;
                break;
        }
    }
    return escaped;
}

std::optional<std::string> json_get_string(const std::string& body, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const auto key_pos = body.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    std::size_t pos = key_pos + needle.size();
    while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos]))) {
        ++pos;
    }
    if (pos >= body.size() || body[pos] != '"') {
        return std::nullopt;
    }
    ++pos;

    std::string value;
    while (pos < body.size()) {
        const char ch = body[pos++];
        if (ch == '"') {
            return value;
        }
        if (ch == '\\' && pos < body.size()) {
            value += body[pos++];
            continue;
        }
        value += ch;
    }

    return std::nullopt;
}

std::optional<long long> parse_note_id(const std::string& path) {
    const std::string prefix = "/api/notes/";
    if (path.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    const std::string id_part = path.substr(prefix.size());
    if (id_part.empty() || id_part.find('/') != std::string::npos) {
        return std::nullopt;
    }

    try {
        std::size_t consumed = 0;
        const long long id = std::stoll(id_part, &consumed);
        if (consumed != id_part.size() || id <= 0) {
            return std::nullopt;
        }
        return id;
    } catch (...) {
        return std::nullopt;
    }
}
