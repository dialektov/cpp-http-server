-- Schema for cpp-http-server notes API.
-- Applied automatically on startup (CREATE TABLE IF NOT EXISTS).
-- Run manually: psql "$HTTP_DATABASE_URL" -f migrations/001_notes.sql

CREATE TABLE IF NOT EXISTS notes (
    id BIGSERIAL PRIMARY KEY,
    title TEXT NOT NULL,
    content TEXT NOT NULL DEFAULT '',
    created_at BIGINT NOT NULL
);
