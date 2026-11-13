#pragma once

#include <cstdint>

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "aw88298_regs.h"

#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::aw88298 {

// Register map, datasheet V1.6 section "Register List".
static constexpr uint8_t REG_ID = 0x00;        // IDCODE, writing 0x55AA resets the device
static constexpr uint8_t REG_SYSST = 0x01;     // status, read only
static constexpr uint8_t REG_SYSCTRL = 0x04;   // enable, receiver mode, clock polarity
static constexpr uint8_t REG_SYSCTRL2 = 0x05;  // mute, HDCC, AGC, boost current limit
static constexpr uint8_t REG_I2SCTRL = 0x06;   // format, data width, slot length, sample rate
static constexpr uint8_t REG_I2SCFG1 = 0x07;   // active slots, slot count
static constexpr uint8_t REG_HAGCCFG1 = 0x09;  // AGC attack and release thresholds
static constexpr uint8_t REG_HAGCCFG2 = 0x0A;  // AGC attack time
static constexpr uint8_t REG_HAGCCFG3 = 0x0B;  // AGC release time
static constexpr uint8_t REG_HAGCCFG4 = 0x0C;  // volume and AGC hold time
static constexpr uint8_t REG_HAGCST = 0x10;    // boost voltage in use
static constexpr uint8_t REG_VDD = 0x12;       // supply monitor
static constexpr uint8_t REG_TEMP = 0x13;      // die temperature
static constexpr uint8_t REG_PVDD = 0x14;      // amplifier supply monitor
static constexpr uint8_t REG_BSTCTRL1 = 0x60;  // smart boost thresholds
static constexpr uint8_t REG_BSTCTRL2 = 0x61;  // boost mode, deglitch, output voltage

static constexpr uint16_t IDCODE_AW88298 = 0x1852;
static constexpr uint16_t RESET_CODE = 0x55AA;

// SYSCTRL (0x04)
static constexpr uint16_t SYSCTRL_PWDN = 1 << 0;
static constexpr uint16_t SYSCTRL_AMPPD = 1 << 1;
static constexpr uint16_t SYSCTRL_PLL_FROM_WS = 1 << 3;
static constexpr uint16_t SYSCTRL_BCLK_INVERT = 1 << 4;
static constexpr uint16_t SYSCTRL_WS_INVERT = 1 << 5;
static constexpr uint16_t SYSCTRL_I2S_ENABLE = 1 << 6;
static constexpr uint16_t SYSCTRL_RECEIVER_MODE = 1 << 7;
/// Bit 14 is a reserved bit that reads back set, so it has to be written back unchanged.
static constexpr uint16_t SYSCTRL_RESERVED = 0x4000;

// SYSCTRL2 (0x05)
static constexpr uint16_t SYSCTRL2_BST_IPEAK_MASK = 0x000F;
static constexpr uint16_t SYSCTRL2_HMUTE = 1 << 4;
static constexpr uint16_t SYSCTRL2_HDCCE = 1 << 5;
static constexpr uint16_t SYSCTRL2_HAGCE = 1 << 6;
static constexpr uint16_t SYSCTRL2_RMSE = 1 << 7;

// I2SCTRL (0x06)
static constexpr uint16_t I2SCTRL_INPLEV = 1 << 13;

// BSTCTRL1 (0x60)
static constexpr uint16_t BSTCTRL1_BST_ATH_MASK = 0x003F;
static constexpr uint16_t BSTCTRL1_BST_RTH_MASK = 0x3F00;

// BSTCTRL2 (0x61)
static constexpr uint16_t BSTCTRL2_VOUT_VREFSET_MASK = 0x003F;
static constexpr uint16_t BSTCTRL2_BST_TDEG_MASK = 0x0700;
static constexpr uint16_t BSTCTRL2_BST_MODE_MASK = 0x7000;

// SYSST (0x01)
static constexpr uint16_t SYSST_BOOST_OVP2 = 1 << 15;
static constexpr uint16_t SYSST_UNDER_VOLTAGE = 1 << 14;
static constexpr uint16_t SYSST_BOOST_OVER_CURRENT = 1 << 11;
static constexpr uint16_t SYSST_BOOST_OVP = 1 << 10;
static constexpr uint16_t SYSST_AMPLIFIER_SWITCHING = 1 << 8;
static constexpr uint16_t SYSST_OVER_CURRENT = 1 << 3;
static constexpr uint16_t SYSST_OVER_TEMPERATURE = 1 << 1;
/// Faults that cut the output back; the remaining status bits are informational.
static constexpr uint16_t SYSST_FAULT_MASK = SYSST_BOOST_OVP2 | SYSST_UNDER_VOLTAGE | SYSST_BOOST_OVP |
                                             SYSST_BOOST_OVER_CURRENT | SYSST_OVER_CURRENT | SYSST_OVER_TEMPERATURE;

/// One writable setting. Entity platforms and code generation address a setting by this value, and
/// the hub keeps the last value of each so a value can be read back without a bus round trip.
enum class Setting : uint8_t {
  SETTING_HDCC,                     // SYSCTRL2.HDCCE, DC blocking of the input
  SETTING_AGC,                      // SYSCTRL2.HAGCE, hardware AGC
  SETTING_AGC_RMS,                  // SYSCTRL2.RMSE, RMS instead of peak detection
  SETTING_INPUT_ATTENUATION,        // I2SCTRL.INPLEV, -6 dB input attenuation
  SETTING_RECEIVER_MODE,            // SYSCTRL.RCV_MODE, class-D fed from VDD without boost
  SETTING_BOOST_MODE,               // BSTCTRL2.BST_MODE
  SETTING_BOOST_DEGLITCH,           // BSTCTRL2.BST_TDEG
  SETTING_BOOST_VOLTAGE,            // BSTCTRL2.VOUT_VREFSET, in volts
  SETTING_BOOST_CURRENT_LIMIT,      // SYSCTRL2.BST_IPEAK, in amperes
  SETTING_BOOST_ATTACK_THRESHOLD,   // BSTCTRL1.BST_ATH, percent of full scale
  SETTING_BOOST_RELEASE_THRESHOLD,  // BSTCTRL1.BST_RTH, percent of full scale
  SETTING_AGC_ATTACK_THRESHOLD,     // HAGCCFG1.AVTH, percent of full scale
  SETTING_AGC_RELEASE_THRESHOLD,    // HAGCCFG1.RVTH, percent of full scale
  SETTING_AGC_ATTACK_TIME,          // HAGCCFG2.ATTH, in milliseconds
  SETTING_AGC_RELEASE_TIME,         // HAGCCFG3.RTTH, in milliseconds
  SETTING_AGC_HOLD_TIME,            // HAGCCFG4.HOLDTH, in milliseconds
  SETTING_COUNT,
};

/// A measurement the hub polls and hands to every sensor that asks for it.
enum class SensorType : uint8_t {
  SENSOR_TYPE_DIE_TEMPERATURE,    // TEMP
  SENSOR_TYPE_SUPPLY_VOLTAGE,     // VDD
  SENSOR_TYPE_AMPLIFIER_VOLTAGE,  // PVDD
  SENSOR_TYPE_BOOST_VOLTAGE,      // HAGCST
  SENSOR_TYPE_COUNT,
};

/// One bit of SYSST plus the combined fault state.
enum class StatusFlag : uint8_t {
  STATUS_FLAG_BOOST_OVER_VOLTAGE_2,
  STATUS_FLAG_UNDER_VOLTAGE,
  STATUS_FLAG_BOOST_ADAPTIVE,
  STATUS_FLAG_BOOST_OVER_CURRENT,
  STATUS_FLAG_BOOST_OVER_VOLTAGE,
  STATUS_FLAG_BOOST_STARTED,
  STATUS_FLAG_AMPLIFIER_SWITCHING,
  STATUS_FLAG_CLIPPING,
  STATUS_FLAG_NO_CLOCK,
  STATUS_FLAG_CLOCK_STABLE,
  STATUS_FLAG_OVER_CURRENT,
  STATUS_FLAG_PRE_CLIPPING,
  STATUS_FLAG_OVER_TEMPERATURE,
  STATUS_FLAG_PLL_LOCKED,
  STATUS_FLAG_FAULT,  // any of the bits in SYSST_FAULT_MASK
  STATUS_FLAG_COUNT,
};

/// The SYSST bit a status flag stands for. STATUS_FLAG_FAULT has no bit of its own.
inline constexpr uint16_t status_bit(StatusFlag flag) {
  switch (flag) {
    case StatusFlag::STATUS_FLAG_BOOST_OVER_VOLTAGE_2:
      return SYSST_BOOST_OVP2;
    case StatusFlag::STATUS_FLAG_UNDER_VOLTAGE:
      return SYSST_UNDER_VOLTAGE;
    case StatusFlag::STATUS_FLAG_BOOST_ADAPTIVE:
      return 1 << 13;
    case StatusFlag::STATUS_FLAG_BOOST_OVER_CURRENT:
      return SYSST_BOOST_OVER_CURRENT;
    case StatusFlag::STATUS_FLAG_BOOST_OVER_VOLTAGE:
      return SYSST_BOOST_OVP;
    case StatusFlag::STATUS_FLAG_BOOST_STARTED:
      return 1 << 9;
    case StatusFlag::STATUS_FLAG_AMPLIFIER_SWITCHING:
      return SYSST_AMPLIFIER_SWITCHING;
    case StatusFlag::STATUS_FLAG_CLIPPING:
      return 1 << 7;
    case StatusFlag::STATUS_FLAG_NO_CLOCK:
      return 1 << 5;
    case StatusFlag::STATUS_FLAG_CLOCK_STABLE:
      return 1 << 4;
    case StatusFlag::STATUS_FLAG_OVER_CURRENT:
      return SYSST_OVER_CURRENT;
    case StatusFlag::STATUS_FLAG_PRE_CLIPPING:
      return 1 << 2;
    case StatusFlag::STATUS_FLAG_OVER_TEMPERATURE:
      return SYSST_OVER_TEMPERATURE;
    case StatusFlag::STATUS_FLAG_PLL_LOCKED:
      return 1 << 0;
    default:
      return 0;
  }
}

/// Name of a setting, for the log lines of the entity platforms.
const LogString *setting_name(Setting setting);

class AW88298 : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  // No setup priority: the default DATA runs after the I2C buses and after any IO expander a reset
  // pin sits on, and before the speaker, which is the component that needs the amplifier ready.

  bool set_mute_off() override { return this->set_mute_state_(false); }
  bool set_mute_on() override { return this->set_mute_state_(true); }
  bool is_muted() override { return this->is_muted_; }
  bool set_volume(float volume) override;
  float volume() override { return this->volume_; }
  /// Called by the speaker when its clocks start; the device can only lock to a running clock.
  void on_audio_started() override;

  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_data_bits(uint8_t data_bits) { this->data_bits_ = data_bits; }
  void set_slot_bits(uint8_t slot_bits) { this->slot_bits_ = slot_bits; }
  void set_slot_count(uint8_t slot_count) { this->slot_count_ = slot_count; }
  void set_rx_slot_mask(uint8_t rx_slot_mask) { this->rx_slot_mask_ = rx_slot_mask; }
  void set_i2s_format(I2SFormat format) { this->format_ = format; }
  void set_i2s_channel(ChannelSelect channel) { this->channel_ = channel; }
  void set_ws_invert(bool ws_invert) { this->ws_invert_ = ws_invert; }
  void set_bclk_invert(bool bclk_invert) { this->bclk_invert_ = bclk_invert; }
  void set_pll_from_ws(bool pll_from_ws) { this->pll_from_ws_ = pll_from_ws; }
  void set_ws_pulse_one_bck(bool ws_pulse_one_bck) { this->ws_pulse_one_bck_ = ws_pulse_one_bck; }
  /// Optional hardware reset on RSTN, driven through any GPIO the pin schema knows about.
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }
  void set_volume_min_db(float volume_min_db) { this->volume_min_db_ = volume_min_db; }
  void set_volume_max_db(float volume_max_db) { this->volume_max_db_ = volume_max_db; }
  void set_initial_volume(float initial_volume) { this->volume_ = initial_volume; }

  /// Store a setting. Before setup it only records what to configure, afterwards it also writes
  /// the register. Returns false when the value cannot be represented or the write failed.
  bool set_setting(Setting setting, float value);
  float get_setting(Setting setting) const { return this->settings_[static_cast<uint8_t>(setting)]; }

#ifdef AW88298_SENSOR_COUNT
  void add_sensor(SensorType type, sensor::Sensor *sensor);
#endif
#ifdef AW88298_STATUS_SENSOR_COUNT
  void add_status_binary_sensor(StatusFlag flag, binary_sensor::BinarySensor *sensor);
#endif

 protected:
  /// ESPHome 2026.11 adds `Component::status_comm_result()`. Declaring it here keeps the driver
  /// building against 2026.10, which has no such helper.
  bool status_comm_result(bool ok);

  /// Write one setting's register bits, using the value in settings_.
  bool write_setting_(Setting setting);
  /// Read modify write, so a change to one field keeps the others.
  bool update_bits_(uint8_t reg, uint16_t mask, uint16_t value);
  bool write_i2s_config_();
  uint16_t sysctrl_value_() const;
  uint16_t sysctrl2_value_() const;
  bool set_mute_state_(bool mute_state);
  /// Hold RSTN low, then let the device come back up before the first bus access.
  void reset_();
  bool read_monitor_(SensorType type, float *value);
  void publish_status_(uint16_t status);
  void publish_monitors_();

  GPIOPin *reset_pin_{nullptr};
  uint32_t sample_rate_{16000};
  uint8_t data_bits_{16};
  uint8_t slot_bits_{16};
  uint8_t slot_count_{2};
  uint8_t rx_slot_mask_{0x03};
  I2SFormat format_{I2SFormat::I2S_FORMAT_STANDARD};
  ChannelSelect channel_{ChannelSelect::CHANNEL_SELECT_LEFT};
  bool ws_invert_{false};
  bool bclk_invert_{false};
  bool pll_from_ws_{false};
  bool ws_pulse_one_bck_{false};
  bool initialized_{false};
  float volume_{1.0f};
  float volume_min_db_{-96.0f};
  float volume_max_db_{0.0f};
  float settings_[static_cast<uint8_t>(Setting::SETTING_COUNT)]{};

#ifdef AW88298_SENSOR_COUNT
  struct SensorEntry {
    SensorType type;
    sensor::Sensor *sensor;
  };
  StaticVector<SensorEntry, AW88298_SENSOR_COUNT> sensors_;
#endif
#ifdef AW88298_STATUS_SENSOR_COUNT
  struct StatusEntry {
    StatusFlag flag;
    binary_sensor::BinarySensor *sensor;
  };
  StaticVector<StatusEntry, AW88298_STATUS_SENSOR_COUNT> status_sensors_;
#endif
};

}  // namespace esphome::aw88298
