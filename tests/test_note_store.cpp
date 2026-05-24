#include <doctest/doctest.h>

#include "note_store.hpp"
#include "test_database.hpp"

TEST_CASE("NoteStore supports CRUD operations") {
    SKIP_IF_NO_PG();

    NoteStore store(test_database_url());
    store.reset_for_tests();

    const Note created = store.create_note("first", "content");
    CHECK(created.id >= 1);
    CHECK(created.title == "first");

    const auto fetched = store.get_note(created.id);
    REQUIRE(fetched.has_value());
    CHECK(fetched->content == "content");

    const auto updated = store.update_note(created.id, std::string("updated"), std::nullopt);
    REQUIRE(updated.has_value());
    CHECK(updated->title == "updated");

    CHECK(store.delete_note(created.id));
    CHECK_FALSE(store.get_note(created.id).has_value());
}

TEST_CASE("NoteStore ping checks database connectivity") {
    SKIP_IF_NO_PG();

    NoteStore store(test_database_url());
    CHECK(store.ping());
}

TEST_CASE("NoteStore list_notes returns all notes") {
    SKIP_IF_NO_PG();

    NoteStore store(test_database_url());
    store.reset_for_tests();
    store.create_note("a", "1");
    store.create_note("b", "2");
    CHECK(store.list_notes().size() == 2);
}
