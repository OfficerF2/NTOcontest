#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>
#include <PCA9634.h>
#include <WiFi.h>
#include <WebServer.h>

// Настройки датчиков
Adafruit_BME280 bme;
BH1750 lightMeter;
PCA9634 ledDriver(0x1C); // Адрес PCA9634

// Настройки WiFi
const char* ssid = "POCO M3";
const char* password = "007008009124";
WebServer server(80);

// Состояние системы
bool yellowLedOn = false;

// Данные с датчиков
struct {
  float temperature = 0;
  float humidity = 0;
  float pressure = 0;
  float lux = 0;
  String lastUpdate = "";
} sensorData;

void setup() {
  Serial.begin(115200);

  // Инициализация датчиков
  if (!bme.begin(0x76)) {
    Serial.println("Ошибка инициализации BME280!");
    while (1);
  }
  
  lightMeter.begin();
  
  // Инициализация светодиодов
  Wire.begin();
  ledDriver.begin();
  for (int i = 0; i < ledDriver.channelCount(); i++) {
    ledDriver.setLedDriverMode(i, PCA9634_LEDPWM);
  }
  setYellowLED(false); // Выключить при старте

  // Подключение к WiFi
  WiFi.begin(ssid, password);
  Serial.print("Подключение к WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nПодключено! IP: " + WiFi.localIP().toString());

  // Настройка веб-сервера
  server.on("/", handleRoot);
  server.on("/data", handleSensorData);
  server.on("/yellow/toggle", handleToggleYellow);
  server.begin();
}

void loop() {
  server.handleClient();
  updateSensorData();
}

void updateSensorData() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 2000) {
    lastUpdate = millis();
    
    sensorData.temperature = bme.readTemperature();
    sensorData.humidity = bme.readHumidity();
    sensorData.pressure = bme.readPressure() / 100.0F;
    sensorData.lux = lightMeter.readLightLevel();
    sensorData.lastUpdate = String(millis() / 1000) + " сек назад";
    
    Serial.println("Температура: " + String(sensorData.temperature, 1) + " °C");
    Serial.println("Влажность: " + String(sensorData.humidity, 1) + " %");
    Serial.println("Давление: " + String(sensorData.pressure, 1) + " гПа");
    Serial.println("Освещенность: " + String(sensorData.lux, 1) + " лк");
  }
}

void setYellowLED(bool state) {
  yellowLedOn = state;
  if (state) {
    // Включить желтый (красный + зеленый)
  ledDriver.write1(3, 0x90);
  ledDriver.write1(2, 0x90);
  ledDriver.write1(5, 0x00);
    Serial.println("Желтый светодиод включен");
  } else {
    // Выключить все каналы
  ledDriver.write1(3, 0x00);
  ledDriver.write1(2, 0x00);
  ledDriver.write1(5, 0x00);
    Serial.println("Желтый светодиод выключен");
  }
}

void handleToggleYellow() {
  setYellowLED(!yellowLedOn);
  server.send(200, "text/plain", yellowLedOn ? "ON" : "OFF");
}

void handleRoot() {
  String html = R"=====(
<!DOCTYPE html>
<html lang='ru'>
<head>
  <meta charset='UTF-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1.0'>
  <title>Метеостанция</title>
  <style>
    body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }
    .card { background: white; border-radius: 10px; padding: 20px; margin-bottom: 15px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
    h1 { color: #2c3e50; text-align: center; }
    .value { font-size: 24px; font-weight: bold; color: #2980b9; }
    button { 
      background-color: #4CAF50; 
      color: white; 
      border: none; 
      padding: 15px 30px; 
      margin: 10px; 
      border-radius: 5px; 
      cursor: pointer; 
      font-size: 18px;
    }
    button.off { background-color: #f44336; }
    .update-time { text-align: right; font-style: italic; color: #95a5a6; }
    .led-status { 
      display: inline-block; 
      width: 20px; 
      height: 20px; 
      border-radius: 50%; 
      margin-left: 10px;
      background-color: #ccc;
    }
    .led-on { background-color: yellow; box-shadow: 0 0 10px yellow; }
  </style>
  <script>
    let ledState = false;
    
    function updateData() {
      fetch('/data')
        .then(r => r.json())
        .then(data => {
          document.getElementById('temp').innerText = data.temp + ' °C';
          document.getElementById('hum').innerText = data.hum + ' %';
          document.getElementById('press').innerText = data.press + ' гПа';
          document.getElementById('lux').innerText = data.lux + ' лк';
          document.getElementById('update').innerText = 'Обновлено: ' + data.time;
        });
    }
    
    function toggleLED() {
      fetch('/yellow/toggle')
        .then(r => r.text())
        .then(state => {
          ledState = state === 'ON';
          updateLEDButton();
        });
    }
    
    function updateLEDButton() {
      const btn = document.getElementById('ledBtn');
      const status = document.getElementById('ledStatus');
      
      if(ledState) {
        btn.className = '';
        btn.innerText = 'Выключить желтый свет';
        status.className = 'led-status led-on';
      } else {
        btn.className = 'off';
        btn.innerText = 'Включить желтый свет';
        status.className = 'led-status';
      }
    }
    
    // Обновлять данные каждые 2 секунды
    setInterval(updateData, 100);
    // Загрузить данные при старте
    window.onload = function() {
      updateData();
      updateLEDButton();
    };
  </script>
</head>
<body>
  <h1>Метеостанция с освещением</h1>
  
  <div class="card">
    <h2>Температура воздуха</h2>
    <div class="value" id="temp">--</div>
  </div>
  
  <div class="card">
    <h2>Влажность воздуха</h2>
    <div class="value" id="hum">--</div>
  </div>
  
  <div class="card">
    <h2>Атмосферное давление</h2>
    <div class="value" id="press">--</div>
  </div>
  
  <div class="card">
    <h2>Освещенность</h2>
    <div class="value" id="lux">--</div>
  </div>
  
  <div class="card">
    <h2>Управление светодиодом</h2>
    <button id="ledBtn" onclick="toggleLED()">Включить желтый свет</button>
    <span id="ledStatus" class="led-status"></span>
  </div>
  
  <div class="update-time" id="update">Обновлено: никогда</div>
</body>
</html>
)=====";
  server.send(200, "text/html", html);
}

void handleSensorData() {
  String json = "{";
  json += "\"temp\":" + String(sensorData.temperature, 1) + ",";
  json += "\"hum\":" + String(sensorData.humidity, 1) + ",";
  json += "\"press\":" + String(sensorData.pressure, 1) + ",";
  json += "\"lux\":" + String(sensorData.lux, 1) + ",";
  json += "\"time\":\"" + sensorData.lastUpdate + "\"";
  json += "}";
  
  server.send(200, "application/json", json);
}