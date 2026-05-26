#!/bin/bash
# Запуск веб-дашборда Farm Dashboard
# Требуется: MySQL сервер с данными от ESP32
set -e
DIR="$(cd "$(dirname "$0")" && pwd)"

if command -v python3.11 &> /dev/null; then
    PY=python3.11
else
    PY=python3
fi

echo "=== Farm Dashboard ==="
echo "Python: $($PY --version)"
echo "Server: http://localhost:50002"
echo ""

$PY "$DIR/app.py"
