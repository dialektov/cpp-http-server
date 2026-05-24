#include <doctest/doctest.h>

#include "json_utils.hpp"

TEST_CASE("json_get_string extracts string field") {
    const std::string body = R"({"title":"hello","content":"world"})";
    CHECK(json_get_string(body, "title") == "hello");
    CHECK(json_get_string(body, "content") == "world");
}

TEST_CASE("parse_note_id extracts numeric id from path") {
    CHECK(parse_note_id("/api/notes/42") == 42);
    CHECK_FALSE(parse_note_id("/api/notes/").has_value());
    CHECK_FALSE(parse_note_id("/api/notes/abc").has_value());
}

TEST_CASE("json_escape escapes quotes and slashes") {
    CHECK(json_escape(R"(say "hi")") == R"(say \"hi\")");
}
