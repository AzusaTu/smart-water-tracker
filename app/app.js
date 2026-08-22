// 智慧喝水偵測器 Mobile App 核心邏輯
let deviceBaseUrl = localStorage.getItem('water_device_url') || 'http://192.168.68.131';
let currentGoal = parseInt(localStorage.getItem('water_daily_goal')) || 2000;
let hourlyChartInstance = null;
let weeklyChartInstance = null;

// 初始化
document.addEventListener('DOMContentLoaded', () => {
  lucide.createIcons();
  document.getElementById('setting-ip').value = deviceBaseUrl;
  document.getElementById('setting-goal').value = currentGoal;

  initCharts();
  fetchDeviceData();
  setInterval(fetchDeviceData, 2000);
});

// 切換 Tab
function switchTab(tabId) {
  document.querySelectorAll('.tab-content').forEach(el => el.classList.add('hidden'));
  document.getElementById(`tab-${tabId}`).classList.remove('hidden');

  const navs = ['dashboard', 'analytics', 'settings'];
  navs.forEach(nav => {
    const btn = document.getElementById(`nav-${nav}`);
    if (nav === tabId) {
      btn.className = 'flex flex-col items-center gap-1 text-brand-400 transition';
    } else {
      btn.className = 'flex flex-col items-center gap-1 text-slate-500 hover:text-slate-300 transition';
    }
  });

  if (tabId === 'analytics') {
    updateCharts();
  }
}

// 取得 ESP32 數據
async function fetchDeviceData() {
  try {
    const res = await fetch(`${deviceBaseUrl}/api/status`, { mode: 'cors' });
    const data = await res.json();

    // 更新連線指示燈
    document.getElementById('conn-indicator').className = 'w-2 h-2 rounded-full bg-emerald-400 animate-pulse';
    document.getElementById('device-info').innerText = `已連線至 ${data.ip || deviceBaseUrl}`;

    // 更新即時重量與水杯水位動畫
    const weight = data.weight || 0;
    document.getElementById('live-weight').innerText = weight.toFixed(1);
    document.getElementById('mins-since').innerText = data.mins_since_drink || 0;

    // 水杯動畫 (假設滿杯容量約 500g)
    const maxCupGrams = 500;
    const cupPercent = Math.min(100, Math.max(0, Math.round((weight / maxCupGrams) * 100)));
    document.getElementById('cup-water-level').style.height = `${cupPercent}%`;
    document.getElementById('cup-percent-text').innerText = `${cupPercent}%`;

    // 狀態標籤
    const badge = document.getElementById('state-badge');
    badge.innerText = data.state || '待機中';
    if (data.is_reminder_due) {
      badge.className = 'px-2.5 py-0.5 rounded-full text-xs font-semibold bg-rose-500/20 text-rose-300 border border-rose-500/30';
      badge.innerText = '⏰ 該喝水囉!';
    } else if (data.state && (data.state.includes('喝水') || data.state.includes('拿起'))) {
      badge.className = 'px-2.5 py-0.5 rounded-full text-xs font-semibold bg-brand-500/20 text-brand-300 border border-brand-500/30';
    } else {
      badge.className = 'px-2.5 py-0.5 rounded-full text-xs font-semibold bg-emerald-500/20 text-emerald-300 border border-emerald-500/30';
    }

    // 目標進度
    const total = data.today_total || 0;
    const goal = data.daily_goal || currentGoal;
    currentGoal = goal;
    document.getElementById('val-today-total').innerText = total;
    document.getElementById('val-daily-goal').innerText = goal;

    const percent = Math.min(100, Math.round((total / (goal || 1)) * 100));
    document.getElementById('progress-bar-fill').style.width = `${percent}%`;
    document.getElementById('val-achieve-percent').innerText = `達成率: ${percent}%`;

    const remain = Math.max(0, goal - total);
    document.getElementById('val-remaining-ml').innerText = remain > 0 ? `還差 ${remain} ml 達標` : '🎉 今日已達標！';

    // 取得歷史紀錄
    fetchHistoryData();

  } catch (err) {
    document.getElementById('conn-indicator').className = 'w-2 h-2 rounded-full bg-rose-500';
    document.getElementById('device-info').innerText = '正在嘗試重新連線...';
  }
}

// 取得歷史紀錄清單
async function fetchHistoryData() {
  try {
    const res = await fetch(`${deviceBaseUrl}/api/history`, { mode: 'cors' });
    const list = await res.json();
    const container = document.getElementById('history-timeline');
    document.getElementById('history-count-badge').innerText = `${list.length} 筆`;

    if (list.length === 0) {
      container.innerHTML = '<div class="text-center py-6 text-xs text-slate-500">尚無喝水事件</div>';
      return;
    }

    container.innerHTML = list.map(item => {
      const isDrink = item.type === 0;
      return `
        <div class="flex items-center justify-between p-3 rounded-2xl bg-slate-900/60 border border-slate-800/80">
          <div class="flex items-center gap-3">
            <div class="w-8 h-8 rounded-xl ${isDrink ? 'bg-brand-500/20 text-brand-400' : 'bg-emerald-500/20 text-emerald-400'} flex items-center justify-center text-sm font-bold">
              ${isDrink ? '🥤' : '🚰'}
            </div>
            <div>
              <div class="text-xs font-bold ${isDrink ? 'text-brand-300' : 'text-emerald-300'}">
                ${isDrink ? '喝水 +' + item.amount + ' ml' : '補水 +' + item.amount + ' ml'}
              </div>
              <div class="text-[10px] text-slate-400">${item.time || item.time_ago}</div>
            </div>
          </div>
          <span class="text-[11px] text-slate-400 font-medium">剩餘 ${item.remaining}g</span>
        </div>
      `;
    }).join('');
  } catch (e) {}
}

// 遠端去皮
async function executeRemoteTare() {
  if (!confirm('請確保秤盤清空，是否執行一鍵去皮？')) return;
  try {
    await fetch(`${deviceBaseUrl}/api/tare`, { method: 'POST', mode: 'cors' });
    alert('去皮歸零成功！');
    fetchDeviceData();
  } catch (e) {
    alert('操作失敗，請確認與裝置在同一個 WiFi 網路');
  }
}

// 遠端校準
async function executeRemoteCalibrate() {
  const weight = parseFloat(document.getElementById('cal-weight-input').value);
  if (isNaN(weight) || weight <= 0) {
    alert('請輸入有效的大於 0 克數');
    return;
  }
  if (!confirm(`請確認秤盤上已放上 ${weight} 克的物品，開始執行校準？`)) return;
  try {
    const res = await fetch(`${deviceBaseUrl}/api/calibrate`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ known_weight: weight }),
      mode: 'cors'
    });
    const data = await res.json();
    alert(data.message || '校準完成！');
    fetchDeviceData();
  } catch (e) {
    alert('校準請求失敗');
  }
}

// 個人化目標計算
function calculateGoal() {
  const weight = parseFloat(document.getElementById('calc-weight').value) || 65;
  const calculated = Math.round(weight * 35);
  document.getElementById('setting-goal').value = calculated;
  alert(`依據體重 ${weight}kg，建議每日飲水目標為：${calculated} ml`);
}

// 儲存 App 設定
async function saveAppSettings() {
  const newIp = document.getElementById('setting-ip').value.trim();
  const newGoal = parseInt(document.getElementById('setting-goal').value);
  const newReminder = parseInt(document.getElementById('setting-reminder').value);

  deviceBaseUrl = newIp;
  currentGoal = newGoal;
  localStorage.setItem('water_device_url', deviceBaseUrl);
  localStorage.setItem('water_daily_goal', currentGoal);

  try {
    await fetch(`${deviceBaseUrl}/api/settings`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        daily_goal: newGoal,
        reminder_min: newReminder
      }),
      mode: 'cors'
    });
    alert('設定已儲存並同步至 ESP32');
  } catch (e) {
    alert('已儲存本機設定 (無法連線至 ESP32 同步)');
  }
  fetchDeviceData();
}

// 手動補登喝水
function setManualAmount(ml) {
  document.getElementById('manual-amount').value = ml;
}

function submitManualDrink() {
  const ml = parseInt(document.getElementById('manual-amount').value) || 200;
  toggleModal('modal-add');
  alert(`已手動記錄喝水 +${ml} ml！`);
  // 記錄至本機歷史
}

function toggleModal(id) {
  const el = document.getElementById(id);
  el.classList.toggle('hidden');
}

// 初始化圖表
function initCharts() {
  const ctxHourly = document.getElementById('hourlyChart').getContext('2d');
  hourlyChartInstance = new Chart(ctxHourly, {
    type: 'bar',
    data: {
      labels: ['08:00', '10:00', '12:00', '14:00', '16:00', '18:00', '20:00'],
      datasets: [{
        label: '飲水量 (ml)',
        data: [250, 180, 320, 200, 450, 150, 300],
        backgroundColor: '#38bdf8',
        borderRadius: 8
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { grid: { display: false }, ticks: { color: '#94a3b8', font: { size: 10 } } },
        y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8', font: { size: 10 } } }
      }
    }
  });

  const ctxWeekly = document.getElementById('weeklyChart').getContext('2d');
  weeklyChartInstance = new Chart(ctxWeekly, {
    type: 'line',
    data: {
      labels: ['週一', '週二', '週三', '週四', '週五', '週六', '今日'],
      datasets: [{
        label: '飲水量',
        data: [1950, 2200, 1800, 2050, 2400, 1600, 1850],
        borderColor: '#10b981',
        backgroundColor: 'rgba(16, 185, 129, 0.1)',
        fill: true,
        tension: 0.4,
        pointBackgroundColor: '#10b981'
      }]
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      plugins: { legend: { display: false } },
      scales: {
        x: { grid: { display: false }, ticks: { color: '#94a3b8', font: { size: 10 } } },
        y: { grid: { color: 'rgba(255,255,255,0.05)' }, ticks: { color: '#94a3b8', font: { size: 10 } } }
      }
    }
  });
}

function updateCharts() {
  if (hourlyChartInstance) hourlyChartInstance.update();
  if (weeklyChartInstance) weeklyChartInstance.update();
}
