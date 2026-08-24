# 🥤 Smart Water Tracker Backend API

Node.js + TypeScript + Express + SQLite 輕量化 RESTful API 後端服務，專為 ESP32-C3 智慧喝水杯與手機/網頁應用程式設計。

---

## 🌟 功能特色

- **雙重身分認證 (Dual Auth)**：
  - 用戶端（網頁/App）：Email + 密碼 + **JWT Token** (30 天有效)
  - 裝置端（ESP32-C3）：**Device Token**（綁定時產生，永久有效且可撤銷）
- **韌體事件完全對齊**：
  - 直接支援 ESP32-C3 上傳的 JSON 格式（包含 `eventId`, `type`, `amountMl`, `remainingMl`, `todayTotalMl`, `timeSynced`）。
  - 具備 **`eventId` 冪等去重**機制，防止重播緩衝重複計數。
  - 支援 **喝水 (`drink`)** 與 **補水 (`refill`)** 事件，補水僅記錄不計入喝水總量。
  - 當裝置尚未校時 (`timeSynced: false`) 時，自動採用伺服器時間。
- **統計分析**：
  - 台北時區 (`Asia/Taipei`) 00:00 自動日分界。
  - 當日進度與目標達成率、週分析趨勢、月分析與連續達標天數 (Streak)。
- **內建網頁管理儀表板**：
  - 瀏覽器打開即用：喝水進度圓環、裝置管理與在線狀態 (Green Dot)、週趨勢圖 (Chart.js)、歷程表格、即時模擬測試工具。
- **極致輕量**：
  - 採用 Node.js 24 原生內建 `node:sqlite` (零外部 C++ 編譯依賴)，RAM 佔用僅 ~35MB，完美適合各型雲端 VPS。

---

## 🚀 快速啟動

### 1. 安裝與啟動

```bash
# 在 backend 目錄下
npm install

# 啟動開發伺服器 (預設 Port 3000)
npm run dev

# 正式編譯與運行
npm run build
npm start

# 執行自動化測試 (21 個測試全通過)
npm test
```

### 2. 環境變數設定 (`.env`)

```env
PORT=3000
NODE_ENV=development
JWT_SECRET=smart_water_tracker_super_secret_jwt_key_2026
DATABASE_PATH=./data/water_tracker.db
```

---

## 📡 API 端點總覽

### 🩺 系統監控
- `GET /api/v1/health` — 健康檢查（回傳 uptime, service 名稱）

### 🔐 用戶認證 (`/api/v1/auth`)
- `POST /api/v1/auth/register` — 用戶註冊 `{ email, password, displayName? }`
- `POST /api/v1/auth/login` — 用戶登入 `{ email, password }`

### 👤 個人檔案 (`/api/v1/user`)
- `GET /api/v1/user/me` — 取得個人檔案與喝水目標 `[JWT]`
- `PUT /api/v1/user/me` — 更新個人檔案與喝水目標 `{ displayName?, dailyGoalMl? }` `[JWT]`

### 📱 裝置管理 (`/api/v1/devices`)
- `POST /api/v1/devices` — 綁定裝置 `{ deviceId, name? }` `[JWT]` (回傳 `deviceToken`)
- `GET /api/v1/devices` — 列出已綁定裝置與在線狀態 `[JWT]`
- `DELETE /api/v1/devices/:id` — 解除裝置綁定並撤銷 Token `[JWT]`
- `GET /api/v1/devices/:id/status` — 查詢單一裝置即時在線狀態 `[JWT]`

### 💧 喝水紀錄與統計 (`/api/v1/water`)
- `POST /api/v1/water/records` — 裝置/App 上傳事件 `[Device Token 或 JWT]`
- `GET /api/v1/water/records` — 查詢喝水歷程 `?from=&to=&type=&page=&limit=` `[JWT]`
- `GET /api/v1/water/stats/daily` — 當日進度與達成率 `?date=YYYY-MM-DD` `[JWT]`
- `GET /api/v1/water/stats/weekly` — 過去 7 日飲水趨勢 `[JWT]`
- `GET /api/v1/water/stats/monthly` — 過去 30 日月統計與 Streak `[JWT]`

---

## 🌐 網頁儀表板

伺服器啟動後，使用瀏覽器開啟：
👉 `http://localhost:3000`
