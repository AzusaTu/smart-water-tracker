#include "BleWaterService.h"
#include <sys/time.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "ScaleManager.h"
#include "DrinkTracker.h"

class BleWaterService::Impl {
public:
    BLEServer* server = nullptr;
    BLECharacteristic* liveEvent = nullptr;
    BLECharacteristic* summary = nullptr;
    BLECharacteristic* historySync = nullptr;
    BLECharacteristic* command = nullptr;
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

class WaterCommandCallbacks final : public BLECharacteristicCallbacks {
public:
    explicit WaterCommandCallbacks(BleWaterService& service) : _service(service) {}

    void onWrite(BLECharacteristic* characteristic) override {
        const String value = characteristic->getValue();
        JsonDocument request;
        const DeserializationError error = deserializeJson(request, value);
        if (error) {
            Serial.println("[BLE] command JSON 格式錯誤");
            return;
        }

        // 這裡是 BLE host task，不可直接碰 HX711 或 NVS，只排隊給主迴圈。
        const char* action = request["action"] | "";
        if (strcmp(action, "tare") == 0) {
            Serial.println("[BLE] 收到去皮 (Tare) 命令，排入主迴圈執行...");
            _service._pendingCommand = BleWaterService::PENDING_TARE;
        } else if (strcmp(action, "reset_daily") == 0) {
            Serial.println("[BLE] 收到重設今日喝水量命令，排入主迴圈執行...");
            _service._pendingCommand = BleWaterService::PENDING_RESET_DAILY;
        } else if (strcmp(action, "set_time") == 0) {
            // 本裝置沒有 WiFi/NTP，手機是唯一的時間來源。
            const long long epoch = request["epoch"] | 0LL;
            if (epoch < TIME_SYNCED_EPOCH_MIN) {
                Serial.printf("[BLE] set_time 的 epoch 不合理 (%lld)，忽略\n", epoch);
                return;
            }
            _service._pendingEpoch = static_cast<time_t>(epoch);
            _service._pendingTzOffsetMinutes = request["tzOffsetMinutes"] | 0;
            _service._pendingCommand = BleWaterService::PENDING_SET_TIME;
        }
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

class WaterServerCallbacks final : public BLEServerCallbacks {
public:
    void onConnect(BLEServer* pServer) override {
        Serial.println("[BLE] 手機已連線");
    }

    void onDisconnect(BLEServer* pServer) override {
        Serial.println("[BLE] 手機已中斷連線，重新啟動廣播 (Advertising)...");
        BLEDevice::startAdvertising();
    }
};

BleWaterService::BleWaterService(const String& deviceId) : _deviceId(deviceId) {}

void BleWaterService::begin(const String& deviceId, ScaleManager* scale, DrinkTracker* tracker) {
    _deviceId = deviceId;
    _scale = scale;
    _tracker = tracker;
    const String suffix = _deviceId.length() >= 4
        ? _deviceId.substring(_deviceId.length() - 4)
        : _deviceId;
    const String advertisedName = String(BleProtocol::DEVICE_NAME_PREFIX) + suffix;

    BLEDevice::init(advertisedName.c_str());
    BLEDevice::setMTU(517);
    _impl = new Impl();
    _impl->server = BLEDevice::createServer();
    _impl->server->setCallbacks(new WaterServerCallbacks());
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

    _impl->command = service->createCharacteristic(
        BleProtocol::COMMAND_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR | BLECharacteristic::PROPERTY_READ);
    _impl->command->setCallbacks(new WaterCommandCallbacks(*this));

    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(BleProtocol::SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    Serial.printf("[BLE] 服務已啟動: %s (%s)\n", advertisedName.c_str(), _deviceId.c_str());
}

void BleWaterService::tare() {
    if (_scale != nullptr) {
        _scale->tare(3);
        _currentWeight = _scale->getFilteredWeight();
        _isScaleStable = _scale->isStable();
    } else {
        _currentWeight = 0.0f;
    }
    if (_impl != nullptr && _impl->summary != nullptr) {
        _impl->summary->setValue(summaryJson().c_str());
    }
    Serial.printf("[BLE] 執行去皮完成，當前重量: %.1fg\n", _currentWeight);
}

bool BleWaterService::isClockSynced() {
    return time(nullptr) > TIME_SYNCED_EPOCH_MIN;
}

void BleWaterService::applyDeviceTime(time_t epoch, int tzOffsetMinutes) {
    struct timeval tv;
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    // POSIX 的 TZ 字串符號與日常寫法相反：UTC+8 要寫成 "UTC-8"
    char tz[16];
    snprintf(tz, sizeof(tz), "UTC%+d:%02d", -(tzOffsetMinutes / 60), abs(tzOffsetMinutes % 60));
    setenv("TZ", tz, 1);
    tzset();

    struct tm timeinfo;
    const time_t now = time(nullptr);
    localtime_r(&now, &timeinfo);
    Serial.printf("[BLE] 已由手機校時: %04d-%02d-%02d %02d:%02d:%02d (TZ=%s)\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, tz);
}

void BleWaterService::processPendingCommands() {
    const PendingCommand pending = _pendingCommand;
    if (pending == PENDING_NONE) {
        return;
    }
    _pendingCommand = PENDING_NONE;

    // 回應內容與格式維持不變，只是改在實際執行完成後才寫回 command characteristic。
    JsonDocument response;
    response["success"] = true;
    if (pending == PENDING_TARE) {
        tare();
        response["action"] = "tare";
        response["currentWeight"] = _currentWeight;
    } else if (pending == PENDING_SET_TIME) {
        applyDeviceTime(_pendingEpoch, _pendingTzOffsetMinutes);
        response["action"] = "set_time";
        response["epoch"] = static_cast<long long>(time(nullptr));
    } else {
        Serial.println("[BLE] 執行重設今日喝水量，已重設為 0 ml");
        if (_tracker != nullptr) {
            _tracker->resetDailyTotal();
        }
        _todayTotalMl = 0;
        updateSummary(0, _dailyGoalMl, _currentWeight, _isScaleStable);
        response["action"] = "reset_daily";
    }

    if (_impl != nullptr && _impl->command != nullptr) {
        String json;
        serializeJson(response, json);
        _impl->command->setValue(json.c_str());
    }
}

void BleWaterService::updateSummary(int todayTotalMl, int dailyGoalMl, float currentWeight, bool isStable) {
    _todayTotalMl = todayTotalMl;
    _dailyGoalMl = dailyGoalMl;
    _currentWeight = currentWeight;
    _isScaleStable = isStable;
    if (_impl != nullptr && _impl->summary != nullptr) {
        _impl->summary->setValue(summaryJson().c_str());
    }
}

void BleWaterService::recordDrink(time_t occurredAt, int amountMl, int remainingMl, int todayTotalMl) {
    if (amountMl <= 0 || remainingMl < 0 || todayTotalMl < 0) {
        Serial.println("[BLE] 忽略無效喝水事件");
        return;
    }

    // 未校時的 time() 只是開機秒數，送出去會被當成 1970 年。
    // 明確標成 0 (未知)，並由 eventJson 的 timeSynced 告訴手機。
    if (occurredAt < TIME_SYNCED_EPOCH_MIN) {
        occurredAt = 0;
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
    document["timeSynced"] = event.occurredAt > 0;

    String json;
    serializeJson(document, json);
    return json;
}

String BleWaterService::summaryJson() const {
    JsonDocument document;
    document["deviceId"] = _deviceId;
    document["dailyGoalMl"] = _dailyGoalMl;
    document["todayTotalMl"] = _todayTotalMl;
    document["currentWeight"] = _currentWeight;
    document["isStable"] = _isScaleStable;
    document["timeSynced"] = isClockSynced();
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
    // The central enables notifications asynchronously. Wait for the CCCD
    // subscription to settle before returning the first event/sync_complete.
    delay(200);
    for (const BleDrinkEvent& event : eventsAfter(afterEventId)) {
        const String json = eventJson(event);
        _impl->historySync->setValue(json.c_str());
        _impl->historySync->notify();
        delay(20);
    }
    delay(20);
    notifySyncComplete();
}

void BleWaterService::notifySyncComplete() {
    JsonDocument document;
    document["type"] = "sync_complete";
    String latestId = latestEventId();
    if (latestId.length() == 0) {
        document["latestEventId"] = nullptr;
    } else {
        document["latestEventId"] = latestId;
    }
    String json;
    serializeJson(document, json);
    _impl->historySync->setValue(json.c_str());
    _impl->historySync->notify();
}
