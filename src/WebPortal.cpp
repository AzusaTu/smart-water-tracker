#include "WebPortal.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>智慧喝水偵測器</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #1e293b;
      --card-border: #334155;
      --primary: #38bdf8;
      --primary-hover: #0284c7;
      --accent: #10b981;
      --warning: #f59e0b;
      --danger: #ef4444;
      --text: #f8fafc;
      --text-muted: #94a3b8;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background: var(--bg); color: var(--text); padding: 16px; min-height: 100vh; display: flex; justify-content: center; }
    .container { width: 100%; max-width: 640px; display: flex; flex-direction: column; gap: 16px; }
    
    header { text-align: center; padding: 12px 0; }
    header h1 { font-size: 1.5rem; color: var(--primary); display: flex; align-items: center; justify-content: center; gap: 8px; }
    header p { font-size: 0.85rem; color: var(--text-muted); margin-top: 4px; }

    .card { background: var(--card); border: 1px solid var(--card-border); border-radius: 16px; padding: 18px; box-shadow: 0 4px 12px rgba(0,0,0,0.3); }
    .card-title { font-size: 1rem; font-weight: 600; color: var(--text-muted); margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }

    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }

    .metric-value { font-size: 2rem; font-weight: 700; color: var(--text); }
    .metric-unit { font-size: 0.9rem; font-weight: 400; color: var(--text-muted); margin-left: 4px; }
    .badge { padding: 4px 10px; border-radius: 999px; font-size: 0.75rem; font-weight: 600; }
    .badge-idle { background: #064e3b; color: #6ee7b7; }
    .badge-drinking { background: #1e3a8a; color: #93c5fd; }
    .badge-alert { background: #7f1d1d; color: #fca5a5; }

    /* Progress bar */
    .progress-bar-bg { background: #334155; height: 12px; border-radius: 6px; overflow: hidden; margin-top: 10px; }
    .progress-bar-fill { background: linear-gradient(90deg, #38bdf8, #10b981); height: 100%; width: 0%; transition: width 0.4s ease; }

    /* Buttons */
    .btn { display: inline-flex; align-items: center; justify-content: center; gap: 6px; padding: 10px 16px; border-radius: 10px; font-size: 0.9rem; font-weight: 600; cursor: pointer; border: none; transition: 0.2s; color: #fff; width: 100%; }
    .btn-primary { background: var(--primary); color: #0f172a; }
    .btn-primary:hover { background: var(--primary-hover); }
    .btn-secondary { background: #334155; color: var(--text); }
    .btn-secondary:hover { background: #475569; }
    .btn-warning { background: var(--warning); color: #0f172a; }
    .btn-danger { background: var(--danger); }
    .btn-group { display: flex; gap: 8px; margin-top: 12px; }

    /* Form Inputs */
    .form-group { margin-bottom: 12px; }
    .form-group label { display: block; font-size: 0.8rem; color: var(--text-muted); margin-bottom: 4px; }
    .form-input { width: 100%; padding: 10px 12px; background: #0f172a; border: 1px solid var(--card-border); border-radius: 8px; color: #fff; font-size: 0.9rem; }
    .form-input:focus { outline: none; border-color: var(--primary); }

    /* History Table */
    .history-list { display: flex; flex-direction: column; gap: 8px; max-height: 220px; overflow-y: auto; }
    .history-item { display: flex; justify-content: space-between; align-items: center; padding: 8px 12px; background: #0f172a; border-radius: 8px; font-size: 0.85rem; }
    .history-drink { color: var(--primary); font-weight: 600; }
    .history-refill { color: var(--accent); font-weight: 600; }

    /* Collapsible */
    details { border-top: 1px solid var(--card-border); padding-top: 10px; margin-top: 10px; }
    summary { cursor: pointer; color: var(--primary); font-size: 0.85rem; font-weight: 600; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <h1>🥤 智慧喝水偵測器</h1>
      <p id="system-status">連線中...</p>
    </header>

    <!-- 即時狀態卡片 -->
    <div class="card">
      <div class="card-title">
        <span>即時監測</span>
        <span id="badge-state" class="badge badge-idle">待機</span>
      </div>
      <div class="grid-2">
        <div>
          <div style="font-size:0.8rem; color:var(--text-muted);">當前秤重</div>
          <div><span id="val-weight" class="metric-value">0.0</span><span class="metric-unit">g</span></div>
        </div>
        <div>
          <div style="font-size:0.8rem; color:var(--text-muted);">距離上次喝水</div>
          <div><span id="val-since-drink" class="metric-value">0</span><span class="metric-unit">分</span></div>
        </div>
      </div>
      <div class="btn-group">
        <button class="btn btn-secondary" onclick="executeTare()">⚡ 一鍵去皮 (歸零)</button>
      </div>
    </div>

    <!-- 今日飲水目標卡片 -->
    <div class="card">
      <div class="card-title">
        <span>今日飲水進度</span>
        <span id="val-progress-text" style="font-size:0.9rem; color:var(--primary);">0 / 2000 ml</span>
      </div>
      <div class="progress-bar-bg">
        <div id="progress-bar-fill" class="progress-bar-fill"></div>
      </div>
      <div style="display:flex; justify-content:space-between; margin-top:8px; font-size:0.8rem; color:var(--text-muted);">
        <span id="val-percent">達成率: 0%</span>
        <a href="javascript:void(0)" onclick="resetDaily()" style="color:var(--danger); text-decoration:none;">重設今日紀錄</a>
      </div>
    </div>

    <!-- 喝水紀錄卡片 -->
    <div class="card">
      <div class="card-title">最近喝水紀錄</div>
      <div id="history-container" class="history-list">
        <div style="color:var(--text-muted); font-size:0.85rem; text-align:center; padding:12px;">尚無紀錄</div>
      </div>
    </div>

    <!-- 感測器校準 -->
    <div class="card">
      <div class="card-title">感測器校準精靈</div>
      <p style="font-size:0.8rem; color:var(--text-muted); margin-bottom:12px;">
        1. 先清空秤面並按「一鍵去皮」。<br>
        2. 放上已知重量物品 (如一瓶 500g 的水或手機)，輸入克數後按「儲存校準」。
      </p>
      <div class="form-group">
        <label>已知物體實際重量 (克)</label>
        <input type="number" id="cal-known-weight" class="form-input" placeholder="例如: 200" value="200">
      </div>
      <div class="btn-group">
        <button class="btn btn-warning" onclick="executeCalibrate()">🎯 執行校準並儲存</button>
      </div>
    </div>

    <!-- 參數與推播設定 (摺疊) -->
    <div class="card">
      <details>
        <summary>⚙️ 系統設定 & 推播配置</summary>
        <div style="margin-top:14px;">
          <div class="grid-2">
            <div class="form-group">
              <label>每日目標 (ml)</label>
              <input type="number" id="cfg-goal" class="form-input" value="2000">
            </div>
            <div class="form-group">
              <label>提醒間隔 (分鐘)</label>
              <input type="number" id="cfg-reminder" class="form-input" value="45">
            </div>
          </div>
          <div class="grid-2">
            <div class="form-group">
              <label>喝水最小閾值 (g)</label>
              <input type="number" id="cfg-min-drink" class="form-input" value="15">
            </div>
            <div class="form-group">
              <label>空秤閾值 (g)</label>
              <input type="number" id="cfg-empty-cup" class="form-input" value="25">
            </div>
          </div>
          <div class="form-group">
            <label>LINE Notify Token (可選)</label>
            <input type="password" id="cfg-line" class="form-input" placeholder="貼上 LINE Notify 權杖">
          </div>
          <div class="form-group">
            <label>Webhook URL (可選，如 Home Assistant/Discord)</label>
            <input type="text" id="cfg-webhook" class="form-input" placeholder="https://...">
          </div>
          <div class="btn-group">
            <button class="btn btn-primary" onclick="saveSettings()">儲存設定</button>
            <button class="btn btn-secondary" onclick="testNotification()">測試推播</button>
          </div>
        </div>
      </details>

      <details>
        <summary>📶 WiFi 設定</summary>
        <div style="margin-top:14px;">
          <div class="form-group">
            <label>WiFi 名稱 (SSID)</label>
            <input type="text" id="wifi-ssid" class="form-input" placeholder="選擇或輸入 WiFi 名稱">
          </div>
          <div class="form-group">
            <label>WiFi 密碼</label>
            <input type="password" id="wifi-pass" class="form-input" placeholder="WiFi 密碼">
          </div>
          <div class="btn-group">
            <button class="btn btn-primary" onclick="saveWifi()">連線 WiFi</button>
            <button class="btn btn-secondary" onclick="scanWifi()">搜尋附近 WiFi</button>
          </div>
          <div id="wifi-list" style="margin-top:10px; font-size:0.8rem;"></div>
        </div>
      </details>
    </div>
  </div>

  <script>
    let isEditing = false;

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        
        document.getElementById('val-weight').innerText = data.weight.toFixed(1);
        document.getElementById('val-since-drink').innerText = data.mins_since_drink;
        document.getElementById('badge-state').innerText = data.state;
        
        const badge = document.getElementById('badge-state');
        if (data.is_reminder_due) {
          badge.className = 'badge badge-alert';
          badge.innerText = '⏰ 請記得喝水!';
        } else if (data.state.includes('喝水') || data.state.includes('拿起')) {
          badge.className = 'badge badge-drinking';
        } else {
          badge.className = 'badge badge-idle';
        }

        const goal = data.daily_goal;
        const total = data.today_total;
        const pct = Math.min(100, Math.round((total / (goal || 1)) * 100));
        document.getElementById('val-progress-text').innerText = `${total} / ${goal} ml`;
        document.getElementById('val-percent').innerText = `達成率: ${pct}%`;
        document.getElementById('progress-bar-fill').style.width = `${pct}%`;

        document.getElementById('system-status').innerText = `IP: ${data.ip} | 訊號: ${data.rssi} dBm`;

        if (!isEditing) {
          document.getElementById('cfg-goal').value = data.daily_goal;
          document.getElementById('cfg-reminder').value = data.reminder_min;
          document.getElementById('cfg-min-drink').value = data.min_drink;
          document.getElementById('cfg-empty-cup').value = data.empty_cup;
        }
      } catch (err) {
        document.getElementById('system-status').innerText = '與裝置斷線中...';
      }
    }

    async function fetchHistory() {
      try {
        const res = await fetch('/api/history');
        const list = await res.json();
        const container = document.getElementById('history-container');
        if (list.length === 0) {
          container.innerHTML = '<div style="color:var(--text-muted); font-size:0.85rem; text-align:center; padding:12px;">尚無紀錄</div>';
          return;
        }
        container.innerHTML = list.map(item => `
          <div class="history-item">
            <span>${item.time_ago}</span>
            <span class="${item.type === 0 ? 'history-drink' : 'history-refill'}">
              ${item.type === 0 ? '🥤 喝水 +' + item.amount + ' ml' : '🚰 補水 +' + item.amount + ' ml'}
            </span>
            <span style="color:var(--text-muted); font-size:0.75rem;">(剩餘 ${item.remaining}g)</span>
          </div>
        `).join('');
      } catch (e) {}
    }

    async function executeTare() {
      if (!confirm('請確保秤面已清空，是否執行去皮 (歸零)？')) return;
      await fetch('/api/tare', { method: 'POST' });
      alert('已完成去皮');
      fetchStatus();
    }

    async function executeCalibrate() {
      const weight = parseFloat(document.getElementById('cal-known-weight').value);
      if (isNaN(weight) || weight <= 0) {
        alert('請輸入大於 0 的有效重量！');
        return;
      }
      if (!confirm(`請確認秤面上已放妥 ${weight} 克的物品，並保持靜止。是否開始校準？`)) return;
      const res = await fetch('/api/calibrate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ known_weight: weight })
      });
      const data = await res.json();
      alert(data.message || '校準已執行');
      fetchStatus();
    }

    async function resetDaily() {
      if (!confirm('確定要將今日喝水量重設為 0 嗎？')) return;
      await fetch('/api/reset_daily', { method: 'POST' });
      fetchStatus();
      fetchHistory();
    }

    async function saveSettings() {
      const payload = {
        daily_goal: parseInt(document.getElementById('cfg-goal').value),
        reminder_min: parseInt(document.getElementById('cfg-reminder').value),
        min_drink: parseFloat(document.getElementById('cfg-min-drink').value),
        empty_cup: parseFloat(document.getElementById('cfg-empty-cup').value),
        line_token: document.getElementById('cfg-line').value,
        webhook_url: document.getElementById('cfg-webhook').value
      };
      await fetch('/api/settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
      });
      alert('設定已儲存');
      isEditing = false;
      fetchStatus();
    }

    async function testNotification() {
      const res = await fetch('/api/test_notify', { method: 'POST' });
      const data = await res.json();
      alert(data.message);
    }

    async function scanWifi() {
      const listDiv = document.getElementById('wifi-list');
      listDiv.innerHTML = '正在搜尋附近的 WiFi 網路...';
      const res = await fetch('/api/wifi/scan');
      const networks = await res.json();
      if (networks.length === 0) {
        listDiv.innerHTML = '未搜尋到任何 WiFi 網路';
        return;
      }
      listDiv.innerHTML = networks.map(n => `
        <div style="padding:4px 0; cursor:pointer; color:var(--primary);" onclick="selectSsid('${n.ssid}')">
          📡 ${n.ssid} (${n.rssi} dBm)
        </div>
      `).join('');
    }

    function selectSsid(ssid) {
      document.getElementById('wifi-ssid').value = ssid;
      document.getElementById('wifi-pass').focus();
    }

    async function saveWifi() {
      const ssid = document.getElementById('wifi-ssid').value;
      const pass = document.getElementById('wifi-pass').value;
      if (!ssid) {
        alert('請輸入 WiFi 名稱');
        return;
      }
      if (!confirm(`確定要連線至 WiFi: ${ssid} 嗎？裝置將重新連線。`)) return;
      await fetch('/api/wifi/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ ssid: ssid, pass: pass })
      });
      alert('已送出 WiFi 連線設定，請稍候...');
    }

    ['cfg-goal', 'cfg-reminder', 'cfg-min-drink', 'cfg-empty-cup'].forEach(id => {
      document.getElementById(id).addEventListener('focus', () => isEditing = true);
    });

    setInterval(fetchStatus, 1500);
    setInterval(fetchHistory, 5000);
    fetchStatus();
    fetchHistory();
  </script>
</body>
</html>
)rawliteral";

WebPortal::WebPortal(ScaleManager& scale, DrinkTracker& tracker, Notifier& notifier)
    : _scale(scale),
      _tracker(tracker),
      _notifier(notifier),
      _server(80),
      _isApMode(false),
      _lastScanTime(0) {
}

void WebPortal::begin() {
    _prefs.begin(PREFS_NAMESPACE, true);
    String savedSSID = _prefs.getString("wifi_ssid", "");
    String savedPass = _prefs.getString("wifi_pass", "");
    _prefs.end();

    // 1. 設定 WiFi 模式為 AP+STA
    WiFi.mode(WIFI_AP_STA);
    WiFi.setTxPower(WIFI_POWER_19_5dBm); // 設定最高發射功率

    // 2. 開機立即廣播 SoftAP 熱點
    bool apOk = WiFi.softAP(AP_SSID_NAME, AP_DEFAULT_PASSWORD);
    _dnsServer.start(53, "*", WiFi.softAPIP());
    _isApMode = true;

    Serial.printf("[WebPortal] 📶 熱點已啟動 (%s): SSID: %s | 密碼: %s | IP: %s\n",
                  apOk ? "成功" : "失敗", AP_SSID_NAME, AP_DEFAULT_PASSWORD, WiFi.softAPIP().toString().c_str());

    // 3. 監聽 WiFi STA 事件
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.printf("\n[WiFi Event] ✅ 取得區域網路 IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.printf("\n[WiFi Event] STA 斷線 (原因: %d)\n", info.wifi_sta_disconnected.reason);
        }
    });

    // 4. 若有儲存的 SSID，進行連線嘗試
    if (savedSSID.length() > 0) {
        Serial.printf("[WebPortal] 嘗試連線至家用 WiFi: %s ...\n", savedSSID.c_str());
        WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
        WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
        WiFi.begin(savedSSID.c_str(), savedPass.c_str());

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
            delay(400);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[WebPortal] ✅ WiFi 連線成功! IP 位址: %s\n", WiFi.localIP().toString().c_str());
            if (MDNS.begin(MDNS_HOSTNAME)) {
                Serial.printf("[WebPortal] mDNS 啟用: http://%s.local\n", MDNS_HOSTNAME);
            }
        } else {
            Serial.println("[WebPortal] 家用 WiFi 暫未連線，請直接連線熱點: WaterTracker");
        }
    }

    setupRoutes();
    _server.begin();
    Serial.println("[WebPortal] Web 伺服器已在 Port 80 啟動");
}

void WebPortal::update() {
    if (_isApMode) {
        _dnsServer.processNextRequest();
    }
    _server.handleClient();
}

void WebPortal::setupRoutes() {
    _server.enableCORS(true);
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    // Captive Portal 探測端點 (Apple / Android / Windows)
    _server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/gen_204", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/connecttest.txt", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/redirect", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/success.txt", HTTP_GET, [this]() { _server.send(200, "text/plain", "success"); });

    _server.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
    _server.on("/api/history", HTTP_GET, [this]() { handleApiHistory(); });
    _server.on("/api/tare", HTTP_POST, [this]() { handleApiTare(); });
    _server.on("/api/calibrate", HTTP_POST, [this]() { handleApiCalibrate(); });
    _server.on("/api/settings", HTTP_POST, [this]() { handleApiSettings(); });
    _server.on("/api/reset_daily", HTTP_POST, [this]() { handleApiResetDaily(); });
    _server.on("/api/test_notify", HTTP_POST, [this]() { handleApiTestNotify(); });
    _server.on("/api/wifi/scan", HTTP_GET, [this]() { handleApiWifiScan(); });
    _server.on("/api/wifi/save", HTTP_POST, [this]() { handleApiWifiSave(); });

    // Captive Portal 重新導向
    _server.onNotFound([this]() { handleNotFound(); });
}

void WebPortal::handleRoot() {
    _server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebPortal::handleApiStatus() {
    JsonDocument doc;
    doc["weight"] = _scale.getFilteredWeight();
    doc["raw_weight"] = _scale.getWeight();
    doc["raw_adc"] = _scale.getRawValue();
    doc["sensor_connected"] = _scale.isConnected();
    doc["is_stable"] = _scale.isStable();
    doc["state"] = _tracker.getStateString();
    doc["today_total"] = _tracker.getTodayTotalMl();
    doc["daily_goal"] = _tracker.getDailyGoalMl();
    doc["reminder_min"] = _tracker.getReminderIntervalMinutes();
    doc["mins_since_drink"] = _tracker.getMinutesSinceLastDrink();
    doc["is_reminder_due"] = _tracker.isReminderDue();
    doc["min_drink"] = _tracker.getMinDrinkThreshold();
    doc["empty_cup"] = _tracker.getEmptyCupThreshold();
    doc["ip"] = (_isApMode && WiFi.status() != WL_CONNECTED) ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();
    doc["uptime_sec"] = millis() / 1000;

    String json;
    serializeJson(doc, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiHistory() {
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    int count = _tracker.getHistoryCount();
    unsigned long nowSec = millis() / 1000;

    for (int i = 0; i < count; i++) {
        DrinkRecord rec = _tracker.getHistoryRecord(i);
        JsonObject obj = array.add<JsonObject>();
        obj["type"] = (int)rec.type;
        obj["amount"] = rec.amountMl;
        obj["remaining"] = rec.remainingMl;
        obj["time"] = rec.timeStr;
        obj["timestamp"] = (long)rec.unixTimestamp;

        time_t nowSec = time(nullptr);
        if (nowSec > 1600000000 && rec.unixTimestamp > 1600000000) {
            long diffSec = nowSec - rec.unixTimestamp;
            if (diffSec < 60) {
                obj["time_ago"] = String(diffSec) + " 秒前";
            } else if (diffSec < 3600) {
                obj["time_ago"] = String(diffSec / 60) + " 分鐘前";
            } else {
                obj["time_ago"] = String(diffSec / 3600) + " 小時前";
            }
        } else {
            obj["time_ago"] = rec.timeStr;
        }
    }

    String json;
    serializeJson(doc, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiTare() {
    _scale.tare(15);
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "去皮成功";
    String json;
    serializeJson(doc, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiCalibrate() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"error\":\"缺少請求內容\"}");
        return;
    }

    JsonDocument req;
    deserializeJson(req, _server.arg("plain"));
    float knownWeight = req["known_weight"] | 0.0f;

    bool ok = _scale.calibrateWithKnownWeight(knownWeight);
    JsonDocument res;
    res["success"] = ok;
    res["message"] = ok ? "校準成功並已存入記憶體" : "校準失敗，請確認感測器狀態";
    res["factor"] = _scale.getCalibrationFactor();

    String json;
    serializeJson(res, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiSettings() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"error\":\"缺少請求內容\"}");
        return;
    }

    JsonDocument req;
    deserializeJson(req, _server.arg("plain"));

    if (!req["daily_goal"].isNull()) {
        _tracker.setDailyGoalMl(req["daily_goal"]);
    }
    if (!req["reminder_min"].isNull()) {
        _tracker.setReminderIntervalMinutes(req["reminder_min"]);
    }
    if (!req["min_drink"].isNull()) {
        _tracker.setMinDrinkThreshold(req["min_drink"]);
    }
    if (!req["empty_cup"].isNull()) {
        _tracker.setEmptyCupThreshold(req["empty_cup"]);
    }
    if (!req["line_token"].isNull()) {
        _notifier.setLineToken(req["line_token"].as<String>());
    }
    if (!req["webhook_url"].isNull()) {
        _notifier.setWebhookUrl(req["webhook_url"].as<String>());
    }

    JsonDocument res;
    res["success"] = true;
    res["message"] = "設定已更新";
    String json;
    serializeJson(res, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiResetDaily() {
    _tracker.resetDailyTotal();
    JsonDocument doc;
    doc["success"] = true;
    String json;
    serializeJson(doc, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiTestNotify() {
    String msg;
    bool ok = _notifier.testPushNotification(msg);
    JsonDocument doc;
    doc["success"] = ok;
    doc["message"] = msg;
    String json;
    serializeJson(doc, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiWifiScan() {
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    for (int i = 0; i < n; ++i) {
        JsonObject item = array.add<JsonObject>();
        item["ssid"] = WiFi.SSID(i);
        item["rssi"] = WiFi.RSSI(i);
        item["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    String json;
    serializeJson(doc, json);
    _server.send(200, "application/json", json);
}

void WebPortal::handleApiWifiSave() {
    if (!_server.hasArg("plain")) {
        _server.send(400, "application/json", "{\"error\":\"缺少請求內容\"}");
        return;
    }

    JsonDocument req;
    deserializeJson(req, _server.arg("plain"));
    String ssid = req["ssid"] | "";
    String pass = req["pass"] | "";

    _prefs.begin(PREFS_NAMESPACE, false);
    _prefs.putString("wifi_ssid", ssid);
    _prefs.putString("wifi_pass", pass);
    _prefs.end();

    JsonDocument res;
    res["success"] = true;
    res["message"] = "WiFi 設定已儲存，5 秒後重新開機連線";
    String json;
    serializeJson(res, json);
    _server.send(200, "application/json", json);

    delay(1000);
    ESP.restart();
}

void WebPortal::handleNotFound() {
    if (_isApMode) {
        _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
        _server.send(302, "text/plain", "");
    } else {
        _server.send(404, "text/plain", "Not Found");
    }
}
