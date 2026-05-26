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

# Проверка зависимостей
echo ""
echo "Checking dependencies..."
$PY -c "import flask" 2>/dev/null || { echo "ERROR: Flask не установлен. Выполните: pip install -r requirements.txt"; exit 1; }
$PY -c "import pymysql" 2>/dev/null || { echo "ERROR: PyMySQL не установлен. Выполните: pip install -r requirements.txt"; exit 1; }

echo "Dependencies OK"
echo ""
echo "Server: http://localhost:50002"
echo ""

$PY "$DIR/app.py"
