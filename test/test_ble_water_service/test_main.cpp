#include <Arduino.h>
#include <unity.h>

#include "BleWaterService.h"

void test_history_replay_returns_only_events_after_cursor() {
    BleWaterService service("water_c3_a1b2");
    service.recordDrink(1721389200, 250, 450, 250);
    const String firstId = service.latestEventId();
    service.recordDrink(1721389201, 200, 250, 450);

    const std::vector<BleDrinkEvent> replay = service.eventsAfter(firstId);
    TEST_ASSERT_EQUAL_UINT32(1, replay.size());
    TEST_ASSERT_EQUAL_STRING(service.latestEventId().c_str(), replay[0].id.c_str());
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_history_replay_returns_only_events_after_cursor);
    UNITY_END();
}

void loop() {}
