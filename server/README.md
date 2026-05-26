# Farm DB Logger — C++ сервер для записи MQTT данных в MySQL

## Зависимости

```bash
# macOS
brew install mysql-client mosquitto nlohmann-json pkg-config

# Ubuntu/Debian
sudo apt install libmysqlclient-dev libmosquitto-dev nlohmann-json3-dev pkg-config
```

## Настройка MySQL

```bash
# Запустить MySQL
mysql.server start  # macOS
sudo systemctl start mysql  # Linux

# Создать БД
mysql -u root -p < init_db.sql
```

## Настройка MQTT брокера

```bash
# Запустить Mosquitto
mosquitto -v
```

## Сборка

```bash
cd server
mkdir build && cd build
cmake ..
make
```

## Запуск

```bash
# Изменить настройки в main.cpp (MySQL пароль и т.д.)
./farm_db_logger
```

## Структура данных

ESP32 отправляет JSON на топик `farm/logs`:
```json
{
  "temp": 24.5,
  "hum": 42,
  "soil": 1500,
  "water": 2000,
  "light": 3000,
  "rain": 500
}
```

Сервер сохраняет в таблицу `sensor_data`:
| id | timestamp | temperature | humidity | soil | water | light | rain |
