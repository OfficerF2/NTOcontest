#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESPConnect.h>
#include <ESPAsyncWebServer.h>

String BOTtoken = "ТОКЕН-БОТА";
String CHAT_ID = "ВАШ-USERID";
int botRequestDelay = 1000; //задержка для проверки новых сообщений

AsyncWebServer server(80);
WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);
bool sendPhoto = false;
bool flashState = LOW;
unsigned long lastTimeBotRan;
#define FLASH_LED_PIN 4

void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  }
  
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Инициализация камеры завершилась с ошибкой: 0x%x", err);
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_XGA);  // Доступные разрешения камеры: UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA
}

void setup(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200); //Монитор порта

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, flashState);

  configInitCamera();

  WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, password);
  ESPConnect.autoConnect("Название точки доступа для менеджера"); //Подключение к wifi

  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Сертификат для api.telegram.org
  while (!ESPConnect.begin(&server)) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
}

void handleNewMessages(int numNewMessages) {
  Serial.print("Ожидаем новые сообщения: ");
  Serial.println(numNewMessages);
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID){
      bot.sendMessage(chat_id, "Неавторизованный пользователь", "");
      continue;
    }
    
    String text = bot.messages[i].text;
    Serial.println(text); //присланное сообщение
    String from_name = bot.messages[i].from_name;

    //Меню: (Тут можно добавить свои команды)
    if (text == "/start") {
      String welcome = "Привет , " + from_name + "!\n";
      welcome += "Доступные команды:\n";
      welcome += "/photo : сделать фото\n";
      welcome += "/flash : включить светодиод\n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
    if (text == "/flash") {
      flashState = !flashState;
      digitalWrite(FLASH_LED_PIN, flashState);
      Serial.println("Изменяем состояние светодиода");
    }
    if (text == "/photo") {
      sendPhoto = true;
      Serial.println("Новый запрос на фотографию");
    }
  }
}

String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Ошибка камеры");
    delay(1000);
    ESP.restart();
    return "Ошибка камеры";
  }  
  
  Serial.println("Подключаемя к " + String(myDomain));

  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Успешное подключение");
    
    String head = "--Amperkot\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--Amperkot\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--Amperkot--\r\n";

    uint16_t imageLen = fb->len;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=Amperkot");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Не удалось подключиться к api.telegram.org.";
    Serial.println("Не удалось подключиться к api.telegram.org.");
  }
  return getBody;
}
void loop() {
  if (sendPhoto) {
    Serial.println("Готовим фото");
    sendPhotoTelegram(); 
    sendPhoto = false; 
  }
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("Ответ получен!");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}