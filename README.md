# 🥤 智慧喝水偵測器 (ESP32-C3 SuperMini + HX711 5KG)

以 **ESP32-C3 SuperMini** 搭配 **5KG 圓形秤架 (HX711 稱重模組)** 打造的桌面智慧喝水追蹤器。自動感應水杯拿起/放下、計算喝水量、辨識補水，並透過 **BLE** 把事件送給手機 App。

## 設計原則：裝置只做偵測，不做統計

裝置負責「**發生了什麼事**」，手機負責「**這些事代表什麼**」。

| 裝置負責 | 手機負責 |
| :--- | :--- |
| HX711 取樣、濾波、穩定判斷 | 今日累計、目標、達成率 |
| 校準係數與零點 | 提醒排程與推播通知 |
| 事件偵測（拿起/放回 → 喝了幾 ml） | 歷史紀錄與統計 |
| 未送出事件的重播緩衝 | 日期與時區的權威來源 |
| 板載 LED 即時回饋 | |

裝置**沒有 WiFi、沒有網頁介面、沒有雲端同步**。這是刻意的：避免裝置成為第二個真相來源
（過去兩邊都保有「今日總量」，導致手機端的本地操作每次同步都被覆蓋），同時大幅降低發熱與 Flash 用量。

手機端實作位於 `clock_in_app` 的 `lib/features/water/`。

---

## 🛠️ 硬體接線指南

### 1. ESP32-C3 SuperMini 與 HX711

| HX711 稱重模組 | ESP32-C3 SuperMini | 說明 |
| :--- | :--- | :--- |
| **VCC** | **3V3** (或 5V) | 供電 (建議接 3.3V) |
| **GND** | **GND** | 接地 |
| **DT (Data)** | **GPIO 2** | 數據輸出訊號線 |
| **SCK (Clock)**| **GPIO 3** | 時脈控制訊號線 |

> 腳位定義在 [include/Config.h](include/Config.h)，若要改接請以該檔為準。

> **提示**：ESP32-C3 SuperMini 板載藍色 LED 位於 **GPIO 8**（程式已配置為喝水雙閃、未喝水警示脈衝閃爍）。

### 2. 5KG 圓形秤架感測器 (4 線) 接 HX711

| 導線顏色 | HX711 稱重端子 | 說明 |
| :--- | :--- | :--- |
| **紅色 (Red)** | **E+** | 激勵正極 (Excitation +) |
| **黑色 (Black)** | **E-** | 激勵負極 (Excitation -) |
| **白色 (White)** | **A-** | 訊號負極 (Signal -) |
| **綠色 (Green)** | **A+** | 訊號正極 (Signal +) |

*(若測出重量數值方向相反，將 A+ 與 A- 對調即可)*

---

## 🚀 專案結構

- [platformio.ini](platformio.ini) : PlatformIO 環境與依賴庫設定
- [include/Config.h](include/Config.h) : 腳位與系統常數設定
- [include/BleProtocol.h](include/BleProtocol.h) : BLE UUID 與協定常數（與手機端共用）
- [src/ScaleManager.cpp](src/ScaleManager.cpp) : HX711 讀取、滑動濾波與校準邏輯
- [src/DrinkTracker.cpp](src/DrinkTracker.cpp) : 水杯狀態機、喝水/加水判斷、每日總計
- [src/BleWaterService.cpp](src/BleWaterService.cpp) : BLE GATT 服務、事件重播緩衝、命令佇列
- [src/Notifier.cpp](src/Notifier.cpp) : 板載 LED 燈效
- [src/main.cpp](src/main.cpp) : 主程式入口

---

## 💻 燒錄與使用說明

將 ESP32-C3 SuperMini 透過 Type-C 連接至電腦，在終端機執行：

```bash
pio run -t upload
```

開啟序列埠監視器（115200 波特率）：

```bash
pio device monitor
```

### Serial 診斷指令

| 指令 | 說明 |
| :--- | :--- |
| `TARE` | 去皮歸零 |
| `CAL:<克數>` | 以已知重量校準，例如 `CAL:500` |
| `RAW` | 顯示 HX711 連線狀態與原始 ADC 讀數 |
| `STATUS` | 顯示重量、狀態機、今日累計與裝置時間 |
| `RESET` | 重設今日累計 |

---

## ⚖️ 感測器兩步校準

1. **清空秤面** → 輸入 `TARE`（或從 App 按去皮）。
2. **放上已知重量物品**（例如量過重量的水杯）→ 輸入 `CAL:<克數>`。

校準係數與零點會存進 NVS，重開機不需要重做。

---

## 📲 BLE 協定

UUID 定義在 [include/BleProtocol.h](include/BleProtocol.h)，手機端使用同一組常數。

### Command Characteristic

寫入 JSON 即可下指令：

| action | 參數 | 說明 |
| :--- | :--- | :--- |
| `tare` | 無 | 去皮歸零 |
| `reset_daily` | 無 | 今日累計歸零 |
| `set_time` | `epoch` (UTC 秒), `tzOffsetMinutes` (東為正) | 校時 |

```json
{"action": "set_time", "epoch": 1787918400, "tzOffsetMinutes": 480}
```

指令一律排入佇列由主迴圈執行，不在 BLE 回呼中直接操作 HX711 或 NVS
（BLE 回呼跑在 BLE host task，與主迴圈競爭會讓 bit-bang 讀數損毀）。

### 校時（重要）

**手機端必須在每次連線後送一次 `set_time`。** 裝置沒有 WiFi，也就沒有 NTP，手機是它唯一的時間來源。

未校時的話：跨日不會自動歸零，事件時間也無法對應到真實日期。一併送出時區，裝置的「一天」才會跟手機在同一個邊界換日。

- Summary characteristic 的 `timeSynced` 會告訴你目前是否已校時。
- 事件 JSON 的 `timeSynced` 為 `false` 時，`occurredAt` 會是 `0`（時間未知），請改用收到事件的時間。

---

## 🧪 測試

```bash
pio test
```

需要接上開發板。測試會讀寫與韌體相同的 NVS namespace 並改動系統時間，**請勿對正在使用中的裝置執行**。

---

## 🎓 實體手作工作坊

本專案提供 5 小時實體工作坊課程與教學套件（定價 NT$ 2,500，含完整硬體材料包）：
- **工作坊說明與線上報名頁**：[workshop/index.html](workshop/index.html)
- **主辦人籌備與開課指南**：[workshop/README.md](workshop/README.md)
