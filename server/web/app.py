#!/usr/bin/env python3.11
"""Farm Dashboard — Flask + MySQL + Chart.js"""

from flask import Flask, jsonify, render_template, abort
import pymysql
from pymysql import Error
from datetime import datetime, timedelta
import os

app = Flask(__name__)

DB_CONFIG = {
    "host": "localhost",
    "user": "farm_user",
    "password": "abobik",
    "database": "farm_db",
    "port": 3306,
}


def _fix_timestamp(row):
    if row and "timestamp" in row and row["timestamp"]:
        row["timestamp"] = row["timestamp"].isoformat()
    return row


def get_db():
    try:
        conn = pymysql.connect(**DB_CONFIG)
        return conn
    except Error as e:
        print(f"[DB ERROR] {e}")
        print(f"[DB ERROR] Убедитесь, что:")
        print(f"  1. MySQL/MariaDB запущен")
        print(f"  2. Пользователь '{DB_CONFIG['user']}' создан и имеет права на БД '{DB_CONFIG['database']}'")
        print(f"  3. Пароль верный")
        print(f"  4. Зависимость 'pymysql' установлена: pip install pymysql")
        return None


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/api/latest")
def api_latest():
    conn = get_db()
    if not conn:
        return jsonify({"ok": False, "error": "DB connection failed"}), 503
    cursor = conn.cursor(pymysql.cursors.DictCursor)
    cursor.execute(
        "SELECT id, timestamp, temperature, humidity, soil, water, light, rain, "
        "anomaly, mse_score FROM sensor_data ORDER BY id DESC LIMIT 1"
    )
    row = cursor.fetchone()
    cursor.close()
    conn.close()
    if not row:
        return jsonify({"ok": False, "error": "No data"}), 404
    row["anomaly"] = bool(row["anomaly"])
    row["mse_score"] = float(row["mse_score"]) if row["mse_score"] else 0
    _fix_timestamp(row)
    return jsonify({"ok": True, "data": row})


@app.route("/api/history")
def api_history():
    conn = get_db()
    if not conn:
        return jsonify({"ok": False, "error": "DB connection failed"}), 503
    cursor = conn.cursor(pymysql.cursors.DictCursor)
    cursor.execute(
        "SELECT id, timestamp, temperature, humidity, soil, water, light, rain, "
        "anomaly, mse_score FROM sensor_data ORDER BY id DESC LIMIT 100"
    )
    rows = cursor.fetchall()
    cursor.close()
    conn.close()
    rows.reverse()
    for r in rows:
        r["anomaly"] = bool(r["anomaly"])
        _fix_timestamp(r)
    return jsonify({"ok": True, "data": rows})


@app.route("/api/anomalies")
def api_anomalies():
    conn = get_db()
    if not conn:
        return jsonify({"ok": False, "error": "DB connection failed"}), 503
    cursor = conn.cursor(pymysql.cursors.DictCursor)
    cursor.execute(
        "SELECT id, timestamp, temperature, humidity, soil, water, light, rain, "
        "mse_score FROM sensor_data WHERE anomaly = 1 ORDER BY id DESC LIMIT 20"
    )
    rows = cursor.fetchall()
    cursor.close()
    conn.close()
    for r in rows:
        _fix_timestamp(r)
    return jsonify({"ok": True, "data": rows})


@app.route("/api/stats")
def api_stats():
    conn = get_db()
    if not conn:
        return jsonify({"ok": False, "error": "DB connection failed"}), 503
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*), AVG(temperature), AVG(humidity) FROM sensor_data")
    count, avg_temp, avg_hum = cursor.fetchone()
    cursor.execute("SELECT COUNT(*) FROM sensor_data WHERE anomaly = 1")
    anom_count = cursor.fetchone()[0]
    cursor.execute("SELECT MIN(timestamp) FROM sensor_data")
    since_row = cursor.fetchone()
    cursor.close()
    conn.close()
    return jsonify({
        "ok": True,
        "data": {
            "total_readings": count,
            "total_anomalies": anom_count,
            "avg_temperature": round(avg_temp, 1) if avg_temp else 0,
            "avg_humidity": round(avg_hum, 1) if avg_hum else 0,
            "since": str(since_row[0]) if since_row and since_row[0] else "—",
        }
    })


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=50002, debug=True)
