#include "note_store.hpp"

#include "json_utils.hpp"

#include <chrono>
#include <sstream>
#include <stdexcept>
#include <mutex>

#include <libpq-fe.h>

namespace {

std::int64_t now_unix_seconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

Note read_note_row(PGresult* result, int row) {
    Note note;
    note.id = std::stoll(PQgetvalue(result, row, 0));
    note.title = PQgetvalue(result, row, 1);
    note.content = PQgetvalue(result, row, 2);
    note.created_at = std::stoll(PQgetvalue(result, row, 3));
    return note;
}

void clear_result(PGresult* result) {
    if (result != nullptr) {
        PQclear(result);
    }
}

}  // namespace

NoteStore::NoteStore(std::string database_url) : database_url_(std::move(database_url)) {
    connection_ = PQconnectdb(database_url_.c_str());
    if (PQstatus(connection_) != CONNECTION_OK) {
        const std::string error = PQerrorMessage(connection_);
        PQfinish(connection_);
        connection_ = nullptr;
        throw std::runtime_error("Failed to connect to PostgreSQL: " + error);
    }
    exec_schema();
}

NoteStore::~NoteStore() {
    if (connection_ != nullptr) {
        PQfinish(connection_);
    }
}

void NoteStore::exec_command(const std::string& sql) const {
    std::lock_guard lock(mutex_);
    PGresult* result = PQexec(connection_, sql.c_str());
    const ExecStatusType status = PQresultStatus(result);
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        const std::string error = PQerrorMessage(connection_);
        clear_result(result);
        throw std::runtime_error("PostgreSQL command failed: " + error);
    }
    clear_result(result);
}

void NoteStore::exec_schema() const {
    exec_command(R"(
        CREATE TABLE IF NOT EXISTS notes (
            id BIGSERIAL PRIMARY KEY,
            title TEXT NOT NULL,
            content TEXT NOT NULL DEFAULT '',
            created_at BIGINT NOT NULL
        );
    )");
}

void NoteStore::reset_for_tests() const {
    exec_command("TRUNCATE TABLE notes RESTART IDENTITY;");
}

bool NoteStore::ping() const {
    std::lock_guard lock(mutex_);
    PGresult* result = PQexec(connection_, "SELECT 1");
    const bool ok = PQresultStatus(result) == PGRES_TUPLES_OK;
    clear_result(result);
    return ok;
}

std::vector<Note> NoteStore::list_notes() const {
    std::lock_guard lock(mutex_);
    PGresult* result = PQexec(connection_, "SELECT id, title, content, created_at FROM notes ORDER BY id");
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        const std::string error = PQerrorMessage(connection_);
        clear_result(result);
        throw std::runtime_error("Failed to list notes: " + error);
    }

    std::vector<Note> notes;
    const int rows = PQntuples(result);
    notes.reserve(static_cast<std::size_t>(rows));
    for (int row = 0; row < rows; ++row) {
        notes.push_back(read_note_row(result, row));
    }
    clear_result(result);
    return notes;
}

std::optional<Note> NoteStore::get_note(std::int64_t id) const {
    std::lock_guard lock(mutex_);
    const std::string id_value = std::to_string(id);
    const char* params[] = {id_value.c_str()};

    PGresult* result = PQexecParams(connection_,
                                    "SELECT id, title, content, created_at FROM notes WHERE id = $1",
                                    1, nullptr, params, nullptr, nullptr, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        const std::string error = PQerrorMessage(connection_);
        clear_result(result);
        throw std::runtime_error("Failed to get note: " + error);
    }

    std::optional<Note> note;
    if (PQntuples(result) > 0) {
        note = read_note_row(result, 0);
    }
    clear_result(result);
    return note;
}

Note NoteStore::create_note(const std::string& title, const std::string& content) const {
    std::lock_guard lock(mutex_);
    const std::int64_t created_at = now_unix_seconds();
    const std::string created_at_value = std::to_string(created_at);
    const char* params[] = {title.c_str(), content.c_str(), created_at_value.c_str()};

    PGresult* result =
        PQexecParams(connection_,
                     "INSERT INTO notes(title, content, created_at) VALUES($1, $2, $3) RETURNING id",
                     3, nullptr, params, nullptr, nullptr, 0);
    if (PQresultStatus(result) != PGRES_TUPLES_OK || PQntuples(result) == 0) {
        const std::string error = PQerrorMessage(connection_);
        clear_result(result);
        throw std::runtime_error("Failed to create note: " + error);
    }

    Note note;
    note.id = std::stoll(PQgetvalue(result, 0, 0));
    note.title = title;
    note.content = content;
    note.created_at = created_at;
    clear_result(result);
    return note;
}

std::optional<Note> NoteStore::update_note(std::int64_t id,
                                           const std::optional<std::string>& title,
                                           const std::optional<std::string>& content) const {
    std::lock_guard lock(mutex_);
    auto existing = get_note(id);
    if (!existing.has_value()) {
        return std::nullopt;
    }

    Note updated = *existing;
    if (title.has_value()) {
        updated.title = *title;
    }
    if (content.has_value()) {
        updated.content = *content;
    }

    const std::string id_value = std::to_string(id);
    const char* params[] = {updated.title.c_str(), updated.content.c_str(), id_value.c_str()};

    PGresult* result = PQexecParams(connection_,
                                  "UPDATE notes SET title = $1, content = $2 WHERE id = $3",
                                  3, nullptr, params, nullptr, nullptr, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const std::string error = PQerrorMessage(connection_);
        clear_result(result);
        throw std::runtime_error("Failed to update note: " + error);
    }
    clear_result(result);
    return updated;
}

bool NoteStore::delete_note(std::int64_t id) const {
    std::lock_guard lock(mutex_);
    const std::string id_value = std::to_string(id);
    const char* params[] = {id_value.c_str()};

    PGresult* result =
        PQexecParams(connection_, "DELETE FROM notes WHERE id = $1", 1, nullptr, params, nullptr, nullptr, 0);
    if (PQresultStatus(result) != PGRES_COMMAND_OK) {
        const std::string error = PQerrorMessage(connection_);
        clear_result(result);
        throw std::runtime_error("Failed to delete note: " + error);
    }

    const bool deleted = std::stoi(PQcmdTuples(result)) > 0;
    clear_result(result);
    return deleted;
}

std::string note_to_json(const Note& note) {
    std::ostringstream out;
    out << R"({"id":)" << note.id << R"(,"title":")" << json_escape(note.title) << R"(","content":")"
        << json_escape(note.content) << R"(","created_at":)" << note.created_at << '}';
    return out.str();
}

std::string notes_to_json(const std::vector<Note>& notes) {
    std::ostringstream out;
    out << '[';
    for (std::size_t i = 0; i < notes.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << note_to_json(notes[i]);
    }
    out << ']';
    return out.str();
}
