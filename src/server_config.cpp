#include "server_config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

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

std::uint16_t parse_port_value(const std::string& value) {
    const int port = std::stoi(value);
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("Port must be between 1 and 65535");
    }
    return static_cast<std::uint16_t>(port);
}

std::size_t parse_thread_pool_size(const std::string& value) {
    const int threads = std::stoi(value);
    if (threads <= 0 || threads > 256) {
        throw std::runtime_error("thread_pool_size must be between 1 and 256");
    }
    return static_cast<std::size_t>(threads);
}

const char* env_or_null(const char* name) {
    const char* value = std::getenv(name);
    return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

void apply_config_line(ServerConfig& config, const std::string& line) {
    const auto comment = line.find('#');
    const std::string trimmed = trim(comment == std::string::npos ? line : line.substr(0, comment));
    if (trimmed.empty()) {
        return;
    }

    const auto delimiter = trimmed.find('=');
    if (delimiter == std::string::npos) {
        throw std::runtime_error("Invalid config line: " + line);
    }

    const std::string key = to_lower(trim(trimmed.substr(0, delimiter)));
    const std::string value = trim(trimmed.substr(delimiter + 1));

    if (key == "host") {
        config.host = value;
    } else if (key == "port") {
        config.port = parse_port_value(value);
    } else if (key == "public_dir") {
        config.public_dir = value;
    } else if (key == "database_url") {
        config.database_url = value;
    } else if (key == "database_path") {
        config.database_url = value;
    } else if (key == "thread_pool_size") {
        config.thread_pool_size = parse_thread_pool_size(value);
    } else if (key == "rate_limit_rpm") {
        config.rate_limit_rpm = static_cast<std::size_t>(std::stoi(value));
    } else if (key == "api_key") {
        config.api_key = value;
    } else if (key == "structured_logs") {
        config.structured_logs = (value == "1" || value == "true" || value == "yes");
    } else {
        throw std::runtime_error("Unknown config key: " + key);
    }
}

void load_config_file(ServerConfig& config, const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Failed to open config file: " + path.string());
    }

    std::string line;
    while (std::getline(input, line)) {
        apply_config_line(config, line);
    }
}

void apply_env(ServerConfig& config) {
    if (const char* host = env_or_null("HTTP_HOST")) {
        config.host = host;
    }
    if (const char* port = env_or_null("HTTP_PORT")) {
        config.port = parse_port_value(port);
    }
    if (const char* public_dir = env_or_null("HTTP_PUBLIC_DIR")) {
        config.public_dir = public_dir;
    }
    if (const char* threads = env_or_null("HTTP_THREAD_POOL_SIZE")) {
        config.thread_pool_size = parse_thread_pool_size(threads);
    }
    if (const char* database_url = env_or_null("HTTP_DATABASE_URL")) {
        config.database_url = database_url;
    } else if (const char* database_path = env_or_null("HTTP_DATABASE_PATH")) {
        config.database_url = database_path;
    }
    if (const char* rate_limit = env_or_null("HTTP_RATE_LIMIT_RPM")) {
        config.rate_limit_rpm = static_cast<std::size_t>(std::stoi(rate_limit));
    }
    if (const char* api_key = env_or_null("HTTP_API_KEY")) {
        config.api_key = api_key;
    }
    if (const char* structured_logs = env_or_null("HTTP_STRUCTURED_LOGS")) {
        const std::string value = structured_logs;
        config.structured_logs = (value == "1" || value == "true" || value == "yes");
    }
}

std::filesystem::path default_public_dir() {
    return std::filesystem::current_path() / "public";
}

}  // namespace

ServerConfig load_server_config(int argc, char* argv[]) {
    ServerConfig config;
    config.public_dir = default_public_dir();

    std::filesystem::path config_file_path;
    bool config_file_set = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config") {
            if (i + 1 >= argc) {
                throw std::runtime_error("--config requires a file path");
            }
            config_file_path = argv[++i];
            config_file_set = true;
            continue;
        }
        if (arg.rfind("--", 0) == 0) {
            throw std::runtime_error("Unknown option: " + arg);
        }
    }

    if (!config_file_set) {
        if (const char* config_env = env_or_null("HTTP_CONFIG")) {
            config_file_path = config_env;
            config_file_set = true;
        } else {
            const std::filesystem::path default_config = "config/server.conf";
            if (std::filesystem::exists(default_config)) {
                config_file_path = default_config;
                config_file_set = true;
            }
        }
    }

    if (config_file_set) {
        load_config_file(config, config_file_path);
    }

    apply_env(config);

    int positional = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config") {
            ++i;
            continue;
        }
        if (arg.rfind("--", 0) == 0) {
            continue;
        }

        if (positional == 0) {
            config.port = parse_port_value(arg);
        } else if (positional == 1) {
            config.public_dir = arg;
        } else if (positional == 2) {
            config.host = arg;
        } else {
            throw std::runtime_error("Too many positional arguments");
        }
        ++positional;
    }

    return config;
}
