#include "Notifier.h"

Notifier::Notifier()
    : _ledPin(PIN_STATUS_LED),
      _currentPattern(LED_OFF),
      _lastLedToggle(0),
      _ledState(false),
      _blinkCounter(0) {
}

void Notifier::begin(uint8_t ledPin) {
    _ledPin = ledPin;
    pinMode(_ledPin, OUTPUT);
    setLedHardware(false); // 預設關閉

    Serial.println("[Notifier] 指示燈模組啟動完成");
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

void Notifier::notifyDrink() {
    _currentPattern = LED_BLINK_SHORT;
    _lastLedToggle = millis();
    _blinkCounter = 0;
    setLedHardware(true);
}

void Notifier::notifyRefill() {
    _currentPattern = LED_BLINK_SHORT;
    _lastLedToggle = millis();
    _blinkCounter = 2; // 單閃一次
    setLedHardware(true);
}

void Notifier::notifyReminder() {
    _currentPattern = LED_ALERT_PULSE;
    _lastLedToggle = millis();
}
