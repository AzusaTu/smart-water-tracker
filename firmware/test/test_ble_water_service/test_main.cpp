#include <Arduino.h>
#include <unity.h>

#include "BleWaterService.h"

void test_history_replay_returns_only_events_after_cursor() {
    BleWaterService service("water_c3_a1b2");
    service.recordDrink(1721389200, 250, 450, 250);
    const String firstId = service.latestEventId();
    service.recordDrink(1721389201, 200, 250, 450);

    const std::vector<BleWaterEvent> replay = service.eventsAfter(firstId);
    TEST_ASSERT_EQUAL_UINT32(1, replay.size());
    TEST_ASSERT_EQUAL_STRING(service.latestEventId().c_str(), replay[0].id.c_str());
}

void test_refill_event_recording_and_64bit_boot_session() {
    BleWaterService service1("water_c3_a1b2");
    service1.recordRefill(0, 500, 600, 250); // Unsynced time (0)
    const String id1 = service1.latestEventId();

    const std::vector<BleWaterEvent> events = service1.eventsAfter("");
    TEST_ASSERT_EQUAL_UINT32(1, events.size());
    TEST_ASSERT_EQUAL(EVENT_REFILL, events[0].type);
    TEST_ASSERT_EQUAL_INT(500, events[0].amountMl);

    BleWaterService service2("water_c3_a1b2"); // Simulated reboot (new 64-bit session)
    service2.recordRefill(0, 500, 600, 250);
    const String id2 = service2.latestEventId();

    // Even with occurredAt = 0 and seq = 0, event IDs from different boot sessions must be distinct!
    TEST_ASSERT_NOT_EQUAL(id1, id2);
    TEST_ASSERT_GREATER_THAN(20, id1.length());

    const String claimBeforeRotation = service1.claimSecret();
    TEST_ASSERT_TRUE(service1.rotateClaimSecret());
    TEST_ASSERT_NOT_EQUAL(claimBeforeRotation, service1.claimSecret());
    TEST_ASSERT_NOT_EQUAL(service1.claimSecret(), "");
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_history_replay_returns_only_events_after_cursor);
    RUN_TEST(test_refill_event_recording_and_64bit_boot_session);
    UNITY_END();
}

void loop() {}
