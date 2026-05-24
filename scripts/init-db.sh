#!/usr/bin/env bash
set -euo pipefail

URL="${HTTP_DATABASE_URL:-postgresql://postgres:postgres@127.0.0.1:5432/cpp_http_server}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

psql "$URL" -f "$SCRIPT_DIR/../migrations/001_notes.sql"
echo "Schema applied: $URL"
