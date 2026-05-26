#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <mysql/mysql.h>
#include <mosquitto.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// ─── НАСТРОЙКИ MySQL ─────────────────────────────────────────
const std::string DB_HOST     = "localhost";
const std::string DB_USER     = "root";
const std::string DB_PASSWORD = "abobik";
const std::string DB_NAME     = "farm_db";
const int         DB_PORT     = 3306;

// ─── НАСТРОЙКИ MQTT ──────────────────────────────────────────
const std::string MQTT_HOST    = "localhost";
const int         MQTT_PORT    = 1883;
const std::string MQTT_TOPIC   = "farm/logs";
const std::string MQTT_CLIENT  = "FarmDBLogger";
const int         MQTT_QOS     = 1;

// ─── ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ───────────────────────────────────
volatile sig_atomic_t running = 1;
MYSQL* dbConn = nullptr;
struct mosquitto* mqttClient = nullptr;

// ─── ОБРАБОТКА СИГНАЛОВ ──────────────────────────────────────
void signalHandler(int signum) {
    std::cout << "\nПолучен сигнал " << signum << ", завершение..." << std::endl;
    running = 0;
}

// ─── ПОДКЛЮЧЕНИЕ К MySQL ─────────────────────────────────────
bool connectDB() {
    dbConn = mysql_init(nullptr);
    if (!dbConn) {
        std::cerr << "Ошибка mysql_init" << std::endl;
        return false;
    }

    if (!mysql_real_connect(dbConn, DB_HOST.c_str(), DB_USER.c_str(),
                            DB_PASSWORD.c_str(), DB_NAME.c_str(), DB_PORT, nullptr, 0)) {
        std::cerr << "Ошибка подключения к MySQL: " << mysql_error(dbConn) << std::endl;
        mysql_close(dbConn);
        dbConn = nullptr;
        return false;
    }

    std::cout << "Подключено к MySQL: " << DB_HOST << "/" << DB_NAME << std::endl;
    return true;
}

// ─── СОЗДАНИЕ ТАБЛИЦЫ ────────────────────────────────────────
bool createTable() {
    const std::string sql = 
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "  id INT AUTO_INCREMENT PRIMARY KEY,"
        "  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  temperature FLOAT,"
        "  humidity FLOAT,"
        "  soil INT,"
        "  water INT,"
        "  light INT,"
        "  rain INT,"
        "  anomaly BOOLEAN DEFAULT FALSE,"
        "  mse_score FLOAT DEFAULT NULL"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;";

    if (mysql_query(dbConn, sql.c_str())) {
        std::cerr << "Ошибка создания таблицы: " << mysql_error(dbConn) << std::endl;
        return false;
    }

    std::cout << "Таблица sensor_data готова" << std::endl;
    return true;
}

// ─── СОХРАНЕНИЕ ДАННЫХ В БД ──────────────────────────────────
bool saveToDB(const json& data) {
    float temp    = data.value("temp", 0.0f);
    float hum     = data.value("hum", 0.0f);
    int   soil    = data.value("soil", 0);
    int   water   = data.value("water", 0);
    int   light   = data.value("light", 0);
    int   rain    = data.value("rain", 0);
    int   anomaly = data.value("anomaly", false) ? 1 : 0;
    float mse     = data.value("mse", 0.0f);

    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO sensor_data (temperature, humidity, soil, water, light, rain, anomaly, mse_score) "
        "VALUES (%.2f, %.2f, %d, %d, %d, %d, %d, %.4f)",
        temp, hum, soil, water, light, rain, anomaly, mse);

    if (mysql_query(dbConn, sql)) {
        std::cerr << "Ошибка INSERT: " << mysql_error(dbConn) << std::endl;
        return false;
    }

    unsigned long id = mysql_insert_id(dbConn);
    std::cout << "[" << id << "] T:" << temp << "C"
              << " H:" << hum << "%"
              << " Soil:" << soil
              << (anomaly ? " ANOMALY! MSE=" : "")
              << (anomaly ? std::to_string(mse) : "")
              << std::endl;
    return true;
}

// ─── CALLBACK MQTT СООБЩЕНИЙ ─────────────────────────────────
void onMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* message) {
    if (!message->payload) return;

    std::string payload(static_cast<char*>(message->payload), message->payloadlen);
    
    try {
        json data = json::parse(payload);
        saveToDB(data);
    } catch (const json::parse_error& e) {
        std::cerr << "Ошибка парсинга JSON: " << e.what() << std::endl;
        std::cerr << "Payload: " << payload << std::endl;
    }
}

// ─── ГЛАВНАЯ ФУНКЦИЯ ─────────────────────────────────────────
int main() {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "=== Farm DB Logger ===" << std::endl;

    if (!connectDB()) {
        return 1;
    }

    if (!createTable()) {
        mysql_close(dbConn);
        return 1;
    }

    // Инициализация mosquitto
    mosquitto_lib_init();
    mqttClient = mosquitto_new(MQTT_CLIENT.c_str(), true, nullptr);
    if (!mqttClient) {
        std::cerr << "Ошибка создания MQTT клиента" << std::endl;
        mysql_close(dbConn);
        return 1;
    }

    mosquitto_message_callback_set(mqttClient, onMessage);

    if (mosquitto_connect(mqttClient, MQTT_HOST.c_str(), MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Ошибка подключения к MQTT серверу" << std::endl;
        mosquitto_destroy(mqttClient);
        mysql_close(dbConn);
        return 1;
    }

    std::cout << "Подключено к MQTT: " << MQTT_HOST << ":" << MQTT_PORT << std::endl;

    if (mosquitto_subscribe(mqttClient, nullptr, MQTT_TOPIC.c_str(), MQTT_QOS) != MOSQ_ERR_SUCCESS) {
        std::cerr << "Ошибка подписки на тему" << std::endl;
        mosquitto_destroy(mqttClient);
        mysql_close(dbConn);
        return 1;
    }

    std::cout << "Подписка на тему: " << MQTT_TOPIC << std::endl;
    std::cout << "\nОжидание данных..." << std::endl;

    // Запуск loop в отдельном потоке
    std::thread mqttLoop([&]() {
        while (running) {
            mosquitto_loop(mqttClient, -1, 1);
        }
    });

    // Главный поток ждёт сигнала
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    mqttLoop.join();

    std::cout << "\nОтключение..." << std::endl;
    mosquitto_disconnect(mqttClient);
    mosquitto_destroy(mqttClient);
    mosquitto_lib_cleanup();
    mysql_close(dbConn);

    std::cout << "Завершено" << std::endl;
    return 0;
}
