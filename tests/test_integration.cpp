#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <thread>

#include "http_client.hpp"
#include "http_server.hpp"
#include "server_config.hpp"
#include "test_database.hpp"

namespace {

std::filesystem::path project_public_dir() {
    const std::filesystem::path from_cwd = std::filesystem::current_path() / "public";
    if (std::filesystem::exists(from_cwd)) {
        return from_cwd;
    }
    return std::filesystem::path(__FILE__).parent_path().parent_path() / "public";
}

ServerConfig make_test_config(std::uint16_t port) {
    ServerConfig config;
    config.host = "127.0.0.1";
    config.port = port;
    config.public_dir = project_public_dir();
    config.database_url = test_database_url();
    config.thread_pool_size = 1;
    config.rate_limit_rpm = 100000;
    config.api_key = "test-api-key";
    return config;
}

}  // namespace

TEST_CASE("integration: server responds to /health over TCP") {
    SKIP_IF_NO_PG();

    HttpServer server(make_test_config(19081));

    std::thread server_thread([&]() { server.serve(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const HttpClientResponse response = http_get("127.0.0.1", 19081, "/health");
    server_thread.join();

    CHECK(response.status_code == 200);
    CHECK(response.body == R"({"status":"ok","database":"ok"})");
}

TEST_CASE("integration: server returns 404 for unknown route") {
    SKIP_IF_NO_PG();
    HttpServer server(make_test_config(19082));

    std::thread server_thread([&]() { server.serve(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const HttpClientResponse response = http_get("127.0.0.1", 19082, "/does-not-exist");
    server_thread.join();

    CHECK(response.status_code == 404);
}

TEST_CASE("integration: server serves static index page") {
    SKIP_IF_NO_PG();
    HttpServer server(make_test_config(19083));

    std::thread server_thread([&]() { server.serve(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const HttpClientResponse response = http_get("127.0.0.1", 19083, "/");
    server_thread.join();

    CHECK(response.status_code == 200);
    CHECK(response.body.find("cpp-http-server") != std::string::npos);
}

TEST_CASE("integration: server echoes JSON body on POST /api/echo") {
    SKIP_IF_NO_PG();
    HttpServer server(make_test_config(19084));

    std::thread server_thread([&]() { server.serve(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const HttpClientResponse response =
        http_post("127.0.0.1", 19084, "/api/echo", R"({"ping":1})");
    server_thread.join();

    CHECK(response.status_code == 200);
    CHECK(response.body == R"({"echo":{"ping":1}})");
}

TEST_CASE("integration: notes CRUD over TCP") {
    SKIP_IF_NO_PG();
    HttpServer server(make_test_config(19085));

    std::thread server_thread([&]() { server.serve(2); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const HttpClientResponse created =
        http_post("127.0.0.1", 19085, "/api/notes", R"({"title":"note","content":"text"})");
    CHECK(created.status_code == 201);

    const std::size_t id_pos = created.body.find(R"("id":)");
    REQUIRE(id_pos != std::string::npos);
    const std::int64_t note_id = std::stoll(created.body.substr(id_pos + 5));

    const HttpClientResponse fetched =
        http_get("127.0.0.1", 19085, "/api/notes/" + std::to_string(note_id));
    CHECK(fetched.status_code == 200);
    CHECK(fetched.body.find("note") != std::string::npos);

    server_thread.join();
}

TEST_CASE("integration: /api/me requires api key") {
    SKIP_IF_NO_PG();
    HttpServer server(make_test_config(19086));

    std::thread server_thread([&]() { server.serve(2); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    CHECK(http_get("127.0.0.1", 19086, "/api/me").status_code == 401);
    const HttpClientResponse ok =
        http_request("127.0.0.1", 19086, "GET", "/api/me", "", "application/json", "test-api-key");
    CHECK(ok.status_code == 200);

    server_thread.join();
}
