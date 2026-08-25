# Device Claim Secret Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep the ESP32 claim secret stable across reboots and make ownership transfer use a hardware-generated replacement secret that is synchronized with the backend.

**Architecture:** The ESP32 is the source of truth for claim secrets, persisting one in Preferences/NVS. The App retains the old secret, rotates the device secret over BLE, and sends both values to the backend; the backend verifies the old value and persists the replacement instead of inventing one it cannot deliver to the device.

**Tech Stack:** ESP32 Arduino/Preferences, BLE, TypeScript, Express, Node.js SQLite, Jest, GitHub Actions.

---

### Task 1: Persist the firmware claim secret

**Files:**
- Modify: `firmware/src/BleWaterService.cpp`
- Modify: `firmware/include/BleWaterService.h`
- Test: `firmware/test/test_ble_water_service/test_main.cpp`

- [ ] **Step 1: Add a failing persistence test seam**

Add a small `loadOrCreateClaimSecret()` private method and test the observable contract: a service initialized twice with the same persisted Preferences namespace reads the same non-empty secret; after `rotateClaimSecret()`, a newly initialized service reads the rotated value.

- [ ] **Step 2: Verify the current firmware test baseline**

Run: `pio test -d firmware -e esp32-c3-supermini`

Expected: current tests either pass or expose the existing PlatformIO host limitation before the persistence assertions are added.

- [ ] **Step 3: Implement NVS-backed secret creation and rotation**

Use `Preferences` with namespace `PREFS_NAMESPACE` and key `claim_secret`. On `begin()`, read the key; if it is absent, generate a 128-bit hexadecimal secret with two `esp_random()` calls and persist it before assigning `_claimSecret`. In `rotateClaimSecret()`, generate and persist the replacement before updating the BLE summary. Remove the constructor-generated placeholder and do not print the secret to Serial.

- [ ] **Step 4: Run firmware build and tests**

Run: `pio run -d firmware && pio test -d firmware -e esp32-c3-supermini`

Expected: firmware compiles and the expanded test suite passes.

- [ ] **Step 5: Commit**

```bash
git add firmware/include/BleWaterService.h firmware/src/BleWaterService.cpp firmware/test/test_ble_water_service/test_main.cpp
git commit -m "fix: 持久化裝置認領金鑰"
```

### Task 2: Keep backend transfer aligned with the device secret

**Files:**
- Modify: `backend/src/controllers/deviceController.ts`
- Modify: `backend/tests/api.test.ts`

- [ ] **Step 1: Write a failing transfer contract test**

Update the claiming test so the original secret is `oldClaimCode` and the valid transfer request supplies both `claimCode: oldClaimCode` and `newClaimCode`. Assert that a subsequent transfer with `oldClaimCode` is rejected and that a transfer using `newClaimCode` as the next `claimCode` succeeds, proving the database retains the secret supplied by the hardware rather than generating an unreachable replacement.

- [ ] **Step 2: Run the focused test before implementation**

Run: `npm run backend:test -- --runInBand -t "Hardware Claiming"`

Expected: FAIL because the endpoint currently generates `newRotatedClaimCode` independently.

- [ ] **Step 3: Implement the synchronized transfer update**

In `bindDevice`, require both `claimCode` and `newClaimCode` for a transfer, compare `claimCode` to the stored value, rotate only `device_token`, and update `claim_code` to `newClaimCode`. Rename misleading local variables and response text so it does not claim that the backend rotated hardware state.

- [ ] **Step 4: Run backend verification**

Run: `npm run backend:test && npm run backend:build`

Expected: all Jest tests pass and TypeScript emits with zero errors.

- [ ] **Step 5: Commit**

```bash
git add backend/src/controllers/deviceController.ts backend/tests/api.test.ts
git commit -m "fix: 同步裝置認領金鑰轉讓"
```

### Task 3: Repair the PlatformIO CI job

**Files:**
- Modify: `.github/workflows/ci.yml`

- [ ] **Step 1: Remove unsupported pip cache configuration**

Delete `cache: 'pip'` from `actions/setup-python`. Keep Python 3.11 and the explicit `pip install --upgrade platformio` step.

- [ ] **Step 2: Verify the workflow shape locally**

Run: `rg -n "cache: 'pip'|pio run -d firmware" .github/workflows/ci.yml`

Expected: no `cache: 'pip'` match and one firmware build command match.

- [ ] **Step 3: Commit**

```bash
git add .github/workflows/ci.yml
git commit -m "fix: 修正韌體 CI Python 快取設定"
```

### Task 4: Final integration verification

**Files:**
- Verify: `firmware/src/BleWaterService.cpp`
- Verify: `backend/src/controllers/deviceController.ts`
- Verify: `.github/workflows/ci.yml`

- [ ] **Step 1: Check the final diff and whitespace**

Run: `git diff origin/main...HEAD --check && git diff origin/main...HEAD --stat`

Expected: no whitespace errors and only the intended firmware, backend, test, CI, and design documents are changed.

- [ ] **Step 2: Run all locally available checks**

Run: `npm run backend:test && npm run backend:build && pio run -d firmware`

Expected: every command exits successfully.

- [ ] **Step 3: Push the PR branch and confirm GitHub checks**

Run: `git push origin feat/backend-service`

Expected: GitHub starts CI; merge only after both backend and PlatformIO jobs succeed.
