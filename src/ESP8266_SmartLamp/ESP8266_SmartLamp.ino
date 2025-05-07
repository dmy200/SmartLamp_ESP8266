#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <time.h>

// Настройки Wi-Fi
const char* ssid = "SSID";
const char* password = "PASS";

// Прототипы функций
time_t getNextDSTTransition(time_t now, bool currentDST);
String getDSTStatus();
void syncTime();
void checkSchedule();
void setupNTP();

// Настройки лампы
const int lampPin = 2; // GPIO2

// Параметры по умолчанию
int turnOnHour = 8;
int turnOnMinute = 0;
int turnOffHour = 23;
int turnOffMinute = 0;
bool inverted = false;
bool usePWM = true;

// Состояние системы
enum LampMode { MANUAL_ON, MANUAL_OFF, AUTO };
LampMode lampMode = AUTO; // По умолчанию авторежим
bool lampState = false; // false = выключена, true = включена

bool fading = false;
bool fadeIn = true;
int fadeLevel = 0;
unsigned long fadeStart = 0;
const int fadeDuration = 60000; // 60 секунд

int wifiRSSI = 0; // Для хранения мощности сигнала
String ipAddress = "0.0.0.0"; // Для хранения IP-адреса

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 0, 60000);
ESP8266WebServer server(80);


// ======== Остальной код без изменений ========

void setupWiFi() {
  int retryCount = 0;
  const int maxRetries = 10;

  WiFi.begin(ssid, password);
  Serial.println("Подключаемся к WiFi...");
  
  while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
    delay(1000);
    Serial.print(".");
    retryCount++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    ipAddress = WiFi.localIP().toString();
    wifiRSSI = WiFi.RSSI();
    Serial.println("\nWiFi подключен");
    Serial.print("IP адрес: ");
    Serial.println(ipAddress);
    Serial.print("Мощность сигнала (RSSI): ");
    Serial.print(wifiRSSI);
    Serial.println(" dBm");
  } else {
    Serial.println("\nНе удалось подключиться к WiFi");
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi отключен. Пытаемся переподключиться...");
    WiFi.begin(ssid, password);
    int retryCount = 0;
    const int maxRetries = 10;

    while (WiFi.status() != WL_CONNECTED && retryCount < maxRetries) {
      delay(1000);
      Serial.print(".");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      ipAddress = WiFi.localIP().toString();
      wifiRSSI = WiFi.RSSI();
      Serial.println("\nWiFi переподключен");
      Serial.print("Новый IP: ");
      Serial.println(ipAddress);
      Serial.print("Мощность сигнала: ");
      Serial.print(wifiRSSI);
      Serial.println(" dBm");
    } else {
      Serial.println("\nНе удалось переподключиться");
    }
  }
}

bool isDST(int year, int month, int day, int hour, int weekday) {
  if (month < 3 || month > 10) return false;  // Зимнее время (не DST)
  if (month > 3 && month < 10) return true;   // Летнее время (DST)

  // Март: переход на DST в последнее воскресенье (02:00 → 03:00)
  if (month == 3) {
    int lastSunday = 31 - ((5 * year / 4 + 4) % 7);  // +4 для марта
    return (day > lastSunday) || (day == lastSunday && hour >= 3);
  }

  // Октябрь: переход на зимнее время в последнее воскресенье (03:00 → 02:00)
  if (month == 10) {
    int lastSunday = 31 - ((5 * year / 4 + 1) % 7);  // +1 для октября
    return (day < lastSunday) || (day == lastSunday && hour < 3);
  }

  return false;  // На всякий случай
}

void syncTime() {
    Serial.println("\n[Синхронизация] Начало синхронизации времени...");

    const char* ntpServers[] = {
        "time.google.com",
        "pool.ntp.org",
        "europe.pool.ntp.org",
        "time.windows.com",
        "time.nist.gov"
    };

    const int maxRetries = 3;
    const int baseTimeout = 2000;
    bool syncSuccess = false;
    unsigned long syncStart = millis();
    String usedServer = "";

    for (int serverIndex = 0; serverIndex < sizeof(ntpServers)/sizeof(ntpServers[0]); ++serverIndex) {
        usedServer = ntpServers[serverIndex];
        Serial.printf("[Синхронизация] Пробуем сервер %d: %s\n", serverIndex+1, usedServer.c_str());
        
        ntpUDP.stop();
        delay(100);
        ntpUDP.begin(123);
        timeClient.setPoolServerName(usedServer.c_str());
        
        for (int retry = 0; retry < maxRetries; ++retry) {
            int currentTimeout = baseTimeout * (retry + 1);
            Serial.printf("[Синхронизация] Попытка %d/%d (таймаут: %d мс)... ", 
                         retry+1, maxRetries, currentTimeout);
            
            timeClient.setUpdateInterval(0);
            bool updated = timeClient.forceUpdate();
            
            if (updated) {
                syncSuccess = true;
                Serial.println("Успех!");
                break;
            }
            Serial.println("Ошибка");
            if (retry < maxRetries-1) delay(currentTimeout);
        }
        
        if (syncSuccess) break;
    }

    if (!syncSuccess) {
        Serial.println("[Синхронизация] Критическая ошибка: не удалось синхронизировать время!");
        return;
    }

    // Получаем текущее время для определения DST
    time_t t = timeClient.getEpochTime();
    struct tm* timeinfo = localtime(&t);

    // Устанавливаем смещение с учетом DST
    bool dstActive = isDST(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                         timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_wday);
    int timeOffset = dstActive ? 10800 : 7200; // UTC+3 или UTC+2
    timeClient.setTimeOffset(timeOffset);

    // Вывод информации
    Serial.println("\n[Синхронизация] Успешно синхронизировано:");
    Serial.print("  Сервер: ");
    Serial.println(usedServer);
    Serial.print("  Текущее время: ");
    Serial.print(timeClient.getFormattedTime());
    Serial.print(" (UTC+");
    Serial.print(timeOffset / 3600);
    Serial.print(") Дата: ");
    Serial.print(timeinfo->tm_mday);
    Serial.print(".");
    Serial.print(timeinfo->tm_mon + 1);
    Serial.print(".");
    Serial.println(timeinfo->tm_year + 1900);
    Serial.printf("  Режим времени: %s\n", dstActive ? "Летнее" : "Зимнее");
    Serial.printf("  Общее время синхронизации: %lu мс\n", millis() - syncStart);
}

void setupNTP() {
    timeClient.begin();
    // Устанавливаем временное смещение (будет обновлено после синхронизации)
    timeClient.setTimeOffset(7200); // По умолчанию UTC+2
    timeClient.setUpdateInterval(3600000); // Обновлять время каждый час
}

void saveSettings() {
  EEPROM.put(0, turnOnHour);
  EEPROM.put(4, turnOnMinute);
  EEPROM.put(8, turnOffHour);
  EEPROM.put(12, turnOffMinute);
  EEPROM.put(16, inverted);
  EEPROM.put(20, usePWM);
  EEPROM.commit();
}

void loadSettings() {
  EEPROM.get(0, turnOnHour);
  EEPROM.get(4, turnOnMinute);
  EEPROM.get(8, turnOffHour);
  EEPROM.get(12, turnOffMinute);
  EEPROM.get(16, inverted);
  EEPROM.get(20, usePWM);

  // Валидация
  turnOnHour = constrain(turnOnHour, 0, 23);
  turnOnMinute = constrain(turnOnMinute, 0, 59);
  turnOffHour = constrain(turnOffHour, 0, 23);
  turnOffMinute = constrain(turnOffMinute, 0, 59);
}

void setLamp(bool on) {
  if (lampState == on) return; // Если состояние не меняется, ничего не делаем

  lampState = on; // Обновляем состояние
  if (usePWM) {
    analogWrite(lampPin, inverted ? (on ? 0 : 255) : (on ? 255 : 0));
  } else {
    digitalWrite(lampPin, on ^ inverted);
  }
}

void startFade(bool fadeInDirection) {
  fading = true;
  fadeIn = fadeInDirection;
  fadeStart = millis();
  fadeLevel = fadeIn ? 0 : 255;
}

void updateFade() {
  if (!fading) return;
  
  unsigned long elapsed = millis() - fadeStart;
  
  // Завершение fade
  if (elapsed >= fadeDuration) {
    fading = false;
    setLamp(fadeIn); // Устанавливаем конечное состояние
    return;
  }

  // Квадратичная функция для более естественного fade
  float progress = (float)elapsed / fadeDuration;
  
  // Применяем разные кривые для fade-in и fade-out
  if (fadeIn) {
    // Квадратичная функция для плавного ускорения при включении
    fadeLevel = (int)(255.0f * progress * progress);
  } else {
    // Инвертированная квадратичная для плавного замедления при выключении
    progress = 1.0f - progress;
    fadeLevel = 255 - (int)(255.0f * progress * progress);
  }

  // Применяем инверсию сигнала, если нужно
  int outputLevel = inverted ? (255 - fadeLevel) : fadeLevel;
  
  // Устанавливаем яркость
  if (usePWM) {
    analogWrite(lampPin, outputLevel);
  } else {
    // Для цифрового выхода используем пороговое значение
    digitalWrite(lampPin, (outputLevel >= 128) ? HIGH : LOW);
  }
}

void checkSchedule() {
    if (lampMode != AUTO || fading) return;

    // Получаем текущее время с учетом DST
    time_t t = timeClient.getEpochTime();
    struct tm* timeinfo = localtime(&t);
    
    // Обновляем смещение на случай изменения DST
    bool dstActive = isDST(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                         timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_wday);
    timeClient.setTimeOffset(dstActive ? 10800 : 7200);
    
    // Получаем текущие часы и минуты
    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    
    // Проверяем, должна ли лампа быть включена
    bool shouldBeOn = (h > turnOnHour || (h == turnOnHour && m >= turnOnMinute)) &&
                     (h < turnOffHour || (h == turnOffHour && m < turnOffMinute));

    // Управление лампой
    if (shouldBeOn && !lampState) {
        if (usePWM && fadeDuration > 0) {
            startFade(true);
        } else {
            setLamp(true);
        }
    } else if (!shouldBeOn && lampState) {
        if (usePWM && fadeDuration > 0) {
            startFade(false);
        } else {
            setLamp(false);
        }
    }
}

String getCurrentDateTime() {
  time_t now = timeClient.getEpochTime();
  struct tm* timeinfo = localtime(&now);
  char buf[30];
  strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", timeinfo);
  return String(buf);
}

String getDSTStatus() {
    if (!timeClient.isTimeSet()) {
        return "Время не синхронизировано";
    }
    
    time_t now = timeClient.getEpochTime();
    struct tm* timeinfo = localtime(&now);
    
    bool dstActive = isDST(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                         timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_wday);
    int offsetHours = dstActive ? 3 : 2;
    
    char nextChange[50];
    time_t nextTransition = getNextDSTTransition(now, dstActive);
    struct tm* changeTime = localtime(&nextTransition);
    strftime(nextChange, sizeof(nextChange), "%d.%m.%Y %H:%M", changeTime);
    
    return String(dstActive ? "Летнее время (UTC+3)" : "Зимнее время (UTC+2)") + 
           "\nСледующее изменение: " + String(nextChange);
}

String getLampStatus() {
  if (fading) {
    int progress = (millis() - fadeStart) * 100 / fadeDuration;
    progress = constrain(progress, 0, 100);
    return String("Плавное ") + (fadeIn ? "включение: " : "выключение: ") + progress + "%";
  }
  
  if (lampMode == MANUAL_ON) return "Включена (ручной)";
  if (lampMode == MANUAL_OFF) return "Выключена (ручной)";
  
  int h = timeClient.getHours();
  int m = timeClient.getMinutes();
  bool shouldBeOn = (h > turnOnHour || (h == turnOnHour && m >= turnOnMinute)) &&
                   (h < turnOffHour || (h == turnOffHour && m < turnOffMinute));
  return shouldBeOn ? "Включена (авто)" : "Выключена (авто)";
}

time_t getNextDSTTransition(time_t now, bool currentDST) {
    struct tm* tm = localtime(&now);
    int year = tm->tm_year + 1900;
    
    // Март: последнее воскресенье в 3:00 (переход на летнее время)
    tm->tm_mon = 2; // Март
    tm->tm_mday = 31;
    tm->tm_hour = 3;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    
    // Находим последнее воскресенье марта
    while (tm->tm_wday != 0) {
        tm->tm_mday--;
        mktime(tm); // Обновляем tm_wday
    }
    time_t marchTransition = mktime(tm);
    
    // Октябрь: последнее воскресенье в 3:00 (переход на зимнее время)
    tm->tm_mon = 9; // Октябрь
    tm->tm_mday = 31;
    tm->tm_hour = 3;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    
    // Находим последнее воскресенье октября
    while (tm->tm_wday != 0) {
        tm->tm_mday--;
        mktime(tm);
    }
    time_t octoberTransition = mktime(tm);
    
    // Определяем следующее изменение
    if (currentDST) {
        return (octoberTransition > now) ? octoberTransition : marchTransition + 31536000; // +1 год
    } else {
        return (marchTransition > now) ? marchTransition : octoberTransition;
    }
}

void handleStatus() {
  String json = "{";
  json += "\"datetime\":\"" + getCurrentDateTime() + "\",";
  json += "\"dst\":\"" + getDSTStatus() + "\",";
  json += "\"lamp\":\"" + getLampStatus() + "\",";
  json += "\"mode\":\"" + String(lampMode == AUTO ? "auto" : (lampMode == MANUAL_ON ? "on" : "off")) + "\",";
  json += "\"rssi\":" + String(wifiRSSI) + ",";
  json += "\"ip\":\"" + ipAddress + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleManualOn() {
  lampMode = MANUAL_ON;
  setLamp(true);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleManualOff() {
  lampMode = MANUAL_OFF;
  setLamp(false);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleAutoMode() {
  lampMode = AUTO;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSave() {
  turnOnHour = constrain(server.arg("onH").toInt(), 0, 23);
  turnOnMinute = constrain(server.arg("onM").toInt(), 0, 59);
  turnOffHour = constrain(server.arg("offH").toInt(), 0, 23);
  turnOffMinute = constrain(server.arg("offM").toInt(), 0, 59);
  usePWM = server.arg("pwm") == "on";
  inverted = server.arg("inv") == "on";
  saveSettings();
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleRoot() {
  String html = "<html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body {font-family:sans-serif;max-width:600px;margin:0 auto;padding:20px}";
  html += ".btn {display:inline-block;padding:10px 15px;margin:5px;text-decoration:none;color:white;border-radius:5px}";
  html += ".on {background-color:#4CAF50}";
  html += ".off {background-color:#f44336}";
  html += ".auto {background-color:#2196F3}";
  html += ".active {border:3px solid #000; font-weight:bold}";
  html += ".status-box {border:1px solid #ddd;padding:15px;margin-bottom:20px;border-radius:5px}";
  html += ".summer {background-color:#ffe6cc}";
  html += ".winter {background-color:#e6f3ff}";
  html += "</style>";
  html += "<script>";
  html += "function updateClock(){";
  html += "var xhr=new XMLHttpRequest();";
  html += "xhr.open('GET','/status',true);";
  html += "xhr.onload=function(){";
  html += "var data=JSON.parse(this.responseText);";
  html += "document.getElementById('datetime').innerHTML=data.datetime;";
  html += "document.getElementById('dst').innerHTML=data.dst;";
  html += "document.getElementById('lamp').innerHTML=data.lamp;";
  html += "document.getElementById('btn-on').className=data.mode=='on'?'btn on active':'btn on';";
  html += "document.getElementById('btn-off').className=data.mode=='off'?'btn off active':'btn off';";
  html += "document.getElementById('btn-auto').className=data.mode=='auto'?'btn auto active':'btn auto';";
  html += "};";
  html += "xhr.send();";
  html += "setTimeout(updateClock,1000)}";
  html += "window.onload=function(){updateClock()};";
  html += "</script>";
  html += "</head><body>";
  html += "<h1>Умная лампа</h1>";

  // Блок статуса
  html += "<div class='status-box " + String(getDSTStatus().indexOf("Летнее") != -1 ? "summer" : "winter") + "'>";
  html += "<p><b>Дата и время:</b> <span id='datetime'>" + getCurrentDateTime() + "</span></p>";
  html += "<p><b>Режим времени:</b> <span id='dst'>" + getDSTStatus() + "</span></p>";
  html += "<p><b>Состояние лампы:</b> <span id='lamp'>" + getLampStatus() + "</span></p>";
  html += "</div>";

  // Кнопки управления
  html += "<div style='margin:20px 0;text-align:center'>";
  html += "<a id='btn-on' href='/manual_on' class='btn " + String(lampMode == MANUAL_ON ? "on active" : "on") + "'>Включить</a>";
  html += "<a id='btn-off' href='/manual_off' class='btn " + String(lampMode == MANUAL_OFF ? "off active" : "off") + "'>Выключить</a>";
  html += "<a id='btn-auto' href='/auto_mode' class='btn " + String(lampMode == AUTO ? "auto active" : "auto") + "'>Авто</a>";
  html += "</div>";

  // Настройки
  html += "<h2>Настройки расписания</h2>";
  html += "<form method='POST' action='/save'>";
  html += "Включение:  <input name='onH' type='number' min='0' max='23' value='" + String(turnOnHour) + "'>";
  html += ":<input name='onM' type='number' min='0' max='59' value='" + String(turnOnMinute) + "'><br>";
  html += "Выключение: <input name='offH' type='number' min='0' max='23' value='" + String(turnOffHour) + "'>";
  html += ":<input name='offM' type='number' min='0' max='59' value='" + String(turnOffMinute) + "'><br><br>";
  html += "<label><input type='checkbox' name='pwm' " + String(usePWM ? "checked" : "") + "> Использовать PWM</label><br>";
  html += "<label><input type='checkbox' name='inv' " + String(inverted ? "checked" : "") + "> Инвертировать сигнал</label><br><br>";
  html += "<input type='submit' value='Сохранить настройки'>";
  html += "</form>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  // Отключаем WDT на время инициализации (если нужно)
  ESP.wdtDisable(); 
  EEPROM.begin(64);
  pinMode(lampPin, OUTPUT);
  
  // Инициализация - выключаем лампу
  setLamp(false);
  delay(1000);
  loadSettings();
  setupWiFi();
  
  setupNTP();
  
  // Ждем синхронизации времени
  Serial.println("Waiting for time sync...");
  syncTime();
  
  // Настройка сервера
  server.on("/", handleRoot);
  server.on("/manual_on", handleManualOn);
  server.on("/manual_off", handleManualOff);
  server.on("/auto_mode", handleAutoMode);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/status", handleStatus);
  
  server.begin();
  Serial.println("HTTP server started");
  
  // Включаем аппаратный Watchdog (таймаут ~3.2 сек)
  ESP.wdtEnable(5000); // Таймаут в миллисекундах (макс. ~8.3 сек)
}

void loop() {
  // Сбрасываем Watchdog
  ESP.wdtFeed();
  
  // Обработка HTTP-запросов
  server.handleClient();

  // Проверка и поддержание WiFi-соединения
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }

  // Обновление времени (с защитой от частых вызовов)
  static unsigned long lastTimeUpdate = 0;
  if (millis() - lastTimeUpdate >= 60000) { // Каждые 60 секунд
    if (WiFi.status() == WL_CONNECTED) {
      if (!timeClient.update()) {
        Serial.println("[NTP] Автообновление времени не удалось, пробуем принудительно...");
        syncTime(); // Полная процедура синхронизации при ошибке
      } else {
        // После успешного обновления проверяем DST
        time_t t = timeClient.getEpochTime();
        struct tm* timeinfo = localtime(&t);
        bool dstActive = isDST(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                             timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_wday);
        timeClient.setTimeOffset(dstActive ? 10800 : 7200);
      }
    }
    lastTimeUpdate = millis();
  }

  // Управление лампой в автоматическом режиме
  if (lampMode == AUTO) {
    checkSchedule();
  }

  // Обработка плавного изменения яркости
  updateFade();

  // Диагностика и логирование (каждые 30 секунд)
  static unsigned long lastDiagnosticTime = 0;
  if (millis() - lastDiagnosticTime >= 30000) {
    // Обновление информации о сети
    if (WiFi.status() == WL_CONNECTED) {
      wifiRSSI = WiFi.RSSI();
      ipAddress = WiFi.localIP().toString();
    }
    
    // Вывод диагностической информации
    Serial.println("\n=== Диагностика ===");
    Serial.print("WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "Подключено" : "Отключено");
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("IP: ");
      Serial.println(ipAddress);
      Serial.print("RSSI: ");
      Serial.print(wifiRSSI);
      Serial.println(" dBm");
    }
    
    // Информация о времени
    Serial.print("Время: ");
    if (timeClient.isTimeSet()) {
      time_t t = timeClient.getEpochTime();
      struct tm* timeinfo = localtime(&t);
      bool dstActive = isDST(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                           timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_wday);
      
      Serial.print(timeClient.getFormattedTime());
      Serial.print(" (UTC+");
      Serial.print(dstActive ? "3" : "2");
      Serial.print(") Дата: ");
      Serial.print(timeinfo->tm_mday);
      Serial.print(".");
      Serial.print(timeinfo->tm_mon + 1);
      Serial.print(".");
      Serial.print(timeinfo->tm_year + 1900);
      Serial.print(" ");
      Serial.println(dstActive ? "Летнее время" : "Зимнее время");
    } else {
      Serial.println("Не синхронизировано");
    }
    
    // Состояние лампы
    Serial.print("Лампа: ");
    Serial.println(getLampStatus());
    
    Serial.println("===================");
    lastDiagnosticTime = millis();
  }

  // Фоновая синхронизация времени (каждые 6 часов)
  static unsigned long lastFullSync = 0;
  if (millis() - lastFullSync >= 21600000UL && WiFi.status() == WL_CONNECTED) {
    Serial.println("[NTP] Фоновая синхронизация времени...");
    syncTime();
    lastFullSync = millis();
  }

  // Небольшая задержка для стабильности
  delay(10);
}

