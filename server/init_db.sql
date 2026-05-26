-- Создание базы данных
CREATE DATABASE IF NOT EXISTS farm_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- Создание пользователя (опционально)
-- CREATE USER 'farm_user'@'localhost' IDENTIFIED BY 'farm_password';
-- GRANT ALL PRIVILEGES ON farm_db.* TO 'farm_user'@'localhost';
-- FLUSH PRIVILEGES;

-- Использование базы
USE farm_db;

-- Таблица уже создаётся в коде, но можно создать вручную:
CREATE TABLE IF NOT EXISTS sensor_data (
    id INT AUTO_INCREMENT PRIMARY KEY,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    temperature FLOAT,
    humidity FLOAT,
    soil INT,
    water INT,
    light INT,
    rain INT,
    anomaly BOOLEAN DEFAULT FALSE,
    mse_score FLOAT DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
