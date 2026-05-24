#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct Note {
    std::int64_t id = 0;
    std::string title;
    std::string content;
    std::int64_t created_at = 0;
};

class NoteStore {
public:
    explicit NoteStore(std::string database_url);
    ~NoteStore();

    NoteStore(const NoteStore&) = delete;
    NoteStore& operator=(const NoteStore&) = delete;

    std::vector<Note> list_notes() const;
    std::optional<Note> get_note(std::int64_t id) const;
    Note create_note(const std::string& title, const std::string& content) const;
    std::optional<Note> update_note(std::int64_t id, const std::optional<std::string>& title,
                                    const std::optional<std::string>& content) const;
    bool delete_note(std::int64_t id) const;
    void reset_for_tests() const;
    bool ping() const;

private:
    std::string database_url_;
    struct pg_conn* connection_ = nullptr;
    mutable std::recursive_mutex mutex_{};

    void exec_schema() const;
    void exec_command(const std::string& sql) const;
};

std::string note_to_json(const Note& note);
std::string notes_to_json(const std::vector<Note>& notes);
