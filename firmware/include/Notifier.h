#pragma once
#include <Arduino.h>
#include "Config.h"

enum LedPattern {
    LED_OFF,
    LED_BLINK_SHORT,     // 簡短雙閃 (喝水成功)
    LED_ALERT_PULSE      // 循環閃爍 (久未喝水提醒)
};

// 板載 LED 的即時回饋。
//
// 推播 (LINE Notify / Webhook / 雲端同步) 已全數移除：裝置只負責偵測與立即回饋，
// 統計、提醒與通知一律交給手機端，避免裝置成為第二個真相來源。
class Notifier {
public:
    Notifier();
    void begin(uint8_t ledPin = PIN_STATUS_LED);
    void update(); // 在主迴圈中非阻塞更新 LED 燈效

    // 觸發事件反饋
    void notifyDrink();
    void notifyRefill();
    void notifyReminder();

private:
    uint8_t _ledPin;
    LedPattern _currentPattern;
    unsigned long _lastLedToggle;
    bool _ledState;
    int _blinkCounter;

    void setLedHardware(bool on);
};
