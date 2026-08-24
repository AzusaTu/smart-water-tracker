#pragma once

#include <Arduino.h>
#include <vector>

#include "BleProtocol.h"

struct BleDrinkEvent {
    String id;
    time_t occurredAt;
    int amountMl;
    int remainingMl;
    int todayTotalMl;
};

class ScaleManager;
class DrinkTracker;

// Owns the BLE transport and an in-RAM replay buffer. It deliberately does not
// persist events: the phone is the source of persistence for the BLE MVP.
class BleWaterService {
public:
    explicit BleWaterService(const String& deviceId = "");

    void begin(const String& deviceId, ScaleManager* scale = nullptr, DrinkTracker* tracker = nullptr);
    void updateSummary(int todayTotalMl, int dailyGoalMl, float currentWeight = 0.0f, bool isStable = true);
    void recordDrink(time_t occurredAt, int amountMl, int remainingMl, int todayTotalMl);
    void tare();

    // BLE callback 跑在 BLE host task。tare 會 bit-bang HX711、reset_daily 會寫 NVS，
    // 兩者都與主迴圈競爭同一份硬體/儲存，因此 callback 只排隊、由 loop() 呼叫本函式執行。
    void processPendingCommands();

    std::vector<BleDrinkEvent> eventsAfter(const String& afterEventId) const;
    String latestEventId() const;
    String deviceId() const { return _deviceId; }

private:
    friend class WaterSummaryCallbacks;
    friend class WaterHistorySyncCallbacks;
    friend class WaterCommandCallbacks;

    enum PendingCommand : uint8_t {
        PENDING_NONE = 0,
        PENDING_TARE,
        PENDING_RESET_DAILY,
        PENDING_SET_TIME,
    };

    String _deviceId;
    // payload 一律先寫，最後才寫 _pendingCommand —— 後者是「發佈」動作
    volatile time_t _pendingEpoch = 0;
    volatile int _pendingTzOffsetMinutes = 0;
    volatile PendingCommand _pendingCommand = PENDING_NONE;
    ScaleManager* _scale = nullptr;
    DrinkTracker* _tracker = nullptr;
    BleDrinkEvent _events[BleProtocol::EVENT_BUFFER_SIZE];
    size_t _eventCount = 0;
    size_t _eventHead = 0;
    uint32_t _nextSequence = 0;
    int _todayTotalMl = 0;
    int _dailyGoalMl = 0;
    float _currentWeight = 0.0f;
    bool _isScaleStable = true;

    class Impl;
    Impl* _impl = nullptr;

    void applyDeviceTime(time_t epoch, int tzOffsetMinutes);
    static bool isClockSynced();

    String eventJson(const BleDrinkEvent& event) const;
    String summaryJson() const;
    void publishLiveEvent(const BleDrinkEvent& event);
    void replayAfter(const String& afterEventId);
    void notifySyncComplete();
};
