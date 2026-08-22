# 🥤 智慧喝水偵測器 (ESP32-C3 SuperMini + HX711 5KG)

以 **ESP32-C3 SuperMini** 搭配 **5KG 圓形秤架 (HX711 稱重模組)** 打造的桌面智慧喝水追蹤器。具備自動感應水杯拿起/放下、喝水量計算、補水辨識、每日飲水進度統計、未喝水定時提醒、內建 Web 儀表板與 LINE Notify / Webhook 遠端推播。

---

## 🛠️ 硬體接線指南

### 1. ESP32-C3 SuperMini 與 HX711

| HX711 稱重模組 | ESP32-C3 SuperMini | 說明 |
| :--- | :--- | :--- |
| **VCC** | **3V3** (或 5V) | 供電 (建議接 3.3V) |
| **GND** | **GND** | 接地 |
| **DT (Data)** | **GPIO 4** | 數據輸出訊號線 |
| **SCK (Clock)**| **GPIO 5** | 時脈控制訊號線 |

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

- [platformio.ini](file:///Users/vince.huang/develop/tools/water/platformio.ini) : PlatformIO 環境與依賴庫設定
- [include/Config.h](file:///Users/vince.huang/develop/tools/water/include/Config.h) : 腳位與系統常數設定
- [src/ScaleManager.cpp](file:///Users/vince.huang/develop/tools/water/src/ScaleManager.cpp) : HX711 讀取、滑動濾波與校準邏輯
- [src/DrinkTracker.cpp](file:///Users/vince.huang/develop/tools/water/src/DrinkTracker.cpp) : 水杯狀態機、喝水/加水判斷、每日總計
- [src/Notifier.cpp](file:///Users/vince.huang/develop/tools/water/src/Notifier.cpp) : 板載 LED 燈效、LINE Notify 與 Webhook 推播
- [src/WebPortal.cpp](file:///Users/vince.huang/develop/tools/water/src/WebPortal.cpp) : 現代化 Web 儀表板、REST API、Captive Portal WiFi 配網
- [src/main.cpp](file:///Users/vince.huang/develop/tools/water/src/main.cpp) : 主程式入口

---

## 💻 燒錄與使用說明

### 1. 燒錄韌體
將 ESP32-C3 SuperMini 透過 Type-C 連接至電腦，在終端機執行：
```bash
# 編譯並上傳
pio run -t upload

# 開啟序列埠監視器 (115200 波特率)
pio device monitor
```

### 2. 初次使用與 WiFi 配網
1. 開機後若未配置 WiFi，ESP32 將發射熱點 `WaterTracker-xxxx` (預設密碼 `12345678`)。
2. 用手機連接該熱點，自動彈出 (Captive Portal) 或在瀏覽器輸入 `http://192.168.4.1`。
3. 點選「WiFi 設定」->「搜尋附近 WiFi」，選取您家中的 WiFi 並輸入密碼連線。
4. 連線後，即可在同網域電腦/手機瀏覽器直接輸入：
   - **`http://water.local`** 或 ESP32 的區域網路 IP

---

## ⚖️ 感測器兩步校準精靈

1. **第一步（清空秤面）**：
   - 確保秤面上沒有放置任何物品。
   - 在網頁儀表板上點選 **「⚡ 一鍵去皮 (歸零)」**。
2. **第二步（放置已知重量物品）**：
   - 放上已知重量的物品（例如一瓶 500g 的礦泉水，或用電子秤量過重量的手機/水杯）。
   - 在網頁「感測器校準精靈」欄位中輸入該重量（例如 `500`）。
   - 點選 **「🎯 執行校準並儲存」**。
   - 系統會自動計算校準係數並永久儲存至 ESP32 的 NVS 記憶體，重開機無需重複校準！

---

## 🔔 推播通知整合

- **LINE Notify**：
  1. 前往 [LINE Notify 官方網站](https://notify-bot.line.me/) 登入並發行存取權杖 (Personal Access Token)。
  2. 將 Token 填入 Web 儀表板設定中的「LINE Notify Token」並儲存。
- **Webhook**：
  - 支援將即時事件（喝水量、剩餘量、久未喝水提醒）POST 至 Home Assistant / Discord / Telegram Bot Webhook 等平台。
