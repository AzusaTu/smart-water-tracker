#include "Notifier.h"
#include <WiFi.h>

Notifier::Notifier()
    : _ledPin(PIN_STATUS_LED),
      _currentPattern(LED_OFF),
      _patternStartTime(0),
      _lastLedToggle(0),
      _ledState(false),
      _blinkCounter(0) {
}

void Notifier::begin(uint8_t ledPin) {
    _ledPin = ledPin;
    pinMode(_ledPin, OUTPUT);
    setLedHardware(false); // 預設關閉

    loadConfig();
    Serial.println("[Notifier] 通知模組啟動完成");
}

void Notifier::setLedHardware(bool on) {
    _ledState = on;
    // ESP32-C3 SuperMini 板載 LED 是低電位觸發 (Active LOW)
    digitalWrite(_ledPin, on ? LOW : HIGH);
}

void Notifier::update() {
    unsigned long now = millis();

    switch (_currentPattern) {
        case LED_OFF:
            if (_ledState) setLedHardware(false);
            break;

        case LED_SOLID:
            if (!_ledState) setLedHardware(true);
            break;

        case LED_BLINK_SHORT:
            // 喝水成功：雙閃 (100ms on, 100ms off, 100ms on, 100ms off -> 結束)
            if (now - _lastLedToggle > 100) {
                _lastLedToggle = now;
                _blinkCounter++;
                setLedHardware(!_ledState);

                if (_blinkCounter >= 4) {
                    _currentPattern = LED_OFF;
                    setLedHardware(false);
                }
            }
            break;

        case LED_ALERT_PULSE:
            // 久未喝水警示：持續慢閃 (500ms on, 500ms off)
            if (now - _lastLedToggle > 500) {
                _lastLedToggle = now;
                setLedHardware(!_ledState);
            }
            break;
    }
}

void Notifier::notifyDrink(int amountMl, int todayTotalMl, int goalMl) {
    _currentPattern = LED_BLINK_SHORT;
    _patternStartTime = millis();
    _lastLedToggle = millis();
    _blinkCounter = 0;
    setLedHardware(true);

    if (WiFi.status() == WL_CONNECTED) {
        char msg[128];
        snprintf(msg, sizeof(msg), "🥤 喝水紀錄: +%d ml\n今日進度: %d / %d ml (%.1f%%)",
                 amountMl, todayTotalMl, goalMl, (goalMl > 0) ? (todayTotalMl * 100.0f / goalMl) : 0.0f);

        sendLineNotify(String(msg));

        char json[256];
        snprintf(json, sizeof(json),
                 "{\"event\":\"drink\",\"amount_ml\":%d,\"today_total_ml\":%d,\"goal_ml\":%d}",
                 amountMl, todayTotalMl, goalMl);
        sendWebhook(String(json));
    }
}

void Notifier::notifyRefill(int amountMl, int remainingMl) {
    _currentPattern = LED_BLINK_SHORT;
    _patternStartTime = millis();
    _lastLedToggle = millis();
    _blinkCounter = 2; // 單閃一次
    setLedHardware(true);

    if (WiFi.status() == WL_CONNECTED) {
        char json[128];
        snprintf(json, sizeof(json),
                 "{\"event\":\"refill\",\"amount_ml\":%d,\"remaining_ml\":%d}",
                 amountMl, remainingMl);
        sendWebhook(String(json));
    }
}

void Notifier::notifyReminder(int minutesElapsed, int todayTotalMl, int goalMl) {
    _currentPattern = LED_ALERT_PULSE;
    _lastLedToggle = millis();

    if (WiFi.status() == WL_CONNECTED) {
        char msg[128];
        snprintf(msg, sizeof(msg), "⏰ 該喝水囉！\n您已經 %d 分鐘沒有喝水了。\n今日已喝: %d / %d ml",
                 minutesElapsed, todayTotalMl, goalMl);

        sendLineNotify(String(msg));

        char json[256];
        snprintf(json, sizeof(json),
                 "{\"event\":\"reminder\",\"inactive_minutes\":%d,\"today_total_ml\":%d,\"goal_ml\":%d}",
                 minutesElapsed, todayTotalMl, goalMl);
        sendWebhook(String(json));
    }
}

void Notifier::stopAlert() {
    _currentPattern = LED_OFF;
    setLedHardware(false);
}

void Notifier::sendLineNotify(const String& message) {
    if (_lineToken.length() < 10) return;

    WiFiClientSecure client;
    client.setInsecure(); // 略過 SSL 證書驗證

    HTTPClient http;
    if (http.begin(client, "https://notify-api.line.me/api/notify")) {
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.addHeader("Authorization", "Bearer " + _lineToken);

        String postBody = "message=\n" + message;
        int httpCode = http.POST(postBody);
        Serial.printf("[Notifier] LINE Notify 發送結果代碼: %d\n", httpCode);
        http.end();
    }
}

void Notifier::sendWebhook(const String& jsonPayload) {
    if (_webhookUrl.length() < 8) return;

    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;

    bool isHttps = _webhookUrl.startsWith("https://");
    bool beginOk = false;

    if (isHttps) {
        secureClient.setInsecure();
        beginOk = http.begin(secureClient, _webhookUrl);
    } else {
        beginOk = http.begin(plainClient, _webhookUrl);
    }

    if (beginOk) {
        http.addHeader("Content-Type", "application/json");
        int httpCode = http.POST(jsonPayload);
        Serial.printf("[Notifier] Webhook 發送結果代碼: %d\n", httpCode);
        http.end();
    }
}

bool Notifier::testPushNotification(String& outMessage) {
    if (WiFi.status() != WL_CONNECTED) {
        outMessage = "WiFi 尚未連線，無法發送推播測試";
        return false;
    }

    if (_lineToken.length() == 0 && _webhookUrl.length() == 0) {
        outMessage = "尚未設定 LINE Token 或 Webhook URL";
        return false;
    }

    notifyDrink(100, 500, 2000);
    outMessage = "已發送測試推播訊息";
    return true;
}

void Notifier::setLineToken(const String& token) {
    _lineToken = token;
    saveConfig();
}

void Notifier::setWebhookUrl(const String& url) {
    _webhookUrl = url;
    saveConfig();
}

void Notifier::saveConfig() {
    _prefs.begin(PREFS_NAMESPACE, false);
    _prefs.putString("line_token", _lineToken);
    _prefs.putString("webhook_url", _webhookUrl);
    _prefs.end();
}

void Notifier::loadConfig() {
    _prefs.begin(PREFS_NAMESPACE, true);
    _lineToken = _prefs.getString("line_token", "");
    _webhookUrl = _prefs.getString("webhook_url", "");
    _prefs.end();
}
