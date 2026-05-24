#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"
REQUESTS="${3:-500}"

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is required"
  exit 1
fi

echo "Benchmark: ${REQUESTS} GET /health requests to http://${HOST}:${PORT}"

start=$(date +%s%N)
for ((i = 1; i <= REQUESTS; i++)); do
  curl -s "http://${HOST}:${PORT}/health" >/dev/null
done
end=$(date +%s%N)

elapsed_ms=$(( (end - start) / 1000000 ))
rps=$(( REQUESTS * 1000 / (elapsed_ms + 1) ))

echo "Total time: ${elapsed_ms} ms"
echo "Approx RPS: ${rps}"

metrics=$(curl -s "http://${HOST}:${PORT}/metrics")
echo "Metrics: ${metrics}"
