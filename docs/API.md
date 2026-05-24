# API: cpp-http-server

Базовый URL: `http://127.0.0.1:8080`

Все JSON-ответы: `Content-Type: application/json; charset=utf-8`.

---

## Health & metrics

### `GET /health`

Проверка процесса и PostgreSQL.

**200 OK**

```json
{"status":"ok","database":"ok"}
```

**503 Service Unavailable** (БД недоступна)

```json
{"status":"degraded","database":"unavailable"}
```

### `GET /metrics`

```json
{"requests_total":42,"active_connections":1,"uptime_seconds":120}
```

### `GET /metrics/prometheus`

Текст Prometheus (не JSON).

---

## Time & echo

### `GET /api/time`

```json
{"unix_time":1710000000}
```

### `POST /api/echo`

Требует `Content-Type: application/json`.

**Body:** любой JSON → **200** `{"echo":<body>}`

**400** — пустое тело. **415** — не JSON content-type.

---

## Auth

### `GET /api/me`

Заголовок: `X-API-Key: <key>` или `Authorization: Bearer <key>`.

**200** `{"service":"cpp-http-server","authenticated":true}`

**401** `{"error":"invalid api key"}`

---

## Notes CRUD

| Метод | Путь | Описание |
|-------|------|----------|
| GET | `/api/notes` | Список |
| POST | `/api/notes` | Создать |
| GET | `/api/notes/{id}` | Одна заметка |
| PATCH | `/api/notes/{id}` | Обновить |
| DELETE | `/api/notes/{id}` | Удалить |

**POST / PATCH body**

```json
{"title":"my note","content":"optional"}
```

PATCH: хотя бы одно из `title`, `content`.

**Коды:** `201` создание, `404` не найдено, `400`/`415` валидация.

---

## Rate limiting

- По умолчанию **120 запросов/мин** на IP (`rate_limit_rpm`)
- Без лимита: `/health`, `/metrics`, `/metrics/prometheus`
- **429** `{"error":"rate limit exceeded"}`

---

## Статика

| Путь | Файл |
|------|------|
| `/` | `public/index.html` |
| `/notes.html` | UI для заметок |

---

## Примеры

```bash
curl http://127.0.0.1:8080/health
curl http://127.0.0.1:8080/api/notes
curl -X POST http://127.0.0.1:8080/api/notes \
  -H "Content-Type: application/json" \
  -d '{"title":"todo","content":"buy milk"}'
curl http://127.0.0.1:8080/api/notes/1
curl -X PATCH http://127.0.0.1:8080/api/notes/1 \
  -H "Content-Type: application/json" \
  -d '{"title":"updated"}'
curl -X DELETE http://127.0.0.1:8080/api/notes/1
curl -H "X-API-Key: dev-api-key" http://127.0.0.1:8080/api/me
```
