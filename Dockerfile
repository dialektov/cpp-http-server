FROM gcc:14-bookworm AS builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --config Release

FROM debian:bookworm-slim

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 libpq5 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /app/build/http_server /app/http_server
COPY public /app/public
COPY config /app/config

EXPOSE 8080
CMD ["./http_server", "--config", "config/server.conf"]
