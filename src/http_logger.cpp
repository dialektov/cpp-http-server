#include "http_logger.hpp"

#include "json_utils.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

void log_request(const std::string& method, const std::string& path, int status_code,
                 std::chrono::milliseconds duration, bool structured) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t timestamp = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};
#ifdef _WIN32
    localtime_s(&local_time, &timestamp);
#else
    localtime_r(&timestamp, &local_time);
#endif

    std::ostringstream out;
    if (structured) {
        out << R"({"time":")" << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << R"(","method":")"
            << json_escape(method) << R"(","path":")" << json_escape(path) << R"(","status":)"
            << status_code << R"(,"duration_ms":)" << duration.count() << "}\n";
    } else {
        out << '[' << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S") << "] " << method << ' ' << path
            << ' ' << status_code << ' ' << duration.count() << "ms\n";
    }

    std::cout << out.str();
}
