#include <doctest/doctest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

#include "server_config.hpp"

namespace {

std::filesystem::path temp_config(const std::string& contents) {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "cpp-http-server-test.conf";
    std::ofstream output(path);
    output << contents;
    return path;
}

void unset_env(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

void set_env(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

}  // namespace

TEST_CASE("load_server_config reads values from config file") {
    const std::filesystem::path config_path = temp_config(
        "host=0.0.0.0\n"
        "port=9090\n"
        "public_dir=public\n"
        "thread_pool_size=8\n");

    char arg0[] = "http_server";
    char arg1[] = "--config";
    const std::string config_arg = config_path.string();
    std::vector<char> config_buffer(config_arg.begin(), config_arg.end());
    config_buffer.push_back('\0');
    char* config_ptr = config_buffer.data();
    char* argv[] = {arg0, arg1, config_ptr};

    const ServerConfig config = load_server_config(3, argv);
    CHECK(config.host == "0.0.0.0");
    CHECK(config.port == 9090);
    CHECK(config.public_dir == "public");
    CHECK(config.thread_pool_size == 8);
}

TEST_CASE("load_server_config CLI overrides config file") {
    const std::filesystem::path config_path = temp_config("port=9090\n");

    char arg0[] = "http_server";
    char arg1[] = "--config";
    const std::string config_arg = config_path.string();
    std::vector<char> config_buffer(config_arg.begin(), config_arg.end());
    config_buffer.push_back('\0');
    char* config_ptr = config_buffer.data();
    char arg3[] = "8081";
    char* argv[] = {arg0, arg1, config_ptr, arg3};

    const ServerConfig config = load_server_config(4, argv);
    CHECK(config.port == 8081);
}

TEST_CASE("load_server_config reads environment variables") {
    unset_env("HTTP_CONFIG");
    unset_env("HTTP_PORT");
    unset_env("HTTP_HOST");
    set_env("HTTP_PORT", "7777");
    set_env("HTTP_HOST", "0.0.0.0");

    char arg0[] = "http_server";
    char* argv[] = {arg0};

    const ServerConfig config = load_server_config(1, argv);
    CHECK(config.port == 7777);
    CHECK(config.host == "0.0.0.0");

    unset_env("HTTP_PORT");
    unset_env("HTTP_HOST");
}
