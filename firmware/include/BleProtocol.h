#pragma once

#include <Arduino.h>

// Keep these identifiers in one place. The Flutter client uses the same UUIDs.
namespace BleProtocol {
constexpr char SERVICE_UUID[] = "7d5a0001-2d5c-4d2f-9f85-7d7bf8a9a101";
constexpr char LIVE_EVENT_UUID[] = "7d5a0002-2d5c-4d2f-9f85-7d7bf8a9a101";
constexpr char SUMMARY_UUID[] = "7d5a0003-2d5c-4d2f-9f85-7d7bf8a9a101";
constexpr char HISTORY_SYNC_UUID[] = "7d5a0004-2d5c-4d2f-9f85-7d7bf8a9a101";
constexpr char COMMAND_UUID[] = "7d5a0005-2d5c-4d2f-9f85-7d7bf8a9a101";

constexpr size_t EVENT_BUFFER_SIZE = 64;
constexpr char DEVICE_NAME_PREFIX[] = "WaterTracker-";
}  // namespace BleProtocol
