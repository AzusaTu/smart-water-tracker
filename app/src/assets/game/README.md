# Hydration Game Assets

Issue: #3 — 喝水遊戲化 MVP：水能量、Boss 戰與獎勵循環

這個目錄是 `app` 前端的遊戲化 presentation assets。第一版刻意使用 self-contained SVG：可直接由 Vite import、透明素材可自由疊在場景上，也方便之後替換正式 PNG/WebP/sprite 而不影響 game domain。

## Asset inventory

| Asset | Path | Suggested use |
| --- | --- | --- |
| 深海巨鯨 idle | `boss/whale_idle.svg` | 首頁 Boss / 戰鬥待機 |
| 深海巨鯨 hit | `boss/whale_hit.svg` | 攻擊命中後短暫切換 |
| 玩家攻擊 | `player/player_attack.svg` | Attack action |
| 水滴吉祥物 | `mascot/water_drop.svg` | 喝水成功 / reward feedback |
| 水流攻擊 | `effects/water_blast.svg` | 玩家 → Boss 的 attack effect |
| 金幣 | `rewards/coin.svg` | 點數 / reward |
| 寶箱 | `rewards/chest.svg` | Boss defeat reward |
| 水能量 | `icons/water_energy.svg` | Energy counter / attack cost |
| 深海背景 | `backgrounds/deep_sea.svg` | Boss battle scene background |

## Import example

```tsx
import whaleIdleUrl from './assets/game/boss/whale_idle.svg';
import whaleHitUrl from './assets/game/boss/whale_hit.svg';
import waterBlastUrl from './assets/game/effects/water_blast.svg';

<img src={whaleIdleUrl} alt="深海巨鯨" />
```

## Recommended display sizes

These are display guidelines, not source-size constraints.

| Type | Mobile display target |
| --- | --- |
| Boss | 220–320 px wide |
| Player | 120–180 px wide |
| Mascot | 72–120 px |
| Energy / Coin icon | 20–36 px |
| Chest | 96–160 px |
| Water blast | animate across battle stage; preserve aspect ratio |
| Deep sea background | `object-fit: cover` / background cover |

## State usage

### Boss hit

建議 attack 命中時：

1. `whale_idle.svg` → `whale_hit.svg`
2. 顯示 `water_blast.svg`
3. 顯示傷害數字，例如 `-120`
4. 約 180–300 ms 後回 idle

不要讓 domain state 依賴動畫完成；動畫只是 presentation feedback。

### Reduced motion

`prefers-reduced-motion: reduce` 時：

- 不需要位移 / shake 動畫。
- 可直接切換 hit asset + HP 數字。
- 攻擊與 reward 行為必須完全可用。

## Art direction

第一批素材共同方向：

- 深海藍 / 水藍主色
- 深藍描邊，確保在深色背景仍有輪廓
- 圓潤、輕量 RPG / 手遊感
- 不使用照片
- UI 應維持 hydration app 的清楚資訊層級，不讓裝飾壓過今日喝水進度

## Product safety rule

素材與動畫不得引導使用者為了刷遊戲收益而超量喝水。遊戲能量應依 Issue #3 的規則，只針對每日 hydration goal 範圍內的有效水量產生。

## Future replacement

若之後需要更完整的動畫，可保持檔名/domain identity，逐步換成：

- WebP animation
- PNG sprite sheet
- Rive / Lottie（確認 bundle 與 runtime 成本後）

不建議 MVP 一開始就引入複雜 animation runtime。
