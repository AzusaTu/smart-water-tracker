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

    std::vector<BleDrinkEvent> eventsAfter(const String& afterEventId) const;
    String latestEventId() const;
    String deviceId() const { return _deviceId; }

private:
    friend class WaterSummaryCallbacks;
    friend class WaterHistorySyncCallbacks;
    friend class WaterCommandCallbacks;

    String _deviceId;
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

    String eventJson(const BleDrinkEvent& event) const;
    String summaryJson() const;
    void publishLiveEvent(const BleDrinkEvent& event);
    void replayAfter(const String& afterEventId);
    void notifySyncComplete();
};
