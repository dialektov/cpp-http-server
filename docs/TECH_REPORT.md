# Техотчёт: cpp-http-server



## Зачем я делал этот проект



Хотел C++ pet-проект уровня портфолио: не олимпиадный `.cpp`, а завершённый HTTP-сервис с архитектурой, PostgreSQL, тестами, Docker, CI/CD и живой документацией — по тем же критериям, что Python backend (`task-manager-api`).



## Что в итоге получилось



- HTTP/1.1 сервер на сокетах (Winsock2 / BSD), thread pool, Keep-Alive

- Роутинг: таблица handlers + middleware (rate limit)

- REST Notes CRUD на **PostgreSQL** (libpq, parameterized queries)

- `/health` с проверкой БД, `/metrics`, Prometheus, graceful shutdown

- API key auth (`GET /api/me`), structured JSON logs

- **37** автотестов (doctest): unit + TCP integration

- Docker + docker-compose (postgres + server), CI + CD (ghcr.io)

- Статика: `index.html`, `notes.html` (UI к API)



## Как устроено



```

include/

  http_request.hpp      парсинг HTTP

  http_response.hpp     сборка ответа

  http_router.hpp       handlers + static fallback

  note_store.hpp        PostgreSQL (libpq)

  middleware.hpp        цепочка middleware

  rate_limiter.hpp      лимит по IP

  server_config.hpp     file → env → CLI

  thread_pool.hpp       worker-потоки

  http_server.hpp       accept loop



src/                    реализация

tests/                  doctest + TCP client

public/                 index.html, notes.html

migrations/             SQL-схема notes

```



Поток запроса:



1. `accept()` → thread pool

2. `read_request()` + body (`Content-Length`)

3. `HttpRouter::route()` → middleware → handler / static

4. access log → `send()`



## Решения, которые оставил осознанно



- **`HttpRouter` отдельно от сокетов** — роутинг тестируется без сети.

- **`http_core` library** — общий код для сервера и тестов.

- **Handler table** — новый route = одна строка в `register_routes()`.

- **PostgreSQL + libpq** — как в других backend-проектах портфолио; без ORM, но с `PQexecParams`.

- **`recursive_mutex` на `NoteStore`** — одно соединение libpq, потокобезопасный доступ из thread pool.

- **Схема при старте** — `CREATE TABLE IF NOT EXISTS` + файл `migrations/001_notes.sql` для ручного применения.

- **Rate limit** — health/metrics не лимитируются (мониторинг не ломается).



## Что ломалось по пути



- Монолит в одном файле → вынес `HttpRouter`, `NoteStore`, config.

- Integration tests зависали на `serve(n)` — развёл порты 19081–19086.

- Rate limiter сбрасывал окно на каждый запрос → исправил sentinel `reset_at`.

- SQLite → PostgreSQL: CI с postgres service, `HTTP_DATABASE_URL`.

- Без PostgreSQL локально DB-тесты делают `WARN` + early return; в CI postgres service обязателен.

- На Windows libpq из MSYS2 — `CMAKE` ищет `C:/msys64/ucrt64`.

- Ссылки с главной на `/health` выглядели «пустыми» — это JSON API, не HTML; добавил `notes.html` и пояснения.



## Проверки



```bash

cmake -B build -DCMAKE_BUILD_TYPE=Release

cmake --build build

HTTP_DATABASE_URL=postgresql://postgres:postgres@127.0.0.1:5432/cpp_http_server_test \

  ctest --test-dir build --output-on-failure

```



| Область | Примеры тестов |

|---------|----------------|

| Parser | method, path, headers, malformed |

| Router | health+DB, static, traversal, 404, metrics, notes CRUD |

| Config | file, env, CLI приоритеты |

| Integration | TCP /health, static, POST echo, notes, /api/me |

| Rate limit | 429 после лимита |



## CI/CD



- **CI**: clang-format, build, ctest (postgres service), clang-tidy

- **CD**: Docker image → `ghcr.io` on push to `main`

- **Локально**: `docker compose up --build` или MSYS2 + `HTTP_DATABASE_URL`



## Бенчмарк



```bash

bash scripts/bench.sh 127.0.0.1 8080 500

```



```text

500 GET /health → ~58 RPS (Windows/MinGW, sequential client, pool=4)

```



## Что бы улучшил дальше



- Пул соединений libpq (вместо одного conn + mutex)

- Нормальный JSON parser (сейчас echo вставляет body as-is)

- Fuzz-тесты HTTP-парсера

- Chunked transfer encoding

- TLS (HTTPS)



## Этапы разработки (changelog)



### Этап 0 — скелет

- Сокеты, `/health`, `/api/time`, статика



### Этап 1 — portfolio-ready

- `http_core`, `HttpRouter`, `/metrics`, doctest, Docker, CI, docs



### Этап 2 — production-like

- POST echo, handler table, access log, thread pool, config



### Этап 3 — observability

- Graceful shutdown, Prometheus, select timeout



### Этап 4 — REST & polish

- Notes CRUD, middleware, rate limit, API key, Keep-Alive, structured logs, CD



### Этап 5 — PostgreSQL

- libpq, `database_url`, docker-compose postgres, CI service



### Этап 6 — доводка портфолио (текущий)

- `/health` проверяет БД (`ping`)

- `recursive_mutex` в `NoteStore`

- `migrations/001_notes.sql`, `.env.example`, MIT LICENSE

- `notes.html`, обновлённые README/API/TECH_REPORT

- `SKIP_IF_NO_PG` (WARN + return без БД; CI всегда с postgres)

- clang-tidy в CI, git + GitHub

