// Tests for the AXP2101 event entity. Living in this subdirectory also tells the
// unit test harness to compile the `event` platform of this component.

#include <gtest/gtest.h>

#include "esphome/components/axp2101/event/axp2101_event.h"

#include "../axp2101_test.h"

namespace esphome::axp2101::testing {

namespace {

// Subscribing from a test is the same call codegen makes: one static table.
const char *const POWER_KEY_EVENTS[] = {"power_key_short_press", "power_key_long_press"};
const char *const VBUS_ONLY_EVENTS[] = {"vbus_insert"};

// Status register bits, from the interrupt table in axp2101.cpp.
constexpr uint8_t IRQ_STATUS_1 = 0x49;
constexpr uint8_t VBUS_INSERT_BIT = 0x80;
constexpr uint8_t POWER_KEY_SHORT_BIT = 0x08;

}  // namespace

TEST_F(Axp2101Test, EventEntityReportsTheSourceThatFired) {
  AXP2101Event entity;
  entity.set_parent(&this->axp_);
  entity.set_event_types_static(POWER_KEY_EVENTS, 2);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  // Any read of the status registers happens on the hub's loop, so drive it.
  this->bus_.poke(IRQ_STATUS_1, POWER_KEY_SHORT_BIT);
  LoopBlockingGuard guard(nullptr, nullptr, 1000);
  this->axp_.loop();

  EXPECT_TRUE(entity.has_event());
  EXPECT_EQ(entity.get_last_event_type(), "power_key_short_press");
}

TEST_F(Axp2101Test, EventEntityEnablesOnlyTheSourcesItListensFor) {
  AXP2101Event entity;
  entity.set_parent(&this->axp_);
  entity.set_event_types_static(VBUS_ONLY_EVENTS, 1);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  // vbus_insert is bit 7 of the second enable register.
  EXPECT_EQ(this->bus_.read(0x40), 0x00);
  EXPECT_EQ(this->bus_.read(0x41), 0x80);
  EXPECT_EQ(this->bus_.read(0x42), 0x00);
}

TEST_F(Axp2101Test, TwoEntitiesShareTheChipsInterrupts) {
  AXP2101Event keys;
  keys.set_parent(&this->axp_);
  keys.set_event_types_static(POWER_KEY_EVENTS, 2);
  keys.setup();
  ASSERT_FALSE(keys.is_failed());

  AXP2101Event vbus;
  vbus.set_parent(&this->axp_);
  vbus.set_event_types_static(VBUS_ONLY_EVENTS, 1);
  vbus.setup();
  ASSERT_FALSE(vbus.is_failed());

  // Power key short (bit 3) and long (bit 2) plus vbus insert (bit 7): both
  // entities stay enabled, so neither switched the other one off.
  EXPECT_EQ(this->bus_.read(0x41), 0x80 | 0x08 | 0x04);

  this->bus_.poke(IRQ_STATUS_1, VBUS_INSERT_BIT);
  LoopBlockingGuard guard(nullptr, nullptr, 1000);
  this->axp_.loop();

  EXPECT_TRUE(vbus.has_event());
  EXPECT_EQ(vbus.get_last_event_type(), "vbus_insert");
  EXPECT_FALSE(keys.has_event());
}

TEST_F(Axp2101Test, EventEntityIgnoresSourcesItDoesNotExpose) {
  AXP2101Event entity;
  entity.set_parent(&this->axp_);
  entity.set_event_types_static(VBUS_ONLY_EVENTS, 1);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  this->bus_.poke(IRQ_STATUS_1, POWER_KEY_SHORT_BIT);
  LoopBlockingGuard guard(nullptr, nullptr, 1000);
  this->axp_.loop();

  EXPECT_FALSE(entity.has_event());
}

TEST_F(Axp2101Test, EventEntityDropsWhatTheChipLatchedBeforeItStarted) {
  // A press from before boot is not something Home Assistant should see.
  this->bus_.poke(IRQ_STATUS_1, POWER_KEY_SHORT_BIT);

  AXP2101Event entity;
  entity.set_parent(&this->axp_);
  entity.set_event_types_static(POWER_KEY_EVENTS, 2);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  LoopBlockingGuard guard(nullptr, nullptr, 1000);
  this->axp_.loop();

  EXPECT_FALSE(entity.has_event());
}

TEST_F(Axp2101Test, EventEntityFailsOnASourceTheChipDoesNotHave) {
  static const char *const BOGUS[] = {"power_key_triple_press"};

  AXP2101Event entity;
  entity.set_parent(&this->axp_);
  entity.set_event_types_static(BOGUS, 1);
  entity.setup();

  EXPECT_TRUE(entity.is_failed());
  // A failed entity must not leave interrupts enabled on the chip.
  EXPECT_EQ(this->bus_.read(0x40), 0x00);
  EXPECT_EQ(this->bus_.read(0x41), 0x00);
  EXPECT_EQ(this->bus_.read(0x42), 0x00);
}

TEST_F(Axp2101Test, EventEntityReportsEverySourceThatFiresTogether) {
  AXP2101Event entity;
  entity.set_parent(&this->axp_);
  entity.set_event_types_static(POWER_KEY_EVENTS, 2);
  entity.setup();
  ASSERT_FALSE(entity.is_failed());

  // A press that also started charging: both bits are latched in one read.
  this->bus_.poke(IRQ_STATUS_1, POWER_KEY_SHORT_BIT);
  this->bus_.poke(0x4A, 0x08);  // charge_started
  LoopBlockingGuard guard(nullptr, nullptr, 1000);
  this->axp_.loop();

  // The entity reports the last one it saw; both were delivered in order.
  EXPECT_TRUE(entity.has_event());
}

}  // namespace esphome::axp2101::testing
