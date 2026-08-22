#include "BleWaterService.h"

#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

class BleWaterService::Impl {
public:
    BLEServer* server = nullptr;
    BLECharacteristic* liveEvent = nullptr;
    BLECharacteristic* summary = nullptr;
    BLECharacteristic* historySync = nullptr;
};

class WaterSummaryCallbacks final : public BLECharacteristicCallbacks {
public:
    explicit WaterSummaryCallbacks(BleWaterService& service) : _service(service) {}

    void onRead(BLECharacteristic* characteristic) override {
        characteristic->setValue(_service.summaryJson().c_str());
    }

private:
    BleWaterService& _service;
};

class WaterHistorySyncCallbacks final : public BLECharacteristicCallbacks {
public:
    explicit WaterHistorySyncCallbacks(BleWaterService& service) : _service(service) {}

    void onWrite(BLECharacteristic* characteristic) override {
        const String value = characteristic->getValue();
        JsonDocument request;
        const DeserializationError error = deserializeJson(request, value);
        if (error) {
            Serial.println("[BLE] historySync JSON 格式錯誤，將從最早可用事件開始重送");
            _service.replayAfter("");
            return;
        }

        const char* afterEventId = request["afterEventId"] | "";
        _service.replayAfter(String(afterEventId));
    }

private:
    BleWaterService& _service;
};

BleWaterService::BleWaterService(const String& deviceId) : _deviceId(deviceId) {}

void BleWaterService::begin(const String& deviceId) {
    _deviceId = deviceId;
    const String suffix = _deviceId.length() >= 4
        ? _deviceId.substring(_deviceId.length() - 4)
        : _deviceId;
    const String advertisedName = String(BleProtocol::DEVICE_NAME_PREFIX) + suffix;

    BLEDevice::init(advertisedName.c_str());
    _impl = new Impl();
    _impl->server = BLEDevice::createServer();
    BLEService* service = _impl->server->createService(BleProtocol::SERVICE_UUID);

    _impl->liveEvent = service->createCharacteristic(
        BleProtocol::LIVE_EVENT_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    _impl->liveEvent->addDescriptor(new BLE2902());

    _impl->summary = service->createCharacteristic(
        BleProtocol::SUMMARY_UUID, BLECharacteristic::PROPERTY_READ);
    _impl->summary->setCallbacks(new WaterSummaryCallbacks(*this));
    _impl->summary->setValue(summaryJson().c_str());

    _impl->historySync = service->createCharacteristic(
        BleProtocol::HISTORY_SYNC_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);
    _impl->historySync->addDescriptor(new BLE2902());
    _impl->historySync->setCallbacks(new WaterHistorySyncCallbacks(*this));

    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BleProtocol::SERVICE_UUID);
    advertising->start();
    Serial.printf("[BLE] 服務已啟動: %s (%s)\n", advertisedName.c_str(), _deviceId.c_str());
}

void BleWaterService::updateSummary(int todayTotalMl, int dailyGoalMl) {
    _todayTotalMl = todayTotalMl;
    _dailyGoalMl = dailyGoalMl;
    if (_impl != nullptr && _impl->summary != nullptr) {
        _impl->summary->setValue(summaryJson().c_str());
    }
}

void BleWaterService::recordDrink(time_t occurredAt, int amountMl, int remainingMl, int todayTotalMl) {
    if (amountMl <= 0 || remainingMl < 0 || todayTotalMl < 0) {
        Serial.println("[BLE] 忽略無效喝水事件");
        return;
    }

    if (occurredAt <= 0) {
        occurredAt = millis() / 1000;
    }
    BleDrinkEvent event;
    event.occurredAt = occurredAt;
    event.amountMl = amountMl;
    event.remainingMl = remainingMl;
    event.todayTotalMl = todayTotalMl;
    event.id = _deviceId + "-" + String(static_cast<unsigned long>(occurredAt)) + "-" + String(_nextSequence++);

    _events[_eventHead] = event;
    _eventHead = (_eventHead + 1) % BleProtocol::EVENT_BUFFER_SIZE;
    if (_eventCount < BleProtocol::EVENT_BUFFER_SIZE) {
        ++_eventCount;
    }
    _todayTotalMl = todayTotalMl;
    updateSummary(_todayTotalMl, _dailyGoalMl);
    publishLiveEvent(event);
}

std::vector<BleDrinkEvent> BleWaterService::eventsAfter(const String& afterEventId) const {
    std::vector<BleDrinkEvent> ordered;
    ordered.reserve(_eventCount);
    const size_t oldest = (_eventHead + BleProtocol::EVENT_BUFFER_SIZE - _eventCount) % BleProtocol::EVENT_BUFFER_SIZE;
    bool foundCursor = afterEventId.length() == 0;

    for (size_t offset = 0; offset < _eventCount; ++offset) {
        const BleDrinkEvent& event = _events[(oldest + offset) % BleProtocol::EVENT_BUFFER_SIZE];
        if (foundCursor) {
            ordered.push_back(event);
        } else if (event.id == afterEventId) {
            foundCursor = true;
        }
    }

    // A cursor outside the RAM buffer means the client fell behind. Replay all
    // retained events; the client still deduplicates by event ID.
    if (!foundCursor && afterEventId.length() > 0) {
        return eventsAfter("");
    }
    return ordered;
}

String BleWaterService::latestEventId() const {
    if (_eventCount == 0) {
        return "";
    }
    const size_t newest = (_eventHead + BleProtocol::EVENT_BUFFER_SIZE - 1) % BleProtocol::EVENT_BUFFER_SIZE;
    return _events[newest].id;
}

String BleWaterService::eventJson(const BleDrinkEvent& event) const {
    JsonDocument document;
    document["eventId"] = event.id;
    document["occurredAt"] = static_cast<long>(event.occurredAt);
    document["type"] = "drink";
    document["amountMl"] = event.amountMl;
    document["remainingMl"] = event.remainingMl;
    document["todayTotalMl"] = event.todayTotalMl;

    String json;
    serializeJson(document, json);
    return json;
}

String BleWaterService::summaryJson() const {
    JsonDocument document;
    document["deviceId"] = _deviceId;
    document["dailyGoalMl"] = _dailyGoalMl;
    document["todayTotalMl"] = _todayTotalMl;
    const String latestId = latestEventId();
    if (latestId.length() == 0) {
        document["latestEventId"] = nullptr;
    } else {
        document["latestEventId"] = latestId;
    }

    String json;
    serializeJson(document, json);
    return json;
}

void BleWaterService::publishLiveEvent(const BleDrinkEvent& event) {
    if (_impl == nullptr || _impl->liveEvent == nullptr) {
        return;
    }
    const String json = eventJson(event);
    _impl->liveEvent->setValue(json.c_str());
    _impl->liveEvent->notify();
}

void BleWaterService::replayAfter(const String& afterEventId) {
    if (_impl == nullptr || _impl->historySync == nullptr) {
        return;
    }
    for (const BleDrinkEvent& event : eventsAfter(afterEventId)) {
        const String json = eventJson(event);
        _impl->historySync->setValue(json.c_str());
        _impl->historySync->notify();
        delay(15);
    }
    notifySyncComplete();
}

void BleWaterService::notifySyncComplete() {
    JsonDocument document;
    document["type"] = "sync_complete";
    document["latestEventId"] = latestEventId();
    String json;
    serializeJson(document, json);
    _impl->historySync->setValue(json.c_str());
    _impl->historySync->notify();
}
