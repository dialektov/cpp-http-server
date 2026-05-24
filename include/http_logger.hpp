#pragma once

#include <chrono>
#include <string>

void log_request(const std::string& method, const std::string& path, int status_code,
                 std::chrono::milliseconds duration, bool structured = false);
