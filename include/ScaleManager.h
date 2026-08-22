#pragma once
#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
#include "Config.h"

class ScaleManager {
public:
    ScaleManager();
    bool begin(uint8_t doutPin = PIN_HX711_DT, uint8_t sckPin = PIN_HX711_SCK);
    
    // 週期性更新讀數 (應在 loop 中頻繁呼叫)
    void update();

    // 取得即時重量 (克)
    float getWeight();

    // 取得平滑後重量 (克)
    float getFilteredWeight();

    // 取得目前讀數是否處於穩定狀態
    bool isStable();

    // 去皮 (將當前重量設為 0)
    void tare(uint8_t times = 10);

    // 校準：在秤上放置已知重量物品後呼叫，計算並儲存校準係數
    bool calibrateWithKnownWeight(float knownWeightGrams);

    // 儲存與讀取校準參數
    void saveCalibration(float factor, long offset);
    void loadCalibration();

    // 取得當前校準參數
    float getCalibrationFactor() const { return _calibrationFactor; }
    long getZeroOffset() const { return _zeroOffset; }
    void setCalibrationFactor(float factor);

    // 取得感測器是否正常連線與 24-bit ADC 原始讀數
    bool isConnected();
    bool isReady() { return _scale.is_ready(); }
    long getRawValue();

private:
    HX711 _scale;
    Preferences _prefs;
    uint8_t _doutPin;
    uint8_t _sckPin;

    float _calibrationFactor;
    long _zeroOffset;

    float _currentWeight;
    float _filteredWeight;
    bool _isStable;

    // 滑動視窗濾波與穩定度判斷
    static const int WINDOW_SIZE = 8;
    float _readingHistory[WINDOW_SIZE];
    int _historyIndex;
    int _historyCount;

    unsigned long _lastReadTime;
};
