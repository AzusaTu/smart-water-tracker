#include "ScaleManager.h"

ScaleManager::ScaleManager()
    : _doutPin(PIN_HX711_DT),
      _sckPin(PIN_HX711_SCK),
      _calibrationFactor(DEFAULT_CALIBRATION_FACTOR),
      _zeroOffset(0),
      _currentWeight(0.0f),
      _filteredWeight(0.0f),
      _isStable(false),
      _historyIndex(0),
      _historyCount(0),
      _lastReadTime(0) {
    for (int i = 0; i < WINDOW_SIZE; i++) {
        _readingHistory[i] = 0.0f;
    }
}

bool ScaleManager::begin(uint8_t doutPin, uint8_t sckPin) {
    _doutPin = doutPin;
    _sckPin = sckPin;

    _scale.begin(_doutPin, _sckPin);

    // 載入已儲存的校準係數與零點
    loadCalibration();

    _scale.set_scale(_calibrationFactor);
    if (_zeroOffset != 0) {
        _scale.set_offset(_zeroOffset);
    } else {
        tare(15);
    }

    Serial.printf("[ScaleManager] HX711 初始化完成. Factor: %.2f, Offset: %ld\n", _calibrationFactor, _zeroOffset);
    return isConnected();
}

bool ScaleManager::isConnected() {
    // 檢查 HX711 是否可通訊 (等待超時 200ms)
    return _scale.wait_ready_timeout(200);
}

long ScaleManager::getRawValue() {
    if (_scale.is_ready()) {
        return _scale.read();
    }
    return 0;
}

void ScaleManager::update() {
    // HX711 預設取樣頻率約 10Hz (每 100ms 一次讀數) 或 80Hz
    if (millis() - _lastReadTime < 80) {
        return;
    }

    if (_scale.is_ready()) {
        _lastReadTime = millis();
        float weight = _scale.get_units(1);

        // 避免微小的負零漂
        if (weight < 0.0f && weight > -1.5f) {
            weight = 0.0f;
        }

        _currentWeight = weight;

        // 加入滑動視窗
        _readingHistory[_historyIndex] = weight;
        _historyIndex = (_historyIndex + 1) % WINDOW_SIZE;
        if (_historyCount < WINDOW_SIZE) {
            _historyCount++;
        }

        // 計算滑動平均與極值
        float sum = 0.0f;
        float minVal = _readingHistory[0];
        float maxVal = _readingHistory[0];

        for (int i = 0; i < _historyCount; i++) {
            float val = _readingHistory[i];
            sum += val;
            if (val < minVal) minVal = val;
            if (val > maxVal) maxVal = val;
        }

        _filteredWeight = sum / (float)_historyCount;

        // 穩定度判定：當視窗填滿且極值差距小於公差時視為穩定
        if (_historyCount >= WINDOW_SIZE && (maxVal - minVal) <= STABILITY_TOLERANCE_G) {
            _isStable = true;
        } else {
            _isStable = false;
        }
    }
}

float ScaleManager::getWeight() {
    return _currentWeight;
}

float ScaleManager::getFilteredWeight() {
    return _filteredWeight;
}

bool ScaleManager::isStable() {
    return _isStable;
}

void ScaleManager::tare(uint8_t times) {
    if (_scale.wait_ready_timeout(500)) {
        _scale.tare(times);
        _zeroOffset = _scale.get_offset();
        saveCalibration(_calibrationFactor, _zeroOffset);
        
        // 重設濾波視窗。_historyIndex 必須一併歸零：平均迴圈是從 index 0 掃到
        // _historyCount，只清 count 會讓新讀數寫進舊 index，平均到的是去皮前的值。
        _historyIndex = 0;
        _historyCount = 0;
        _currentWeight = 0.0f;
        _filteredWeight = 0.0f;
        // 視窗尚未重新填滿前不能宣告穩定，否則 DrinkTracker 會拿假穩定值做狀態判斷
        _isStable = false;
        Serial.printf("[ScaleManager] 已去皮. 零點 Offset: %ld\n", _zeroOffset);
    }
}

bool ScaleManager::calibrateWithKnownWeight(float knownWeightGrams) {
    if (knownWeightGrams <= 0.0f) {
        Serial.println("[ScaleManager] 校準重量必須大於 0 克");
        return false;
    }

    if (!_scale.wait_ready_timeout(1000)) {
        Serial.println("[ScaleManager] HX711 未就緒，校準失敗");
        return false;
    }

    // 取得當前物體的原始數值 (Raw reading minus offset)
    long rawReading = _scale.get_value(15);
    float newFactor = (float)rawReading / knownWeightGrams;

    if (abs(newFactor) < 1.0f) {
        Serial.println("[ScaleManager] 校準讀數異常，請檢查接線與重物");
        return false;
    }

    _calibrationFactor = newFactor;
    _scale.set_scale(_calibrationFactor);
    saveCalibration(_calibrationFactor, _zeroOffset);

    // 重設濾波視窗 (理由同 tare())
    _historyIndex = 0;
    _historyCount = 0;
    _isStable = false;
    Serial.printf("[ScaleManager] 校準成功! 新係數: %.2f (已知重量: %.1fg)\n", _calibrationFactor, knownWeightGrams);
    return true;
}

void ScaleManager::setCalibrationFactor(float factor) {
    _calibrationFactor = factor;
    _scale.set_scale(_calibrationFactor);
    saveCalibration(_calibrationFactor, _zeroOffset);
}

void ScaleManager::saveCalibration(float factor, long offset) {
    _prefs.begin(PREFS_NAMESPACE, false);
    _prefs.putFloat("cal_factor", factor);
    _prefs.putLong("zero_offset", offset);
    _prefs.end();
    Serial.printf("[ScaleManager] 校準參數已存入 NVS (Factor: %.2f, Offset: %ld)\n", factor, offset);
}

void ScaleManager::loadCalibration() {
    _prefs.begin(PREFS_NAMESPACE, true);
    _calibrationFactor = _prefs.getFloat("cal_factor", DEFAULT_CALIBRATION_FACTOR);
    _zeroOffset = _prefs.getLong("zero_offset", 0);
    _prefs.end();
    Serial.printf("[ScaleManager] 從 NVS 載入校準參數 (Factor: %.2f, Offset: %ld)\n", _calibrationFactor, _zeroOffset);
}
