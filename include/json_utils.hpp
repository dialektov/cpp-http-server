#pragma once

#include <optional>
#include <string>

std::string json_escape(const std::string& value);
std::optional<std::string> json_get_string(const std::string& body, const std::string& key);
std::optional<long long> parse_note_id(const std::string& path);
