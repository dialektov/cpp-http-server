#include <doctest/doctest.h>

#include <filesystem>

#include "http_metrics.hpp"
#include "http_request.hpp"
#include "http_router.hpp"
#include "note_store.hpp"
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

ServerConfig test_config() {
    ServerConfig config;
    config.database_url = test_database_url();
    config.rate_limit_rpm = 100000;
    config.api_key = "test-api-key";
    return config;
}

HttpRequest make_request(const std::string& method, const std::string& path,
                         const std::string& body = "") {
    HttpRequest request;
    request.method = method;
    request.path = path;
    request.version = "HTTP/1.1";
    request.body = body;
    request.client_ip = "127.0.0.1";
    if (!body.empty()) {
        request.headers["content-type"] = "application/json";
    }
    return request;
}

}  // namespace

TEST_CASE("HttpRouter serves health endpoint") {
    SKIP_IF_NO_PG();

    ServerStats stats;
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config(), &stats);

    const HttpResponse response = router.route(make_request("GET", "/health"));
    CHECK(response.status_code == 200);
    CHECK(response.body == R"({"status":"ok","database":"ok"})");
    CHECK(stats.requests_total.load() == 1);
}

TEST_CASE("HttpRouter blocks path traversal") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    const HttpResponse response = router.route(make_request("GET", "/../secret.txt"));
    CHECK(response.status_code == 403);
}

TEST_CASE("HttpRouter returns 404 for missing static file") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    const HttpResponse response = router.route(make_request("GET", "/missing-file.txt"));
    CHECK(response.status_code == 404);
}

TEST_CASE("HttpRouter serves index.html for root path") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    const HttpResponse response = router.route(make_request("GET", "/"));
    CHECK(response.status_code == 200);
    CHECK(response.content_type == "text/html; charset=utf-8");
    CHECK(response.body.find("cpp-http-server") != std::string::npos);
}

TEST_CASE("HttpRouter returns 404 for unsupported POST route") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    const HttpResponse response = router.route(make_request("POST", "/health", R"({"a":1})"));
    CHECK(response.status_code == 404);
}

TEST_CASE("HttpRouter exposes metrics endpoint") {
    SKIP_IF_NO_PG();
    ServerStats stats;
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config(), &stats);

    router.route(make_request("GET", "/health"));
    const HttpResponse response = router.route(make_request("GET", "/metrics"));

    CHECK(response.status_code == 200);
    CHECK(response.body.find("\"requests_total\":2") != std::string::npos);
    CHECK(response.body.find("\"active_connections\":") != std::string::npos);
}

TEST_CASE("HttpRouter echoes JSON body on POST /api/echo") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    const HttpResponse response =
        router.route(make_request("POST", "/api/echo", R"({"name":"ivan"})"));

    CHECK(response.status_code == 200);
    CHECK(response.body == R"({"echo":{"name":"ivan"}})");
}

TEST_CASE("HttpRouter rejects echo without JSON content type") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    HttpRequest request = make_request("POST", "/api/echo", R"({"name":"ivan"})");
    request.headers.clear();

    const HttpResponse response = router.route(request);
    CHECK(response.status_code == 415);
}

TEST_CASE("HttpRouter rejects empty echo body") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    HttpRequest request = make_request("POST", "/api/echo");
    request.headers["content-type"] = "application/json";

    const HttpResponse response = router.route(request);
    CHECK(response.status_code == 400);
}

TEST_CASE("HttpRouter exposes prometheus metrics endpoint") {
    SKIP_IF_NO_PG();
    ServerStats stats;
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config(), &stats);

    router.route(make_request("GET", "/health"));
    const HttpResponse response = router.route(make_request("GET", "/metrics/prometheus"));

    CHECK(response.status_code == 200);
    CHECK(response.content_type == "text/plain; version=0.0.4; charset=utf-8");
    CHECK(response.body.find("# TYPE http_requests_total counter") != std::string::npos);
    CHECK(response.body.find("http_active_connections") != std::string::npos);
}

TEST_CASE("HttpRouter protects /api/me with api key") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    CHECK(router.route(make_request("GET", "/api/me")).status_code == 401);

    HttpRequest authorized = make_request("GET", "/api/me");
    authorized.headers["x-api-key"] = "test-api-key";
    const HttpResponse response = router.route(authorized);
    CHECK(response.status_code == 200);
    CHECK(response.body.find("\"authenticated\":true") != std::string::npos);
}

TEST_CASE("HttpRouter supports notes CRUD") {
    SKIP_IF_NO_PG();
    NoteStore store(test_database_url());
    store.reset_for_tests();
    HttpRouter router(project_public_dir(), store, test_config());

    const HttpResponse created =
        router.route(make_request("POST", "/api/notes", R"({"title":"todo","content":"buy milk"})"));
    CHECK(created.status_code == 201);
    CHECK(created.body.find(R"("title":"todo")") != std::string::npos);

    const HttpResponse listed = router.route(make_request("GET", "/api/notes"));
    CHECK(listed.status_code == 200);
    CHECK(listed.body.find("buy milk") != std::string::npos);

    const HttpResponse updated = router.route(
        make_request("PATCH", "/api/notes/1", R"({"title":"done"})"));
    CHECK(updated.status_code == 200);
    CHECK(updated.body.find(R"("title":"done")") != std::string::npos);

    const HttpResponse deleted = router.route(make_request("DELETE", "/api/notes/1"));
    CHECK(deleted.status_code == 200);
    CHECK(deleted.body == R"({"deleted":true})");
}
