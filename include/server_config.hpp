#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct ServerConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 8080;
    std::filesystem::path public_dir;
    std::string database_url = "postgresql://postgres:postgres@127.0.0.1:5432/cpp_http_server";
    std::size_t thread_pool_size = 4;
    std::size_t rate_limit_rpm = 120;
    std::string api_key = "dev-api-key";
    bool structured_logs = false;
};

ServerConfig load_server_config(int argc, char* argv[]);
