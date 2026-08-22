#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include "Config.h"

enum LedPattern {
    LED_OFF,
    LED_SOLID,
    LED_BLINK_SHORT,     // 簡短雙閃 (喝水成功)
    LED_ALERT_PULSE      // 循環閃爍 (久未喝水提醒)
};

class Notifier {
public:
    Notifier();
    void begin(uint8_t ledPin = PIN_STATUS_LED);
    void update(); // 在主迴圈中非阻塞更新 LED 燈效

    // 設定推播服務金鑰與網址
    void setLineToken(const String& token);
    String getLineToken() const { return _lineToken; }

    void setWebhookUrl(const String& url);
    String getWebhookUrl() const { return _webhookUrl; }

    // 觸發事件反饋
    void notifyDrink(int amountMl, int todayTotalMl, int goalMl);
    void notifyRefill(int amountMl, int remainingMl);
    void notifyReminder(int minutesElapsed, int todayTotalMl, int goalMl);
    void stopAlert();

    // 測試發送
    bool testPushNotification(String& outMessage);

private:
    uint8_t _ledPin;
    LedPattern _currentPattern;
    unsigned long _patternStartTime;
    unsigned long _lastLedToggle;
    bool _ledState;
    int _blinkCounter;

    String _lineToken;
    String _webhookUrl;
    Preferences _prefs;

    void setLedHardware(bool on);
    void sendLineNotify(const String& message);
    void sendWebhook(const String& jsonPayload);
    void loadConfig();
    void saveConfig();
};
