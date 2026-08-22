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

// Owns the BLE transport and an in-RAM replay buffer. It deliberately does not
// persist events: the phone is the source of persistence for the BLE MVP.
class BleWaterService {
public:
    explicit BleWaterService(const String& deviceId = "");

    void begin(const String& deviceId);
    void updateSummary(int todayTotalMl, int dailyGoalMl);
    void recordDrink(time_t occurredAt, int amountMl, int remainingMl, int todayTotalMl);

    std::vector<BleDrinkEvent> eventsAfter(const String& afterEventId) const;
    String latestEventId() const;
    String deviceId() const { return _deviceId; }

private:
    friend class WaterSummaryCallbacks;
    friend class WaterHistorySyncCallbacks;

    String _deviceId;
    BleDrinkEvent _events[BleProtocol::EVENT_BUFFER_SIZE];
    size_t _eventCount = 0;
    size_t _eventHead = 0;
    uint32_t _nextSequence = 0;
    int _todayTotalMl = 0;
    int _dailyGoalMl = 0;

    class Impl;
    Impl* _impl = nullptr;

    String eventJson(const BleDrinkEvent& event) const;
    String summaryJson() const;
    void publishLiveEvent(const BleDrinkEvent& event);
    void replayAfter(const String& afterEventId);
    void notifySyncComplete();
};
