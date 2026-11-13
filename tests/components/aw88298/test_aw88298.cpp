#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <utility>
#include <vector>

#include "esphome/components/aw88298/aw88298.h"

namespace esphome::aw88298::testing {

namespace {

constexpr uint8_t ADDRESS = 0x36;

/// Register file that keeps one 16 bit word per register address and moves it over the bus as the
/// big endian byte pairs I2CDevice uses.
class RegisterBus final : public i2c::I2CBus {
 public:
  uint16_t reg16(uint8_t reg) const { return this->words[reg]; }
  void set_reg16(uint8_t reg, uint16_t value) { this->words[reg] = value; }
  /// Register and value of every write, in order.
  std::vector<std::pair<uint8_t, uint16_t>> writes;
  /// Number of register reads, so a test can tell the first bus access apart.
  size_t reads{0};

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *out, size_t out_size, uint8_t *in,
                             size_t in_size) override {
    EXPECT_EQ(address, ADDRESS);
    if (out_size == 0)
      return i2c::ERROR_TIMEOUT;
    const uint8_t reg = out[0];
    if (in_size > 0) {
      this->reads++;
      const uint16_t value = this->words[reg];
      for (size_t i = 0; i < in_size; i++)
        in[i] = (i % 2 == 0) ? static_cast<uint8_t>(value >> 8) : static_cast<uint8_t>(value & 0xFF);
    } else {
      uint16_t value = 0;
      if (out_size >= 3)
        value = static_cast<uint16_t>(out[1] << 8 | out[2]);
      else if (out_size == 2)
        value = out[1];
      this->words[reg] = value;
      this->writes.emplace_back(reg, value);
    }
    return i2c::ERROR_OK;
  }

  uint16_t words[256]{};
};

/// Stands in for a reset line, on the ESP32 or on an IO expander: both are just a GPIOPin.
class MockResetPin final : public GPIOPin {
 public:
  void setup() override { this->setup_called = true; }
  void pin_mode(gpio::Flags flags) override { this->flags = flags; }
  gpio::Flags get_flags() const override { return this->flags; }
  bool digital_read() override { return this->level; }
  void digital_write(bool value) override {
    this->level = value;
    this->levels.push_back(value);
    if (value && this->on_release)
      this->on_release();
  }
  size_t dump_summary(char *buffer, size_t len) const override { return snprintf(buffer, len, "mock reset"); }

  bool setup_called{false};
  gpio::Flags flags{gpio::FLAG_NONE};
  bool level{false};
  std::vector<bool> levels;
  std::function<void()> on_release;
};

}  // namespace

TEST(Aw88298Registers, SampleRateCodeCoversEveryDocumentedRate) {
  const uint32_t rates[] = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
  for (uint8_t index = 0; index < 10; index++) {
    uint8_t code = 0xFF;
    EXPECT_TRUE(sample_rate_code(rates[index], &code)) << rates[index];
    EXPECT_EQ(code, index) << rates[index];
  }
  uint8_t code = 0xAA;
  EXPECT_FALSE(sample_rate_code(44101, &code));
  EXPECT_FALSE(sample_rate_code(192000, &code));
  EXPECT_EQ(code, 0xAA);  // Left alone when the rate is refused.
}

TEST(Aw88298Registers, DataWidthAndBitClockCodes) {
  EXPECT_EQ(data_width_code(16), 0);
  EXPECT_EQ(data_width_code(20), 1);
  EXPECT_EQ(data_width_code(24), 2);
  EXPECT_EQ(data_width_code(32), 3);
  EXPECT_EQ(data_width_code(8), 0xFF);

  uint8_t code = 0xFF;
  // BCK = sample rate * slot length * slot count, so two slots double the slot length.
  EXPECT_TRUE(bck_code(16, 2, &code));
  EXPECT_EQ(code, 0);
  EXPECT_TRUE(bck_code(24, 2, &code));
  EXPECT_EQ(code, 1);
  EXPECT_TRUE(bck_code(32, 2, &code));
  EXPECT_EQ(code, 2);
  EXPECT_TRUE(bck_code(16, 4, &code));
  EXPECT_EQ(code, 2);
  // 96 fs and 128 fs have no encoding.
  EXPECT_FALSE(bck_code(24, 4, &code));
  EXPECT_FALSE(bck_code(32, 4, &code));
}

TEST(Aw88298Registers, I2SControlWord) {
  I2SSettings settings{};
  settings.sample_rate_code = 3;  // 16 kHz
  settings.data_bits = 16;
  settings.slot_bits = 16;
  settings.slot_count = 2;
  settings.format = I2SFormat::I2S_FORMAT_STANDARD;
  settings.channel = ChannelSelect::CHANNEL_SELECT_LEFT;
  uint16_t value = 0;
  ASSERT_TRUE(encode_i2sctrl(settings, &value));
  // I2SRXEN, CHSEL left, standard format, 16 bit data, 32 fs, 16 kHz.
  EXPECT_EQ(value, 0x1403);

  settings.sample_rate_code = 8;  // 48 kHz
  settings.data_bits = 24;
  settings.slot_bits = 24;
  settings.channel = ChannelSelect::CHANNEL_SELECT_RIGHT;
  settings.input_attenuation = true;
  ASSERT_TRUE(encode_i2sctrl(settings, &value));
  // INPLEV, I2SRXEN, CHSEL right, standard format, 24 bit data, 48 fs, 48 kHz.
  EXPECT_EQ(value, 0x3898);

  settings.data_bits = 20;  // 20 bit samples travel in 24 bit slots, so the frame still fits.
  ASSERT_TRUE(encode_i2sctrl(settings, &value));
  EXPECT_EQ(value, 0x3858);

  settings.slot_bits = 24;
  settings.slot_count = 4;  // 96 fs cannot be described.
  EXPECT_FALSE(encode_i2sctrl(settings, &value));
}

TEST(Aw88298Registers, I2SConfigWord) {
  I2SSettings settings{};
  settings.slot_count = 2;
  settings.rx_slot_mask = 0x03;
  // Slots 0 and 1 received, unused channel tri-stated, two slot frame, transmitter off: the
  // register default.
  EXPECT_EQ(encode_i2scfg1(settings), 0x0310);

  settings.slot_count = 4;
  settings.rx_slot_mask = 0x05;  // slots 0 and 2
  settings.ws_pulse_one_bck = true;
  EXPECT_EQ(encode_i2scfg1(settings), 0x051C);
}

TEST(Aw88298Registers, VolumeEncodingSatensAtBothEnds) {
  EXPECT_EQ(volume_reg_from_db(0.0f), 0x00);
  EXPECT_EQ(volume_reg_from_db(-0.5f), 0x01);
  EXPECT_EQ(volume_reg_from_db(-5.5f), 0x0B);
  EXPECT_EQ(volume_reg_from_db(-6.0f), 0x10);
  EXPECT_EQ(volume_reg_from_db(-6.5f), 0x11);
  EXPECT_EQ(volume_reg_from_db(-90.0f), 0xF0);
  EXPECT_EQ(volume_reg_from_db(-95.5f), 0xFB);
  // The last half dB step has no code of its own, so -96 dB is full attenuation.
  EXPECT_EQ(volume_reg_from_db(-96.0f), 0xFF);
  EXPECT_EQ(volume_reg_from_db(-100.0f), 0xFF);
  // Anything above 0 dB is not an attenuation at all.
  EXPECT_EQ(volume_reg_from_db(3.0f), 0x00);

  for (float db = -90.0f; db <= 0.0f; db += 0.5f) {
    EXPECT_NEAR(volume_db_from_reg(volume_reg_from_db(db)), db, 0.01f) << db;
  }
}

TEST(Aw88298Registers, BoostVoltageAndCurrentLimit) {
  uint8_t value = 0;
  ASSERT_TRUE(boost_voltage_reg(5.0f, &value));
  EXPECT_EQ(value, 0x0F);
  ASSERT_TRUE(boost_voltage_reg(9.5f, &value));
  EXPECT_EQ(value, 0x33);
  ASSERT_TRUE(boost_voltage_reg(10.25f, &value));
  EXPECT_EQ(value, 0x39);
  EXPECT_FALSE(boost_voltage_reg(4.875f, &value));
  EXPECT_FALSE(boost_voltage_reg(10.375f, &value));
  EXPECT_NEAR(boost_voltage_from_reg(0x33), 9.5f, 0.001f);
  EXPECT_NEAR(boost_voltage_from_reg(0x39), 10.25f, 0.001f);

  ASSERT_TRUE(boost_current_limit_reg(1.5f, &value));
  EXPECT_EQ(value, 0);
  ASSERT_TRUE(boost_current_limit_reg(3.5f, &value));
  EXPECT_EQ(value, 8);
  ASSERT_TRUE(boost_current_limit_reg(4.25f, &value));
  EXPECT_EQ(value, 11);
  EXPECT_FALSE(boost_current_limit_reg(1.25f, &value));
  EXPECT_FALSE(boost_current_limit_reg(4.5f, &value));
  EXPECT_NEAR(boost_current_limit_from_reg(8), 3.5f, 0.001f);
}

TEST(Aw88298Registers, ThresholdsAndTimes) {
  EXPECT_EQ(agc_threshold_reg(64.0f), 0x40);
  EXPECT_EQ(agc_threshold_reg(57.0f), 0x39);
  EXPECT_EQ(agc_threshold_reg(0.0f), 1);  // Zero is reserved.
  EXPECT_EQ(agc_threshold_reg(140.0f), 100);

  EXPECT_EQ(boost_threshold_reg(0.0f), 1);  // Zero is reserved.
  EXPECT_EQ(boost_threshold_reg(100.0f), 63);
  EXPECT_EQ(boost_threshold_reg(50.0f), 32);
  EXPECT_NEAR(boost_threshold_percent(32), 50.0f, 0.001f);

  // The datasheet defaults: 1 ms attack, 10 ms release, 133 ms hold.
  EXPECT_EQ(agc_time_reg(1.0f, AGC_TIME_UNIT_MS), 0x0030);
  EXPECT_EQ(agc_time_reg(10.0f, AGC_TIME_UNIT_MS), 0x01E1);
  EXPECT_EQ(agc_time_reg(133.0f, AGC_HOLD_TIME_UNIT_MS), 100);
  EXPECT_NEAR(agc_time_ms(0x01E0, AGC_TIME_UNIT_MS), 9.984f, 0.001f);
}

TEST(Aw88298Registers, Monitors) {
  // 0x263 is the datasheet's example code for 3.6 V on VDD and 7.2 V on PVDD.
  EXPECT_NEAR(vdd_from_reg(0x263), 3.6f, 0.01f);
  EXPECT_NEAR(pvdd_from_reg(0x263), 7.2f, 0.01f);
  EXPECT_NEAR(vdd_from_reg(0x263 | 0xFC00), 3.6f, 0.01f);  // The upper bits are reserved.
  EXPECT_NEAR(temperature_from_reg(0x019), 25.0f, 0.001f);
  EXPECT_NEAR(temperature_from_reg(0x3D8), -40.0f, 0.001f);
  EXPECT_NEAR(temperature_from_reg(0x000), 0.0f, 0.001f);
  EXPECT_NEAR(boost_status_voltage_from_reg(0x0F), 5.0f, 0.001f);
  EXPECT_NEAR(boost_status_voltage_from_reg(0x33), 9.5f, 0.001f);
}

TEST(Aw88298Registers, StatusBits) {
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_BOOST_OVER_VOLTAGE_2), 1 << 15);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_UNDER_VOLTAGE), 1 << 14);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_BOOST_ADAPTIVE), 1 << 13);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_BOOST_OVER_CURRENT), 1 << 11);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_BOOST_OVER_VOLTAGE), 1 << 10);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_BOOST_STARTED), 1 << 9);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_AMPLIFIER_SWITCHING), 1 << 8);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_CLIPPING), 1 << 7);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_NO_CLOCK), 1 << 5);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_CLOCK_STABLE), 1 << 4);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_OVER_CURRENT), 1 << 3);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_PRE_CLIPPING), 1 << 2);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_OVER_TEMPERATURE), 1 << 1);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_PLL_LOCKED), 1 << 0);
  EXPECT_EQ(status_bit(StatusFlag::STATUS_FLAG_FAULT), 0);
  // Every fault bit is covered by the aggregate.
  EXPECT_EQ(SYSST_FAULT_MASK & status_bit(StatusFlag::STATUS_FLAG_OVER_CURRENT), SYSST_OVER_CURRENT);
  EXPECT_EQ(SYSST_FAULT_MASK & status_bit(StatusFlag::STATUS_FLAG_CLIPPING), 0);
}

class Aw88298Test : public ::testing::Test {
 protected:
  void SetUp() override {
    this->amp_.set_i2c_bus(&this->bus_);
    this->amp_.set_i2c_address(ADDRESS);
    this->bus_.set_reg16(REG_ID, IDCODE_AW88298);
  }

  /// Configure the amplifier the way code generation does for a 16 kHz, 16 bit, stereo frame.
  void configure_() {
    this->amp_.set_sample_rate(16000);
    this->amp_.set_data_bits(16);
    this->amp_.set_slot_bits(16);
    this->amp_.set_slot_count(2);
    this->amp_.set_rx_slot_mask(0x03);
    this->amp_.set_i2s_format(I2SFormat::I2S_FORMAT_STANDARD);
    this->amp_.set_i2s_channel(ChannelSelect::CHANNEL_SELECT_LEFT);
    this->amp_.set_volume_min_db(-96.0f);
    this->amp_.set_volume_max_db(0.0f);
    this->amp_.set_initial_volume(1.0f);
    // Everything the code generation sets, with the configuration defaults of common.yaml.
    this->amp_.set_setting(Setting::SETTING_BOOST_MODE, 3.0f);
    this->amp_.set_setting(Setting::SETTING_BOOST_VOLTAGE, 9.5f);
    this->amp_.set_setting(Setting::SETTING_BOOST_CURRENT_LIMIT, 3.5f);
    this->amp_.set_setting(Setting::SETTING_BOOST_DEGLITCH, 3.0f);
    this->amp_.set_setting(Setting::SETTING_BOOST_ATTACK_THRESHOLD, 30.0f);
    this->amp_.set_setting(Setting::SETTING_BOOST_RELEASE_THRESHOLD, 15.0f);
    this->amp_.set_setting(Setting::SETTING_HDCC, 1.0f);
    this->amp_.set_setting(Setting::SETTING_AGC, 1.0f);
    this->amp_.set_setting(Setting::SETTING_AGC_ATTACK_THRESHOLD, 64.0f);
    this->amp_.set_setting(Setting::SETTING_AGC_RELEASE_THRESHOLD, 57.0f);
    this->amp_.set_setting(Setting::SETTING_AGC_ATTACK_TIME, 1.0f);
    this->amp_.set_setting(Setting::SETTING_AGC_RELEASE_TIME, 10.0f);
    this->amp_.set_setting(Setting::SETTING_AGC_HOLD_TIME, 133.0f);
  }

  RegisterBus bus_;
  AW88298 amp_;
};

TEST_F(Aw88298Test, ResetPinIsPulsedBeforeTheChipIsAddressed) {
  MockResetPin reset_pin;
  this->amp_.set_reset_pin(&reset_pin);
  // The device must be out of reset before its ID is read, so nothing may reach it while RSTN is
  // still held low.
  reset_pin.on_release = [this]() { EXPECT_EQ(this->bus_.reads, 0u); };

  this->configure_();
  this->amp_.setup();
  ASSERT_FALSE(this->amp_.is_failed());

  EXPECT_TRUE(reset_pin.setup_called);
  ASSERT_EQ(reset_pin.levels.size(), 2u);
  EXPECT_FALSE(reset_pin.levels[0]);  // RSTN low
  EXPECT_TRUE(reset_pin.levels[1]);   // released
  // And the amplifier was configured once it was out of reset.
  EXPECT_EQ(this->bus_.reg16(REG_SYSCTRL), 0x4040);
}

TEST_F(Aw88298Test, SetupConfiguresTheFrameAndStartsTheAmplifier) {
  this->configure_();
  this->amp_.setup();
  ASSERT_FALSE(this->amp_.is_failed());

  // Reset, then the frame the speaker will send.
  EXPECT_EQ(this->bus_.reg16(REG_I2SCTRL), 0x1403);
  EXPECT_EQ(this->bus_.reg16(REG_I2SCFG1), 0x0310);
  // Datasheet AGC defaults and the boost settings the code generation defaults to.
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG1), 0x3940);
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG2), 0x0030);
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG3), 0x01E1);
  EXPECT_EQ(this->bus_.reg16(REG_BSTCTRL1), 0x0A13);
  EXPECT_EQ(this->bus_.reg16(REG_BSTCTRL2), 0x6333);
  // HDCC and AGC on, 3.5 A limit, unmuted once the amplifier is running.
  EXPECT_EQ(this->bus_.reg16(REG_SYSCTRL2), 0x0068);
  // I2S receive enabled, system and amplifier powered, reserved bit kept.
  EXPECT_EQ(this->bus_.reg16(REG_SYSCTRL), 0x4040);
  // Maximum volume and the default hold time share HAGCCFG4.
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG4), 0x0064);

  // The amplifier is configured before it is enabled, and unmuted only after that.
  size_t mute_write = this->bus_.writes.size();
  size_t enable_write = this->bus_.writes.size();
  size_t unmute_write = this->bus_.writes.size();
  for (size_t i = 0; i < this->bus_.writes.size(); i++) {
    const auto &[reg, value] = this->bus_.writes[i];
    if (reg == REG_SYSCTRL2 && (value & SYSCTRL2_HMUTE) != 0)
      mute_write = std::min(mute_write, i);
    else if (reg == REG_SYSCTRL && (value & SYSCTRL_AMPPD) == 0)
      enable_write = std::min(enable_write, i);
    else if (reg == REG_SYSCTRL2 && (value & SYSCTRL2_HMUTE) == 0)
      unmute_write = std::min(unmute_write, i);
  }
  EXPECT_LT(mute_write, enable_write);
  EXPECT_LT(enable_write, unmute_write);
}

TEST_F(Aw88298Test, MuteDrivesTheHardMuteBit) {
  this->configure_();
  this->amp_.setup();
  ASSERT_FALSE(this->amp_.is_failed());

  ASSERT_TRUE(this->amp_.set_mute_on());
  EXPECT_TRUE(this->amp_.is_muted());
  EXPECT_NE(this->bus_.reg16(REG_SYSCTRL2) & SYSCTRL2_HMUTE, 0);
  // Muting must not touch the DC cancelling bit.
  EXPECT_NE(this->bus_.reg16(REG_SYSCTRL2) & SYSCTRL2_HDCCE, 0);

  ASSERT_TRUE(this->amp_.set_mute_off());
  EXPECT_FALSE(this->amp_.is_muted());
  EXPECT_EQ(this->bus_.reg16(REG_SYSCTRL2) & SYSCTRL2_HMUTE, 0);
}

TEST_F(Aw88298Test, VolumeSpansTheConfiguredRange) {
  this->configure_();
  this->amp_.setup();
  ASSERT_FALSE(this->amp_.is_failed());

  ASSERT_TRUE(this->amp_.set_volume(1.0f));
  EXPECT_NEAR(this->amp_.volume(), 1.0f, 0.001f);
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG4) >> 8, 0x00);  // 0 dB

  ASSERT_TRUE(this->amp_.set_volume(0.5f));
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG4) >> 8, 0x80);  // -48 dB
  EXPECT_NEAR(volume_db_from_reg(0x80), -48.0f, 0.001f);

  // The lowest volume saturates at the largest attenuation instead of wrapping to 0 dB.
  ASSERT_TRUE(this->amp_.set_volume(0.0f));
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG4) >> 8, 0xFF);
  // The hold time is untouched by a volume change.
  EXPECT_EQ(this->bus_.reg16(REG_HAGCCFG4) & 0x00FF, 100);
}

TEST_F(Aw88298Test, SettingsAreWrittenWhenChanged) {
  this->configure_();
  this->amp_.setup();
  ASSERT_FALSE(this->amp_.is_failed());

  ASSERT_TRUE(this->amp_.set_setting(Setting::SETTING_BOOST_VOLTAGE, 10.25f));
  EXPECT_EQ(this->bus_.reg16(REG_BSTCTRL2) & BSTCTRL2_VOUT_VREFSET_MASK, 0x39);
  EXPECT_NEAR(this->amp_.get_setting(Setting::SETTING_BOOST_VOLTAGE), 10.25f, 0.001f);
  // Changing the voltage keeps the mode and the deglitch time.
  EXPECT_EQ(this->bus_.reg16(REG_BSTCTRL2) & BSTCTRL2_BST_MODE_MASK, 0x6000);
  EXPECT_EQ(this->bus_.reg16(REG_BSTCTRL2) & BSTCTRL2_BST_TDEG_MASK, 0x0300);

  // A value between two codes is stored as the code the device really holds.
  ASSERT_TRUE(this->amp_.set_setting(Setting::SETTING_BOOST_VOLTAGE, 9.86f));
  EXPECT_NEAR(this->amp_.get_setting(Setting::SETTING_BOOST_VOLTAGE), 9.875f, 0.001f);

  ASSERT_TRUE(this->amp_.set_setting(Setting::SETTING_HDCC, 0.0f));
  EXPECT_EQ(this->bus_.reg16(REG_SYSCTRL2) & SYSCTRL2_HDCCE, 0);

  ASSERT_TRUE(this->amp_.set_setting(Setting::SETTING_RECEIVER_MODE, 1.0f));
  EXPECT_NE(this->bus_.reg16(REG_SYSCTRL) & SYSCTRL_RECEIVER_MODE, 0);
  ASSERT_TRUE(this->amp_.set_setting(Setting::SETTING_RECEIVER_MODE, 0.0f));
  EXPECT_EQ(this->bus_.reg16(REG_SYSCTRL) & SYSCTRL_RECEIVER_MODE, 0);
}

TEST_F(Aw88298Test, UnknownChipIdFails) {
  this->bus_.set_reg16(REG_ID, 0x1234);
  this->configure_();
  this->amp_.setup();
  EXPECT_TRUE(this->amp_.is_failed());
}

}  // namespace esphome::aw88298::testing
