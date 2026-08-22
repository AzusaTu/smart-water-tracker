#include "CloudSync.h"
#include <WiFi.h>
#include <ArduinoJson.h>

CloudSync::CloudSync()
    : _cloudEndpoint(""),
      _apiKey(""),
      _deviceId(""),
      _queueHead(0),
      _queueTail(0),
      _queueCount(0),
      _lastSyncAttempt(0) {
}

void CloudSync::begin() {
    loadConfig();

    // 生成基於 MAC 位址的唯一裝置代碼
    char idBuf[32];
    snprintf(idBuf, sizeof(idBuf), "water_c3_%08X", (uint32_t)ESP.getEfuseMac());
    _deviceId = String(idBuf);

    // 啟動 NTP 網路時間校準 (台灣時區 GMT+8)
    configTime(8 * 3600, 0, "pool.ntp.org", "time.google.com", "time.nist.gov");
    Serial.printf("[CloudSync] 雲端同步模組啟動. 裝置 ID: %s\n", _deviceId.c_str());
}

String CloudSync::getFormattedTime(time_t rawTime) {
    if (rawTime == 0) {
        rawTime = time(nullptr);
    }
    struct tm timeinfo;
    localtime_r(&rawTime, &timeinfo);

    char buf[32];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    return String(buf);
}

void CloudSync::queueEvent(const char* eventType, int amountMl, int remainingMl, int todayTotalMl, int dailyGoalMl) {
    if (_queueCount >= QUEUE_SIZE) {
        // 佇列滿時，覆蓋最舊的一筆
        _queueTail = (_queueTail + 1) % QUEUE_SIZE;
        _queueCount--;
    }

    CloudEvent ev;
    ev.timestamp = time(nullptr);
    strncpy(ev.eventType, eventType, sizeof(ev.eventType) - 1);
    ev.eventType[sizeof(ev.eventType) - 1] = '\0';
    ev.amountMl = amountMl;
    ev.remainingMl = remainingMl;
    ev.todayTotalMl = todayTotalMl;
    ev.dailyGoalMl = dailyGoalMl;

    _eventQueue[_queueHead] = ev;
    _queueHead = (_queueHead + 1) % QUEUE_SIZE;
    _queueCount++;

    Serial.printf("[CloudSync] 新增事件至同步佇列 (目前待傳送: %d 筆, 時間: %s)\n",
                  _queueCount, getFormattedTime(ev.timestamp).c_str());
}

void CloudSync::update() {
    if (_queueCount == 0) return;
    if (_cloudEndpoint.length() < 8) return;
    if (WiFi.status() != WL_CONNECTED) return;

    // 每 2 秒嘗試同步一筆
    if (millis() - _lastSyncAttempt < 2000) return;
    _lastSyncAttempt = millis();

    CloudEvent ev = _eventQueue[_queueTail];
    sendEventDirect(ev);
}

void CloudSync::sendEventDirect(const CloudEvent& ev) {
    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    bool isHttps = _cloudEndpoint.startsWith("https://");
    bool beginOk = false;

    if (isHttps) {
        secureClient.setInsecure();
        beginOk = http.begin(secureClient, _cloudEndpoint);
    } else {
        beginOk = http.begin(plainClient, _cloudEndpoint);
    }

    if (!beginOk) {
        Serial.println("[CloudSync] HTTP Client 初始化失敗");
        return;
    }

    http.addHeader("Content-Type", "application/json");
    if (_apiKey.length() > 0) {
        http.addHeader("apikey", _apiKey);
        http.addHeader("Authorization", "Bearer " + _apiKey);
    }

    // 建立 JSON Payload
    JsonDocument doc;
    doc["device_id"] = _deviceId;
    doc["timestamp"] = (long)ev.timestamp;
    doc["datetime"] = getFormattedTime(ev.timestamp);
    doc["event_type"] = ev.eventType;
    doc["amount_ml"] = ev.amountMl;
    doc["cup_remaining_ml"] = ev.remainingMl;
    doc["today_total_ml"] = ev.todayTotalMl;
    doc["daily_goal_ml"] = ev.dailyGoalMl;

    String jsonStr;
    serializeJson(doc, jsonStr);

    int httpCode = http.POST(jsonStr);
    if (httpCode >= 200 && httpCode < 300) {
        Serial.printf("[CloudSync] ✅ 雲端同步成功 (HTTP %d): %s +%dml\n",
                      httpCode, ev.eventType, ev.amountMl);
        // 成功，自佇列中移除
        _queueTail = (_queueTail + 1) % QUEUE_SIZE;
        _queueCount--;
    } else {
        Serial.printf("[CloudSync] ❌ 雲端同步失敗 (HTTP %d), 將於稍後重試\n", httpCode);
    }
    http.end();
}

void CloudSync::setCloudEndpoint(const String& endpoint) {
    _cloudEndpoint = endpoint;
    saveConfig();
}

void CloudSync::setApiKey(const String& key) {
    _apiKey = key;
    saveConfig();
}

void CloudSync::saveConfig() {
    _prefs.begin(PREFS_NAMESPACE, false);
    _prefs.putString("cloud_url", _cloudEndpoint);
    _prefs.putString("cloud_key", _apiKey);
    _prefs.end();
}

void CloudSync::loadConfig() {
    _prefs.begin(PREFS_NAMESPACE, true);
    _cloudEndpoint = _prefs.getString("cloud_url", "");
    _apiKey = _prefs.getString("cloud_key", "");
    _prefs.end();
}
