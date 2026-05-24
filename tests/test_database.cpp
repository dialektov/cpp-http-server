#include "test_database.hpp"

#include "note_store.hpp"

std::string test_database_url() {
    if (const char* url = std::getenv("HTTP_DATABASE_URL")) {
        return url;
    }
    if (const char* url = std::getenv("TEST_DATABASE_URL")) {
        return url;
    }
    return "postgresql://postgres:postgres@127.0.0.1:5432/cpp_http_server_test";
}

bool database_available() {
    static int cached = -1;
    if (cached >= 0) {
        return cached == 1;
    }

    try {
        NoteStore store(test_database_url());
        store.reset_for_tests();
        cached = 1;
    } catch (...) {
        cached = 0;
    }
    return cached == 1;
}
