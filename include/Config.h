#pragma once
#include <Arduino.h>

// ==========================================
// 硬體腳位設定 (ESP32-C3 SuperMini)
// ==========================================
#define PIN_HX711_DT       2   // HX711 數據引腳 (Data Pin)
#define PIN_HX711_SCK      3   // HX711 時脈引腳 (Clock Pin)
#define PIN_STATUS_LED     8   // ESP32-C3 SuperMini 板載藍色 LED (低電位觸發 Active Low)

// ==========================================
// 預設演算法與運作參數
// ==========================================
#define DEFAULT_CALIBRATION_FACTOR   420.0f  // 預設校準係數 (可透過網頁校準更新)
#define DEFAULT_DAILY_GOAL_ML        2000    // 每日飲水目標 (毫升)
#define DEFAULT_REMINDER_MINUTES     45      // 久未喝水提醒間隔 (分鐘)
#define MIN_DRINK_THRESHOLD_G        15.0f   // 判定為喝水的最小重量減少量 (克/毫升)
#define REFILL_THRESHOLD_G           35.0f   // 判定為加水的重量增加量 (克/毫升)
#define EMPTY_CUP_THRESHOLD_G        25.0f   // 空秤閾值 (低於此重量視為杯子已拿開)
#define STABILITY_TOLERANCE_G        1.5f    // 讀數穩定判定公差 (克)
#define STABLE_SAMPLES_REQUIRED      4       // 連續穩定取樣次數

// ==========================================
// 儲存與網路設定
// ==========================================
#define PREFS_NAMESPACE             "water_app"
#define AP_SSID_NAME                "WaterTracker"
#define AP_DEFAULT_PASSWORD         "12345678"
#define MDNS_HOSTNAME               "water"   // 可透過 http://water.local 存取

// 系統設定結構體
struct AppSettings {
    float calibrationFactor;
    long  zeroOffset;
    int   dailyGoalMl;
    int   reminderMinutes;
    float minDrinkThresholdG;
    float emptyCupThresholdG;
    char  wifiSSID[33];
    char  wifiPassword[65];
    char  lineToken[65];
    char  webhookUrl[128];
};
