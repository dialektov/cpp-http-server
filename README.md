# cpp-http-server

Минималистичный HTTP-сервер на C++17: сокеты, парсинг HTTP, роутинг, статика, метрики, тесты и CI.

Pet-проект под портфолио: показывает системное программирование, архитектуру и инженерные практики (тесты, Docker, CI, техотчёт).

## Возможности

- HTTP/1.1 сервер на Berkeley/Winsock2 сокетах
- Эндпоинты: `/health`, `/metrics`, `/api/time`, `POST /api/echo`
- Таблица маршрутов `method + path → handler`
- Access-log: `[2026-05-25 14:30:00] GET /health 200 12ms`
- Prometheus metrics: `GET /metrics/prometheus`
- Graceful shutdown по Ctrl+C / SIGTERM
- Пул потоков для обработки запросов
- Конфиг: файл `config/server.conf`, env-переменные, CLI
- Раздача статики из `public/` с защитой от path traversal
- Notes CRUD API на PostgreSQL (libpq)
- Rate limiting, API-key auth (`GET /api/me`)
- Keep-Alive, structured JSON logs
- CD в GitHub Container Registry
- Unit- и integration-тесты (`doctest`, 37 cases)
- Docker + GitHub Actions CI

## Стек

- C++17, CMake 3.16+
- PostgreSQL (libpq) — хранение заметок
- Winsock2 (Windows) / BSD sockets (Linux)
- doctest, clang-format, GitHub Actions

## Архитектура

```
Client
  │
  ▼
HttpServer (accept loop + thread pool)
  │
  ├─ parse_http_request() + body
  │
  ▼
HttpRouter (handler table + static fallback)
  │
  ▼
build_http_response() → access log
```

- `http_core` — парсинг, роутинг, конфиг, thread pool, логгер
- `http_server_lib` — сетевой слой
- `http_server` — CLI entrypoint

## Конфигурация

Приоритет (снизу вверх): defaults → `config/server.conf` → env → CLI.

**Файл** `config/server.conf`:

```ini
host=127.0.0.1
port=8080
public_dir=public
thread_pool_size=4
```

**Env-переменные:**

| Переменная | Описание |
|------------|----------|
| `HTTP_CONFIG` | Путь к конфиг-файлу |
| `HTTP_HOST` | Host для bind |
| `HTTP_PORT` | Порт |
| `HTTP_PUBLIC_DIR` | Папка статики |
| `HTTP_THREAD_POOL_SIZE` | Размер пула потоков |
| `HTTP_DATABASE_URL` | PostgreSQL connection string |
| `HTTP_RATE_LIMIT_RPM` | Rate limit |
| `HTTP_API_KEY` | Ключ для `/api/me` |
| `HTTP_STRUCTURED_LOGS` | JSON access log (`true`/`1`) |

**CLI:**

```bash
http_server [port] [public_dir] [host]
http_server --config path/to/server.conf
```

## Быстрый старт

**Windows (MSYS2/MinGW):** нужен клиент PostgreSQL:

```powershell
C:\msys64\usr\bin\pacman.exe -S mingw-w64-ucrt-x86_64-postgresql
```

```powershell
cd cpp-http-server
cmake -B build -G "MinGW Makefiles"
cmake --build build
$env:HTTP_DATABASE_URL="postgresql://postgres:postgres@127.0.0.1:5432/cpp_http_server"
.\build\http_server.exe 8080
```

Открыть: http://127.0.0.1:8080

Аргументы: `http_server [port] [public_dir] [host]`

## Docker

```bash
docker compose up --build
```

PostgreSQL поднимается автоматически, сервер ждёт healthcheck БД.

## Локальный PostgreSQL

```bash
# пример: создать БД
createdb cpp_http_server
# или через docker только postgres:
docker run --name cpp-pg -e POSTGRES_PASSWORD=postgres -e POSTGRES_DB=cpp_http_server -p 5432:5432 -d postgres:16-alpine
```

Сервер будет доступен на http://localhost:8080

## Тесты и форматирование

```powershell
cmake -B build -G "MinGW Makefiles"
cmake --build build
$env:HTTP_DATABASE_URL="postgresql://postgres:postgres@127.0.0.1:5432/cpp_http_server_test"
ctest --test-dir build --output-on-failure
```

Тесты, требующие БД, пропускаются, если PostgreSQL недоступен.

```bash
find include src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i
```

## Бенчмарк

Запустить сервер, затем:

```bash
bash scripts/bench.sh 127.0.0.1 8080 500
```

Скрипт замеряет RPS для `/health` и выводит `/metrics`.

## API

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/` | Главная страница (`public/index.html`) |
| GET | `/health` | Healthcheck + статус БД |
| GET | `/notes.html` | UI для заметок |
| GET | `/metrics` | Метрики: `requests_total`, `uptime_seconds` |
| GET | `/api/time` | Текущее unix-время в JSON |
| GET | `/api/notes`, `/api/notes/{id}` | CRUD заметок (PostgreSQL) |
| POST | `/api/notes` | Создать заметку |
| PATCH | `/api/notes/{id}` | Обновить заметку |
| DELETE | `/api/notes/{id}` | Удалить заметку |
| GET | `/api/me` | Auth check (X-API-Key) |

Примеры:

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/metrics
curl -X POST http://127.0.0.1:8080/api/echo -H "Content-Type: application/json" -d "{\"ping\":1}"
```

## CI/CD

- **CI**: clang-format, build, ctest
- **CD**: Docker image → `ghcr.io` on push to `main`

## Миграции БД

Схема применяется при старте сервера. Вручную:

```bash
bash scripts/init-db.sh
# или: psql "$HTTP_DATABASE_URL" -f migrations/001_notes.sql
```

Скопируй `.env.example` → `.env` для локальных переменных (опционально).

## Документация

- [docs/TECH_REPORT.md](docs/TECH_REPORT.md) — техотчёт: решения, ошибки, тесты
- [docs/API.md](docs/API.md) — контракт API

## Структура проекта

```
include/          заголовки
src/              реализация
tests/            unit + integration tests
public/           статические файлы
docs/             техотчёт и API
scripts/          бенчмарк
.github/          CI
```
