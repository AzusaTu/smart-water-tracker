# Device Claim Secret Design

## Goal

Make ownership transfer prove physical possession of an ESP32 device without
breaking after a reboot or leaving the backend and hardware with different
claim secrets.

## Contract

- The ESP32 owns the claim secret. On first boot it generates a cryptographically
  random 128-bit value and saves it in Preferences/NVS. Every later boot reads
  the same value.
- The App may read the secret only through a local BLE connection. The backend
  receives it as `claimCode` only when initially binding or transferring a
  device.
- A transfer is an explicit two-phase App operation: read and retain the current
  secret, issue `rotate_claim` over BLE, then call `POST /api/v1/devices` with
  `claimCode` (the retained old secret) and `newClaimCode` (the replacement).
  The backend validates the old secret, then atomically changes owner, device
  token, and stored claim secret to the replacement.
- If the backend request fails after the BLE rotation, the App retains the new
  secret and retries the same transfer request. It never tries to restore the
  old secret.
- A direct backend request with a stale secret is rejected. The backend never
  invents a claim secret because it cannot write that value to the device.

## Firmware

`BleWaterService` will load or create its claim secret in Preferences under the
existing `water_app` namespace. `rotateClaimSecret()` will persist the new
secret before making it visible through the BLE summary/command response. The
secret is never logged to Serial.

## Backend

The binding endpoint accepts the device's current secret. On a transfer it
compares that submitted value to the stored value, generates a new device token,
and keeps the submitted replacement secret as `claim_code`. It does not generate
another server-only secret.

## Tests and CI

- Firmware tests cover persisted secret loading and rotation where the PlatformIO
  test environment supports Preferences; backend tests cover transfer using a
  replacement secret and stale-secret rejection.
- The PlatformIO workflow does not enable pip dependency caching because this
  repository deliberately has no Python dependency manifest. It installs
  PlatformIO directly before compiling `firmware/`.
