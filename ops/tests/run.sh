#!/usr/bin/env bash
set -euo pipefail

GATEWAY_URL="${GATEWAY_URL:-http://gateway:8080}"
AUTH_URL="${AUTH_URL:-http://auth-service:8001}"

echo "Waiting for auth-service..."
for i in $(seq 1 60); do
  if curl -fsS "${AUTH_URL}/health" >/dev/null; then
    echo "Auth service is healthy."
    break
  fi
  sleep 2
  if [ "$i" -eq 60 ]; then
    echo "Auth service never became healthy."
    exit 1
  fi
done

echo "Waiting for gateway..."
for i in $(seq 1 60); do
  if curl -fsS "${GATEWAY_URL}/health" >/dev/null; then
    echo "Gateway is healthy."
    break
  fi
  sleep 2
  if [ "$i" -eq 60 ]; then
    echo "Gateway never became healthy."
    exit 1
  fi
done

echo "Smoke: gateway /health"
curl -fsS "${GATEWAY_URL}/health" | head -c 200 || true
echo

echo "Smoke: auth /health"
curl -fsS "${AUTH_URL}/health" | head -c 200 || true
echo

echo "OK: integration smoke tests passed."
