#pragma once

// Register encoding and decoding for the AW88298 (datasheet V1.6). Everything here is a pure
// function of its arguments so it can be unit tested without a bus or a component instance.

#include <cmath>
#include <cstdint>

namespace esphome::aw88298 {

/// I2SCTRL.I2SMD: format of the digital audio input.
enum class I2SFormat : uint8_t {
  I2S_FORMAT_STANDARD = 0,
  I2S_FORMAT_LEFT_JUSTIFIED = 1,
  I2S_FORMAT_RIGHT_JUSTIFIED = 2,
};

/// I2SCTRL.CHSEL: which input channel is played, or the average of both.
enum class ChannelSelect : uint8_t {
  CHANNEL_SELECT_LEFT = 1,
  CHANNEL_SELECT_RIGHT = 2,
  CHANNEL_SELECT_MONO = 3,
};

/// BSTCTRL2.BST_MODE for one of the four modes, in the order the configuration lists them:
/// pass-through, force boost, smart boost 1, smart boost 2.
inline uint8_t boost_mode_code(uint8_t index) {
  switch (index) {
    case 1:
      return 0b001;
    case 2:
      return 0b101;
    case 3:
      // The datasheet leaves every value except 000, 001 and 101 to smart boost 2 and uses 110.
      return 0b110;
    default:
      return 0b000;
  }
}

/// Everything I2SCTRL and I2SCFG1 are built from. The values come from the I2S configuration of
/// the speaker that drives this amplifier, see audio_dac.py.
struct I2SSettings {
  uint8_t sample_rate_code;  // I2SSR, from sample_rate_code()
  uint8_t data_bits;         // 16, 20, 24 or 32 bits per sample
  uint8_t slot_bits;         // slot length in BCK cycles: 16, 24 or 32
  uint8_t slot_count;        // 2 for I2S, 4 for TDM
  uint8_t rx_slot_mask;      // I2SCFG1.I2S_RX_SLOTVLD, one bit per active slot
  I2SFormat format;
  ChannelSelect channel;
  bool input_attenuation;
  bool ws_pulse_one_bck;  // I2SCFG1.FSYNC_TYPE: WCK is one BCK wide instead of one slot
};

/// I2SSR for one of the documented sample rates. False for anything the chip cannot lock to.
inline bool sample_rate_code(uint32_t sample_rate, uint8_t *code) {
  switch (sample_rate) {
    case 8000:
      *code = 0;
      return true;
    case 11025:
      *code = 1;
      return true;
    case 12000:
      *code = 2;
      return true;
    case 16000:
      *code = 3;
      return true;
    case 22050:
      *code = 4;
      return true;
    case 24000:
      *code = 5;
      return true;
    case 32000:
      *code = 6;
      return true;
    case 44100:
      *code = 7;
      return true;
    case 48000:
      *code = 8;
      return true;
    case 96000:
      *code = 9;
      return true;
    default:
      return false;
  }
}

/// I2SFS for a data width, or 0xFF when the chip does not support it.
inline uint8_t data_width_code(uint8_t data_bits) {
  switch (data_bits) {
    case 16:
      return 0;
    case 20:
      return 1;
    case 24:
      return 2;
    case 32:
      return 3;
    default:
      return 0xFF;
  }
}

/// I2SBCK from the bit clock ratio, which is the slot length times the slot count.
/// False when that ratio is not one of 32, 48 or 64 BCK cycles per frame.
inline bool bck_code(uint8_t slot_bits, uint8_t slot_count, uint8_t *code) {
  switch (slot_bits * slot_count) {
    case 32:
      *code = 0;
      return true;
    case 48:
      *code = 1;
      return true;
    case 64:
      *code = 2;
      return true;
    default:
      return false;
  }
}

/// I2SCTRL (0x06): INPLEV, I2SRXEN, CHSEL, I2SMD, I2SFS, I2SBCK and I2SSR in one word.
inline bool encode_i2sctrl(const I2SSettings &settings, uint16_t *value) {
  uint8_t width_code = data_width_code(settings.data_bits);
  uint8_t clock_code;
  if (width_code == 0xFF || !bck_code(settings.slot_bits, settings.slot_count, &clock_code))
    return false;

  *value = static_cast<uint16_t>((settings.input_attenuation ? 1u : 0u) << 13 | 1u << 12 |  //
                                 static_cast<uint16_t>(settings.channel) << 10 |            //
                                 static_cast<uint16_t>(settings.format) << 8 |              //
                                 static_cast<uint16_t>(width_code) << 6 |                   //
                                 static_cast<uint16_t>(clock_code) << 4 |                   //
                                 (settings.sample_rate_code & 0x0FU));
  return true;
}

/// I2SCFG1 (0x07): active RX slots, unused channels tri-stated, WCK width, slot count, and the
/// transmitter left off.
inline uint16_t encode_i2scfg1(const I2SSettings &settings) {
  return static_cast<uint16_t>((settings.rx_slot_mask & 0x0FU) << 8 | 1u << 4 |
                               (settings.ws_pulse_one_bck ? 1u : 0u) << 3 | (settings.slot_count == 4 ? 1u : 0u) << 2);
}

/// HAGCCFG4.VOL for an attenuation in dB, 0 dB to -96 dB in 0.5 dB steps.
/// Anything at or past the last step saturates at 0xFF, the largest attenuation.
inline uint8_t volume_reg_from_db(float db) {
  const float attenuation = -db;
  if (attenuation <= 0.0f)
    return 0x00;
  if (attenuation >= 96.0f)
    return 0xFF;

  uint8_t coarse = static_cast<uint8_t>(attenuation / 6.0f);
  auto fine = static_cast<uint8_t>(std::lroundf((attenuation - coarse * 6.0f) * 2.0f));
  if (fine > 11) {
    // A remainder of 6 dB belongs to the next coarse step, and there is none past 0xF.
    coarse++;
    fine = 0;
  }
  if (coarse > 15)
    return 0xFF;
  return static_cast<uint8_t>(coarse << 4 | fine);
}

/// The attenuation an HAGCCFG4.VOL value stands for, in dB.
inline float volume_db_from_reg(uint8_t value) { return -((value >> 4) * 6.0f + (value & 0x0F) * 0.5f); }

/// BSTCTRL2.VOUT_VREFSET: 5.0 V at 0x0F up to 10.25 V at 0x39 in 125 mV steps.
inline bool boost_voltage_reg(float volts, uint8_t *value) {
  auto step = static_cast<int>(std::lroundf((volts - 5.0f) / 0.125f));
  if (step < 0 || step > 42)
    return false;
  *value = static_cast<uint8_t>(15 + step);
  return true;
}

/// The boost output voltage a BSTCTRL2.VOUT_VREFSET value stands for.
inline float boost_voltage_from_reg(uint8_t value) { return 5.0f + (static_cast<int>(value) - 15) * 0.125f; }

/// SYSCTRL2.BST_IPEAK: 1.5 A at 0 to 4.25 A at 0x0B in 250 mA steps.
inline bool boost_current_limit_reg(float amps, uint8_t *value) {
  auto step = static_cast<int>(std::lroundf((amps - 1.5f) / 0.25f));
  if (step < 0 || step > 11)
    return false;
  *value = static_cast<uint8_t>(step);
  return true;
}

/// The peak current limit a SYSCTRL2.BST_IPEAK value stands for.
inline float boost_current_limit_from_reg(uint8_t value) { return 1.5f + value * 0.25f; }

/// BSTCTRL1.BST_ATH and BSTCTRL1.BST_RTH are 6 bit fractions of full scale.
inline uint8_t boost_threshold_reg(float percent) {
  long step = std::lroundf(percent * 64.0f / 100.0f);
  if (step < 1)
    return 1;
  if (step > 63)
    return 63;
  return static_cast<uint8_t>(step);
}

/// Smart boost works on the percentage of full scale, so it converts back the same way.
inline float boost_threshold_percent(uint8_t value) { return value * 100.0f / 64.0f; }

/// HAGCCFG1.RVTH and HAGCCFG1.AVTH are whole percent of full scale.
inline uint8_t agc_threshold_reg(float percent) {
  long value = std::lroundf(percent);
  if (value < 1)
    return 1;
  if (value > 100)
    return 100;
  return static_cast<uint8_t>(value);
}

/// HAGCCFG2.ATTH and HAGCCFG3.RTTH count 20.8 us units.
inline constexpr float AGC_TIME_UNIT_MS = 0.0208f;
/// HAGCCFG4.HOLDTH counts about 1.33 ms units.
inline constexpr float AGC_HOLD_TIME_UNIT_MS = 1.33f;

/// HAGCCFG2.ATTH, HAGCCFG3.RTTH and HAGCCFG4.HOLDTH count fixed units of time.
inline uint16_t agc_time_reg(float milliseconds, float unit_ms) {
  auto value = static_cast<long>(std::lroundf(milliseconds / unit_ms));
  if (value < 1)
    return 1;
  if (value > 0xFFFF)
    return 0xFFFF;
  return static_cast<uint16_t>(value);
}

/// The time an HAGCCFG2.ATTH or HAGCCFG3.RTTH value stands for. One unit is 20.8 us.
inline float agc_time_ms(uint16_t value, float unit_ms) { return value * unit_ms; }

/// VDD.VBAT_DET: 10 bits over a 6.025 V full range.
inline float vdd_from_reg(uint16_t value) { return (value & 0x03FF) * 6.025f / 1023.0f; }

/// PVDD.PVDD_DET: 10 bits over a 12.05 V full range.
inline float pvdd_from_reg(uint16_t value) { return (value & 0x03FF) * 12.05f / 1023.0f; }

/// TEMP.TEMP_DET: 10 bit two's complement, one degree per count.
inline float temperature_from_reg(uint16_t value) {
  auto raw = static_cast<int16_t>(value & 0x03FF);
  if (raw & 0x0200)
    raw -= 0x0400;
  return raw;
}

/// HAGCST.BSTVOUT_ST is the boost voltage the device settled on, in 125 mV steps from 5.0 V.
inline float boost_status_voltage_from_reg(uint16_t value) {
  return 5.0f + (static_cast<int>(value & 0x3F) - 15) * 0.125f;
}

}  // namespace esphome::aw88298
