#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Preferences.h>
#include <time.h>
#include "Config.h"

struct CloudEvent {
    time_t timestamp;
    char eventType[10]; // "drink" 或 "refill"
    int amountMl;
    int remainingMl;
    int todayTotalMl;
    int dailyGoalMl;
};

class CloudSync {
public:
    CloudSync();
    void begin();
    void update(); // 定期檢查並重試發送佇列中的事件

    // 發送喝水與加水事件至雲端
    void queueEvent(const char* eventType, int amountMl, int remainingMl, int todayTotalMl, int dailyGoalMl);

    // 設定雲端 API 終端 (支援 Firebase REST, Supabase REST, 自訂 Webhook)
    void setCloudEndpoint(const String& endpoint);
    String getCloudEndpoint() const { return _cloudEndpoint; }

    void setApiKey(const String& key);
    String getApiKey() const { return _apiKey; }

    // 取得裝置唯一 ID
    String getDeviceId() const { return _deviceId; }

    // 取得當前 NTP 時間字串 (例: "2026-08-22 19:35:00")
    static String getFormattedTime(time_t rawTime = 0);

private:
    String _cloudEndpoint;
    String _apiKey;
    String _deviceId;
    Preferences _prefs;

    // 離線佇列
    static const int QUEUE_SIZE = 30;
    CloudEvent _eventQueue[QUEUE_SIZE];
    int _queueHead;
    int _queueTail;
    int _queueCount;

    unsigned long _lastSyncAttempt;

    void sendEventDirect(const CloudEvent& ev);
    void loadConfig();
    void saveConfig();
};
