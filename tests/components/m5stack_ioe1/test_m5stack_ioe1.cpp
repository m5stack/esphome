#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "esphome/components/m5stack_ioe1/m5stack_ioe1.h"
#include "esphome/core/gpio.h"

namespace esphome::m5stack_ioe1::testing {

namespace {

constexpr uint8_t ADDRESS = 0x6F;

constexpr uint8_t REG_VERSION = 0x02;
constexpr uint8_t REG_GPIO_M_L = 0x03;
constexpr uint8_t REG_GPIO_O_L = 0x05;
constexpr uint8_t REG_GPIO_I_L = 0x07;
constexpr uint8_t REG_GPIO_PU_L = 0x09;
constexpr uint8_t REG_GPIO_PD_L = 0x0B;
constexpr uint8_t REG_GPIO_IE_L = 0x0D;
constexpr uint8_t REG_GPIO_IP_L = 0x0F;
constexpr uint8_t REG_GPIO_IS_L = 0x11;
constexpr uint8_t REG_GPIO_DRV_L = 0x13;
constexpr uint8_t REG_ADC_CTRL = 0x15;
constexpr uint8_t REG_ADC_D_L = 0x16;
constexpr uint8_t REG_LED_CFG = 0x24;
constexpr uint8_t REG_PWM_FREQ_L = 0x25;
constexpr uint8_t REG_PWM2_DUTY_L = 0x1D;
constexpr uint8_t REG_LED_RAM_START = 0x30;

}  // namespace

/// Register file of an M5Stack IOE1, plus transaction bookkeeping.
class FakeBus : public i2c::I2CBus {
 public:
  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override {
    EXPECT_EQ(address, ADDRESS);
    if (this->fail_transactions != 0) {
      if (this->fail_transactions > 0) {
        this->fail_transactions--;
      }
      return i2c::ERROR_TIMEOUT;
    }
    if (write_count == 0) {
      return i2c::ERROR_OK;
    }
    this->write_sizes.push_back(write_count);
    uint8_t reg = write_buffer[0];
    for (size_t i = 1; i < write_count; i++, reg++) {
      this->regs[reg] = write_buffer[i];
    }
    for (size_t i = 0; i < read_count; i++, reg++) {
      read_buffer[i] = this->regs[reg];
    }
    return i2c::ERROR_OK;
  }

  uint8_t regs[0x100]{};
  std::vector<size_t> write_sizes;
  // Positive: fail that many transactions; zero: never fail.
  int fail_transactions{0};
};

class M5StackIOE1Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->component_.set_i2c_bus(&this->bus_);
    this->component_.set_i2c_address(ADDRESS);
    this->bus_.regs[REG_VERSION] = 0x01;
  }

  uint16_t register16_(uint8_t reg) const {
    return static_cast<uint16_t>(this->bus_.regs[reg]) | (static_cast<uint16_t>(this->bus_.regs[reg + 1]) << 8);
  }

  FakeBus bus_;
  M5StackIOE1Component component_;
  InternalGPIOPin int_pin_;
};

TEST_F(M5StackIOE1Test, PinModeOutputConfiguresPushPullAndOpenDrain) {
  this->component_.pin_mode(3, gpio::FLAG_OUTPUT);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_M_L], 0x08);
  // Outputs are push-pull unless open-drain is requested; other pins keep the chip default
  EXPECT_EQ(register16_(REG_GPIO_DRV_L) & 0x0008, 0x0000);
  EXPECT_EQ(register16_(REG_GPIO_DRV_L) & 0x0004, 0x0004);

  this->component_.pin_mode(2, gpio::FLAG_OUTPUT | gpio::FLAG_OPEN_DRAIN);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_M_L], 0x0C);
  EXPECT_EQ(register16_(REG_GPIO_DRV_L) & 0x0008, 0x0000);
  EXPECT_EQ(register16_(REG_GPIO_DRV_L) & 0x0004, 0x0004);
}

TEST_F(M5StackIOE1Test, PinModeInputConfiguresPullUpAndPullDown) {
  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_M_L], 0x00);
  EXPECT_EQ(register16_(REG_GPIO_PU_L), 0x0001);
  EXPECT_EQ(register16_(REG_GPIO_PD_L), 0x0000);

  this->component_.pin_mode(1, gpio::FLAG_INPUT | gpio::FLAG_PULLDOWN);

  // Pin 0 keeps its pull-up, pin 1 gets a pull-down
  EXPECT_EQ(register16_(REG_GPIO_PU_L), 0x0001);
  EXPECT_EQ(register16_(REG_GPIO_PD_L), 0x0002);
}

TEST_F(M5StackIOE1Test, DigitalWriteSetsOnlyTheRequestedPin) {
  this->component_.digital_write(3, true);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_O_L], 0x08);

  this->component_.digital_write(3, false);

  EXPECT_EQ(this->bus_.regs[REG_GPIO_O_L], 0x00);
}

TEST_F(M5StackIOE1Test, DigitalReadUsesBothCacheBanks) {
  this->bus_.regs[REG_GPIO_I_L] = 0x00;
  this->bus_.regs[REG_GPIO_I_L + 1] = 0x20;  // pin 13

  EXPECT_FALSE(this->component_.digital_read(0));
  EXPECT_TRUE(this->component_.digital_read(13));
}

TEST_F(M5StackIOE1Test, PwmDutyKeepsEnableAndPolarityBits) {
  this->component_.setup_pwm_channel(PWM_2, ACTIVE_LOW);

  EXPECT_EQ(register16_(REG_PWM2_DUTY_L), 0xC000);

  this->component_.set_pwm_value(PWM_2, 0.5f);

  const uint16_t value = register16_(REG_PWM2_DUTY_L);
  EXPECT_EQ(value & 0x0FFF, 2048);
  EXPECT_EQ(value & 0xC000, 0xC000);
}

TEST_F(M5StackIOE1Test, AdcValueStartsConversionAndReadsResult) {
  this->bus_.regs[REG_ADC_D_L] = 0x34;
  this->bus_.regs[REG_ADC_D_L + 1] = 0x02;

  EXPECT_FLOAT_EQ(this->component_.get_adc_value(ADC_1), 0x234);
  EXPECT_EQ(this->bus_.regs[REG_ADC_CTRL] & 0x07, 0x01);
  EXPECT_EQ(this->bus_.regs[REG_ADC_CTRL] & 0x40, 0x40);
}

TEST_F(M5StackIOE1Test, LedRamIsWrittenInSmallChunks) {
  std::array<uint8_t, 64> buffer{};
  buffer.fill(0xAB);

  this->component_.set_led_color(buffer.data(), buffer.size());

  // Four chunks of 16 data bytes (plus the register address), then the refresh read and write
  ASSERT_EQ(this->bus_.write_sizes.size(), 6u);
  for (size_t i = 0; i < 4; i++) {
    EXPECT_EQ(this->bus_.write_sizes[i], 17u);
  }
  EXPECT_EQ(this->bus_.regs[REG_LED_RAM_START], 0xAB);
  EXPECT_EQ(this->bus_.regs[REG_LED_RAM_START + 63], 0xAB);
  EXPECT_EQ(this->bus_.regs[REG_LED_CFG] & 0x40, 0x40);
}

TEST_F(M5StackIOE1Test, LoopClearsLatchedInterruptStatus) {
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->int_pin_.set_inverted(true);  // INT released
  this->bus_.regs[REG_GPIO_IS_L] = 0x10;
  this->bus_.regs[REG_GPIO_IS_L + 1] = 0x00;

  this->component_.loop();

  EXPECT_EQ(register16_(REG_GPIO_IS_L), 0x0000);
}

TEST_F(M5StackIOE1Test, SetupWritesInterruptConfiguration) {
  this->component_.set_reset(false);
  this->component_.set_pin_interrupt(4, true, RISING_EDGE);
  this->component_.set_pin_interrupt(6, false, FALLING_EDGE);
  this->component_.set_interrupt_pin(&this->int_pin_);
  this->int_pin_.set_inverted(true);
  // An interrupt enabled for a pin that is not configured must survive
  this->bus_.regs[REG_GPIO_IE_L] = 0x80;

  this->component_.setup();

  EXPECT_FALSE(this->component_.is_failed());
  EXPECT_EQ(register16_(REG_GPIO_IE_L), 0x0090);
  EXPECT_EQ(register16_(REG_GPIO_IP_L), 0x0010);
  EXPECT_EQ(register16_(REG_PWM_FREQ_L), 500);
}

TEST_F(M5StackIOE1Test, SetupRejectsInvalidFirmwareVersion) {
  this->component_.set_reset(false);
  this->bus_.regs[REG_VERSION] = 0x00;

  this->component_.setup();

  EXPECT_TRUE(this->component_.is_failed());
  EXPECT_EQ(this->bus_.regs[REG_PWM_FREQ_L], 0x00);
}

TEST_F(M5StackIOE1Test, FailedComponentIgnoresWritesAndPinModes) {
  this->component_.mark_failed();

  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  this->component_.digital_write(1, true);

  EXPECT_EQ(register16_(REG_GPIO_M_L), 0x0000);
  EXPECT_EQ(register16_(REG_GPIO_PU_L), 0x0000);
  EXPECT_EQ(register16_(REG_GPIO_O_L), 0x0000);
}

TEST_F(M5StackIOE1Test, WarningClearsWhenTheBusRecovers) {
  this->bus_.fail_transactions = 1;

  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);
  EXPECT_TRUE(this->component_.status_has_warning());

  this->component_.pin_mode(0, gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

  EXPECT_FALSE(this->component_.status_has_warning());
  EXPECT_EQ(register16_(REG_GPIO_PU_L), 0x0001);
}

}  // namespace esphome::m5stack_ioe1::testing
