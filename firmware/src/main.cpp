#include <Arduino.h>
#include "Config.h"
#include "ScaleManager.h"
#include "DrinkTracker.h"
#include "Notifier.h"
#include "BleWaterService.h"

#if !defined(UNIT_TEST) && !defined(PIO_UNIT_TESTING)

// 核心物件實例
ScaleManager scale;
DrinkTracker tracker(scale);
Notifier notifier;
BleWaterService bleWaterService;

// 事件回呼處理
void onTrackerEvent(EventType type, int amountMl, int remainingMl) {
    if (type == EVENT_DRINK) {
        Serial.printf("[MAIN] 收到喝水事件: +%dml, 杯中剩餘: %dml, 今日總計: %dml\n",
                      amountMl, remainingMl, tracker.getTodayTotalMl());
        notifier.notifyDrink();
        // BLE only receives accepted drink events. Refill handling remains unchanged.
        bleWaterService.recordDrink(time(nullptr), amountMl, remainingMl, tracker.getTodayTotalMl());
    } else if (type == EVENT_REFILL) {
        Serial.printf("[MAIN] 收到補水事件: +%dml, 杯中剩餘: %dml\n",
                      amountMl, remainingMl);
        notifier.notifyRefill();
        bleWaterService.recordRefill(time(nullptr), amountMl, remainingMl, tracker.getTodayTotalMl());
    }
}

void onTrackerReminder() {
    Serial.printf("[MAIN] 收到久未喝水提醒 (已超過 %d 分鐘未喝水)\n",
                  tracker.getReminderIntervalMinutes());
    notifier.notifyReminder();
}

void setup() {
    // 降頻至 80MHz，大幅降低功耗與發熱，保持晶片低溫涼爽
    setCpuFrequencyMhz(80);

    Serial.begin(115200);
    delay(1000);
    Serial.println("\n==========================================");
    Serial.println("   智慧喝水偵測器 (ESP32-C3 SuperMini)     ");
    Serial.println("==========================================");

    // 1. 初始化通知與指示燈
    notifier.begin(PIN_STATUS_LED);

    // 2. 初始化稱重感測器
    if (!scale.begin(PIN_HX711_DT, PIN_HX711_SCK)) {
        Serial.println("[MAIN] ⚠️ HX711 感測器未就緒，請檢查接線！");
    }

    // 3. 綁定追蹤器回呼並啟動
    tracker.onDrinkEvent(onTrackerEvent);
    tracker.onReminder(onTrackerReminder);
    tracker.begin();

    // 4. 啟動 BLE 藍牙服務 (本裝置唯一的對外介面)
    String deviceId = "water_" + String((uint32_t)ESP.getEfuseMac(), HEX);
    bleWaterService.begin(deviceId, &scale, &tracker);
    bleWaterService.updateSummary(tracker.getTodayTotalMl(), tracker.getDailyGoalMl(),
                                  scale.getFilteredWeight(), scale.isStable());

    Serial.println("[MAIN] 系統初始化完成，開始監控飲水狀態...");
}

void handleSerialCommands() {
    if (!Serial.available()) return;
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("TARE")) {
        scale.tare(15);
        Serial.println("[MAIN] 已執行去皮歸零");
    } else if (cmd.startsWith("CAL:")) {
        float weight = cmd.substring(4).toFloat();
        if (weight > 0) {
            if (scale.calibrateWithKnownWeight(weight)) {
                Serial.printf("[MAIN] 校準成功! 已知重量: %.1fg\n", weight);
            } else {
                Serial.println("[MAIN] 校準失敗");
            }
        }
    } else if (cmd.equalsIgnoreCase("RAW")) {
        Serial.printf("[MAIN] HX711 狀態: %s | 準備就緒: %s | 24-bit ADC 讀數: %ld | 濾波重: %.1fg\n",
                      scale.isConnected() ? "已連線" : "未連線/逾時",
                      scale.isReady() ? "就緒" : "等待中",
                      scale.getRawValue(),
                      scale.getFilteredWeight());
    } else if (cmd.equalsIgnoreCase("STATUS")) {
        const time_t nowSec = time(nullptr);
        Serial.printf("----------------------------------------\n");
        Serial.printf("[STATUS] 重量: %.1fg (原始: %.1f) | 狀態: %s | 今日總計: %dml\n",
                      scale.getFilteredWeight(), scale.getWeight(),
                      tracker.getStateString(), tracker.getTodayTotalMl());
        if (nowSec > TIME_SYNCED_EPOCH_MIN) {
            struct tm timeinfo;
            localtime_r(&nowSec, &timeinfo);
            Serial.printf("[STATUS] 裝置時間: %04d-%02d-%02d %02d:%02d:%02d\n",
                          timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            Serial.println("[STATUS] 裝置時間: 尚未校時 (等待手機透過 BLE 送出 set_time)");
        }
        Serial.printf("----------------------------------------\n");
    } else if (cmd.equalsIgnoreCase("RESET")) {
        tracker.resetDailyTotal();
        Serial.println("[MAIN] 今日飲水量已重設");
    }
}

void loop() {
    // 處理 Serial 命令 (校準與診斷)
    handleSerialCommands();

    // 週期性更新感測器讀數與濾波
    scale.update();

    // 週期性更新喝水狀態機
    tracker.update();

    // 執行 BLE 排隊進來的命令 (tare / reset_daily / set_time)，
    // 確保只在主迴圈碰 HX711 與 NVS
    bleWaterService.processPendingCommands();

    // Keep the GATT summary current without touching the drink-detection path.
    bleWaterService.updateSummary(tracker.getTodayTotalMl(), tracker.getDailyGoalMl(),
                                  scale.getFilteredWeight(), scale.isStable());

    // 處理 LED 燈效與非阻塞動畫
    notifier.update();

    // 微小讓步給背景任務
    delay(10);
}

#endif  // !UNIT_TEST && !PIO_UNIT_TESTING
