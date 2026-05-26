#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include "FS.h"
#include "SPIFFS.h"
#include <ESP32Time.h> 
#include "autoencoder.h"
#include "model_autoencoder.h"

// ─── НАСТРОЙКИ СЕТИ (ЗАМЕНИ НА СВОИ) ─────────────────────────
const char* ssid        = "YOUR_WIFI_SSID";
const char* password    = "YOUR_WIFI_PASSWORD";
const char* mqtt_server = "192.168.1.100"; // IP твоего ПК с брокером Mosquitto

// ─── ПИНЫ ПОДКЛЮЧЕНИЯ ПЕРИФЕРИИ ──────────────────────────────
#define PIN_FAN_IN1    18
#define PIN_FAN_IN2    19
#define PIN_PIR        23
#define PIN_BUTTON      5
#define PIN_TRIG       12
#define PIN_ECHO       13
#define PIN_DHT        17
#define PIN_STEAM      35
#define PIN_PHOTO      34
#define PIN_SERVO      26
#define PIN_BUZZER     16
#define PIN_LED        27
#define PIN_WATER_SENS 33  
#define PIN_SOIL       32
#define PIN_PUMP       25

// ─── ПОРОГИ АВТОМАТИКИ ───────────────────────────────────────
#define TEMP_FAN_ON       28.0
#define LIGHT_LED_ON      1500
#define SOIL_DRY          2000
#define WATER_LOW         1000
#define RAIN_THRESHOLD    2000
#define FOOD_DIST_CM      15
#define SERVO_OPEN        180
#define SERVO_CLOSED        0

// ─── ГЛОБАЛЬНЫЕ ОБЪЕКТЫ ──────────────────────────────────────
WiFiClient espClient;
PubSubClient mqttClient(espClient);
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(PIN_DHT, DHT11);
Servo feederServo;
ESP32Time rtc; 

// ─── TINYML: АВТОЭНКОДЕР ДЛЯ ДЕТЕКЦИИ АНОМАЛИЙ ──────────
TinyAutoencoder autoencoder(
    WEIGHT_E1, ENC1_ROWS, ENC1_COLS, BIAS_E1,
    WEIGHT_E2, ENC2_ROWS, ENC2_COLS, BIAS_E2,
    WEIGHT_D1, DEC1_ROWS, DEC1_COLS, BIAS_D1,
    WEIGHT_D2, DEC2_ROWS, DEC2_COLS, BIAS_D2,
    ANOMALY_THRESHOLD,
    NORM_MIN, NORM_MAX
);
bool anomalyDetected = false;
float anomalyScore = 0;

// ─── ПЕРЕМЕННЫЕ ДАТЧИКОВ И ИСПОЛНИТЕЛЕЙ ──────────────────────
float temperature = 0, humidity = 0;
int soilValue = 0, waterValue = 0, lightValue = 0, steamValue = 0;
long foodDistCm = 0;
bool fanOn = false, ledOn = false, pumpOn = false, feederOpen = false, rainAlert = false;
int lcdPage = 0;

// ─── ТАЙМЕРЫ И ИНТЕРВАЛЫ СРАБАТЫВАНИЯ ────────────────────────
unsigned long lastSensorRead = 0;
unsigned long lastWriteTime  = 0;
unsigned long lastMqttSend   = 0;
unsigned long lastBuzzer     = 0;
unsigned long feederOpenTime = 0;

const unsigned long writeInterval = 10000; // Интервал записи на SPIFFS (10 сек)
const unsigned long mqttInterval  = 10000; // Интервал отправки по MQTT (10 сек)

// Переменные обработки кнопки и пищалки
bool btnPrevState = HIGH;
unsigned long btnPressTime = 0;
bool buzzerState = false;

// ─── БЛОК СЕТЕВЫХ ФУНКЦИЙ (WI-FI И MQTT) ─────────────────────
void setup_wifi() {
    delay(10);
    Serial.print("\nПодключение к WiFi: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED) { 
        delay(500); 
        Serial.print(".");
        if (millis() - wifiStart >= 15000) {
            Serial.println("\nWiFi timeout, продолжаем без сети...");
            return;
        }
    }
    Serial.println("\nWiFi подключен успешно!");
}

void reconnect() {
    if (mqttClient.connect("ESP32_Farm_Client")) {
        Serial.println("Подключено к брокеру!");
        mqttClient.subscribe("farm/control");
    } else { 
        Serial.print("Ошибка MQTT, rc=");
        Serial.println(mqttClient.state());
    }
}

void sendDataToMqtt() {
    StaticJsonDocument<384> doc;
    doc["temp"]  = temperature;
    doc["hum"]   = humidity;
    doc["soil"]  = soilValue;
    doc["water"] = waterValue;
    doc["light"] = lightValue;
    doc["rain"]  = steamValue;
    doc["anomaly"] = anomalyDetected;
    doc["mse"]     = anomalyScore;

    char buffer[384];
    serializeJson(doc, buffer);
    mqttClient.publish("farm/logs", buffer);
    Serial.println("Пакет телеметрии отправлен на C++ сервер.");
}

// ─── БЛОК ОПРОСА ПЕРИФЕРИИ И ФАЙЛОВОЙ СИСТЕМЫ ────────────────
long measureDistance() {
    digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
    digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(PIN_TRIG, LOW);
    long duration = pulseIn(PIN_ECHO, HIGH, 30000);
    long dist = duration * 0.034 / 2;
    return (dist <= 0 || dist > 400) ? 400 : dist;
}

void updateLCD() {
    lcd.clear();
    char anomalyChar = anomalyDetected ? '!' : ' ';
    switch (lcdPage) {
        case 0:
            lcd.setCursor(0,0); lcd.print("T:"); lcd.print(temperature,1); lcd.print("C");
            lcd.setCursor(9,0); lcd.print("H:"); lcd.print((int)humidity); lcd.print("%");
            lcd.setCursor(0,1); lcd.print("Time: "); lcd.print(rtc.getTime("%H:%M:%S"));
            lcd.setCursor(14,1); lcd.print(anomalyChar); lcd.print("1");
            break;
        case 1:
            lcd.setCursor(0,0); lcd.print("Soil:"); lcd.print(soilValue > SOIL_DRY ? "DRY" : "OK");
            lcd.setCursor(0,1); lcd.print("Wat:"); lcd.print(waterValue > WATER_LOW ? "OK" : "LOW");
            lcd.setCursor(11,1); lcd.print("M:"); lcd.print(anomalyScore, 3);
            lcd.setCursor(14,0); lcd.print(anomalyChar); lcd.print("2");
            break;
        case 2:
            lcd.setCursor(0,0); lcd.print("Feed:"); lcd.print(feederOpen ? "OPEN" : "CLOSE");
            lcd.setCursor(0,1); lcd.print("Dist:"); lcd.print(foodDistCm); lcd.print("cm");
            lcd.setCursor(14,1); lcd.print(anomalyChar); lcd.print("3");
            break;
        case 3:
            lcd.setCursor(0,0); lcd.print(anomalyDetected ? "ANOMALY!" : "All OK  ");
            lcd.setCursor(0,1); lcd.print("MSE:"); lcd.print(anomalyScore, 2);
            lcd.setCursor(14,1); lcd.print(anomalyChar); lcd.print("4");
            break;
    }
}

void logToFile(String data) {
    File file = SPIFFS.open("/data.txt", FILE_READ);
    if (file && file.size() >= 50000) {
        file.close();
        SPIFFS.remove("/data.txt");
        Serial.println("SPIFFS log rotated (size limit)");
    }
    
    file = SPIFFS.open("/data.txt", FILE_APPEND);
    if (file) {
        file.println(rtc.getTime("%H:%M:%S") + "; " + data);
        file.close();
    }
}

void exportFileToSerial() {
    lcd.clear(); lcd.print("EXTRACTING...");
    File file = SPIFFS.open("/data.txt", FILE_READ);
    if (file) {
        Serial.println("\n--- START DATA ---");
        while (file.available()) Serial.write(file.read());
        Serial.println("--- END DATA ---");
        file.close();
    }
    delay(2000); 
    updateLCD();
}

// ─── НАСТРОЙКА СИСТЕМЫ (SETUP) ───────────────────────────────
void setup() {
    Serial.begin(115200);
    
    if (!SPIFFS.begin(true)) {
        Serial.println("FS Error (SPIFFS)");
    }

    setup_wifi();
    mqttClient.setServer(mqtt_server, 1883);

    dht.begin();
    feederServo.attach(PIN_SERVO);
    feederServo.write(SERVO_CLOSED);

    lcd.init();
    lcd.backlight();
    lcd.print("Farm Synced!");
    delay(1500);

    // Авто-синхронизация RTC по времени компиляции
    String cTime = __TIME__; 
    rtc.setTime(cTime.substring(6,8).toInt(), cTime.substring(3,5).toInt(), cTime.substring(0,2).toInt(), 12, 5, 2026);

    // Настройка портов ввода-вывода
    pinMode(PIN_FAN_IN1, OUTPUT); pinMode(PIN_FAN_IN2, OUTPUT);
    pinMode(PIN_BUZZER,  OUTPUT); pinMode(PIN_LED,     OUTPUT);
    pinMode(PIN_PUMP,    OUTPUT); pinMode(PIN_TRIG,    OUTPUT);
    pinMode(PIN_ECHO,    INPUT);  pinMode(PIN_PIR,     INPUT);
    pinMode(PIN_BUTTON,  INPUT_PULLUP);

    digitalWrite(PIN_PUMP, HIGH); // Помпа выключена (HIGH для реле)
    updateLCD();
}

// ─── ГЛАВНЫЙ ЦИКЛ ОБРАБОТКИ (LOOP) ───────────────────────────
void loop() {
    if (!mqttClient.connected()) reconnect();
    mqttClient.loop();

    unsigned long now = millis();

    // 1. Контур опроса датчиков и локальной автоматики (раз в 2 сек)
    if (now - lastSensorRead >= 2000) {
        lastSensorRead = now;
        float newTemp = dht.readTemperature();
        float newHum  = dht.readHumidity();
        if (!isnan(newTemp)) temperature = newTemp;
        if (!isnan(newHum)) humidity = newHum;
        soilValue   = analogRead(PIN_SOIL);
        waterValue  = analogRead(PIN_WATER_SENS);
        lightValue  = analogRead(PIN_PHOTO);
        steamValue  = analogRead(PIN_STEAM);
        foodDistCm  = measureDistance();
        
        // ─── TinyML: детекция аномалий ───
        float sensorVals[6] = {temperature, humidity, (float)soilValue, (float)waterValue, (float)lightValue, (float)steamValue};
        anomalyScore = autoencoder.computeMSE(sensorVals);
        anomalyDetected = autoencoder.isAnomaly(sensorVals);
        if (anomalyDetected) logToFile("ANOMALY! MSE=" + String(anomalyScore, 2));
        
        // Вентилятор охлаждения
        fanOn = (temperature >= TEMP_FAN_ON);
        if (!anomalyDetected) {
            digitalWrite(PIN_FAN_IN1, LOW);
            digitalWrite(PIN_FAN_IN2, fanOn ? HIGH : LOW);
        } else {
            digitalWrite(PIN_FAN_IN1, LOW);
            digitalWrite(PIN_FAN_IN2, LOW);
        }
        
        // Искусственное освещение
        ledOn = (lightValue < LIGHT_LED_ON);
        if (!anomalyDetected) digitalWrite(PIN_LED, ledOn ? HIGH : LOW);
        
        // Автоматический полив подложки
        pumpOn = (soilValue > SOIL_DRY && waterValue > WATER_LOW);
        if (!anomalyDetected) digitalWrite(PIN_PUMP, pumpOn ? LOW : HIGH);
        
        // Анализ критических осадков
        rainAlert = (steamValue > RAIN_THRESHOLD);
        updateLCD();
    }

    // 2. Локальное резервное логирование во внутренний SPIFFS (раз в 10 сек)
    if (now - lastWriteTime >= writeInterval) {
        lastWriteTime = now;
        String entry = "T:" + String(temperature,1) + "; H:" + String(humidity,0) + "; S:" + String(soilValue);
        logToFile(entry);
    }

    // 3. Отправка JSON-пакета на сервер C++ (раз в 10 сек)
    if (now - lastMqttSend >= mqttInterval) {
        lastMqttSend = now;
        sendDataToMqtt();
    }

    // 4. Логика управления заслонкой автоматической кормушки
    if (digitalRead(PIN_PIR) || foodDistCm > FOOD_DIST_CM) {
        if (!feederOpen) { 
            feederServo.write(SERVO_OPEN); 
            feederOpen = true; 
            feederOpenTime = now; 
        }
    }
    if (feederOpen && (now - feederOpenTime >= 5000)) {
        feederServo.write(SERVO_CLOSED); 
        feederOpen = false;
    }

    // 5. Опрос и устранение дребезга многофункциональной кнопки
    bool btnNow = digitalRead(PIN_BUTTON);
    if (btnPrevState == HIGH && btnNow == LOW) {
        btnPressTime = now; 
    }
    if (btnPrevState == LOW && btnNow == HIGH) {
        if (now - btnPressTime > 3000) {
            exportFileToSerial(); // Длинное удержание — дамп памяти
        } else { 
            lcdPage = (lcdPage + 1) % 4; // Короткое нажатие — смена страниц LCD
            updateLCD(); 
        }
    }
    btnPrevState = btnNow;

    // 6. Контур звуковой аварийной сигнализации (генерация тона без delay)
    if (rainAlert) {
        if (now - lastBuzzer >= 500) {
            lastBuzzer = now; 
            buzzerState = !buzzerState;
            digitalWrite(PIN_BUZZER, buzzerState ? HIGH : LOW);
        }
    } else {
        buzzerState = false;
        digitalWrite(PIN_BUZZER, LOW);
    }
}