#pragma once

#include <cstddef>
#include <cstdint>

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::axp_base {

/// Interface for entities the hub refreshes after each poll.
///
/// Deliberately free of any ESPHome entity base so the hub header stays
/// independent of the platform components; the platform classes inherit from
/// this alongside the entity base they need.
class AxpPolledEntity {
 public:
  virtual ~AxpPolledEntity() = default;

  /// Push a fresh value to the entity. Runs on the main loop, never in an ISR.
  virtual void publish_from_chip() = 0;
};

/// Register number meaning "this chip has no such register".
static constexpr uint8_t AXP_REG_NONE = 0xFF;
/// Status registers cached by update_status().
static constexpr uint8_t AXP_MAX_STATUS_REGS = 2;
/// Interrupt status and enable registers handled in one go.
static constexpr uint8_t AXP_MAX_IRQ_REGS = 4;
/// Events addressable through the 64 bit event mask passed to event callbacks.
static constexpr uint8_t AXP_MAX_EVENTS = 64;

/// One linear voltage window: millivolt = min_mv + step_mv * (code - min_code).
///
/// A rail is described by an array of these because several AXP converters use
/// more than one step size (for example AXP2101 DCDC3 has three windows).
struct AxpVoltageRange {
  uint16_t min_mv;
  uint16_t step_mv;
  uint8_t min_code;
  uint8_t max_code;
};

enum AxpRailFlags : uint8_t {
  /// The rail powers the MCU or the PMIC itself; disabling it is refused.
  AXP_RAIL_ALWAYS_ON = 1 << 0,
  /// The rail's voltage register accepts a dynamic-voltage-management bit in
  /// bit 7, which ramps between set points instead of jumping.
  AXP_RAIL_HAS_DVM = 1 << 1,
};

/// Bit 7 of a voltage register enables DVM on the converters that have it.
static constexpr uint8_t AXP_RAIL_DVM_MASK = 0x80;

/// A switched power output (DCDC, LDO, EXTEN, ...).
struct AxpRail {
  const char *name;
  const AxpVoltageRange *ranges;
  uint16_t max_ma;
  uint8_t range_count;
  uint8_t flags;
  /// Register and bits that switch the rail on. `enable_reg` is AXP_REG_NONE
  /// when the chip has no switch for this output.
  uint8_t enable_reg;
  uint8_t enable_mask;
  uint8_t enable_value;
  /// Register and bit field that select the output voltage. `vsel_reg` is
  /// AXP_REG_NONE when the output voltage is fixed.
  uint8_t vsel_reg;
  uint8_t vsel_mask;
  uint8_t vsel_shift;
};

/// An analog channel read through the chip's ADC.
///
/// The raw value is assembled from up to three registers. With `mid_reg` set the
/// channel is a plain 24 bit big endian value (AXP192 battery power); otherwise
/// the value is `hi_bits` from `hi_reg` followed by `lo_bits` from `lo_reg`.
struct AxpAdcChannel {
  const char *name;
  /// Engineering units per raw count (mV, mA, degrees Celsius, ...).
  float lsb;
  /// Added after scaling, for channels that are not proportional to zero.
  float offset;
  uint8_t enable_reg;
  uint8_t enable_bit;
  uint8_t hi_reg;
  uint8_t hi_mask;
  uint8_t mid_reg;
  uint8_t lo_reg;
  uint8_t lo_mask;
  uint8_t lo_bits;
};

/// Tests `(register & mask) == value`, which covers both single bit flags and
/// multi bit enums such as the AXP2101 charging state.
struct AxpStatusBit {
  uint8_t reg;
  uint8_t mask;
  uint8_t value;
};

/// A status flag the chip reports, addressed by the name used in the
/// configuration. Flags a chip does not have are simply left out of its table.
struct AxpStatusFlag {
  const char *name;
  AxpStatusBit bit;
};

/// A bit field inside one register.
struct AxpBitField {
  uint8_t reg;
  uint8_t mask;
  uint8_t shift;
};

struct AxpLutEntry {
  uint16_t value;
  uint8_t code;
};

/// A bit field whose values are a lookup table rather than a linear scale, as
/// used for charge currents and charge target voltages.
struct AxpLutField {
  const AxpLutEntry *entries;
  uint8_t count;
  uint8_t reg;
  uint8_t mask;
  uint8_t shift;
};

/// A setting with a fixed set of values, addressed by name. The lookup table
/// entries are the options in the order a select presents them.
struct AxpSelect {
  const char *name;
  AxpLutField field;
};

/// A setting with a linear numeric scale, addressed by name: the value is
/// `offset + step * code`, which covers thresholds, currents and voltage limits.
struct AxpNumber {
  const char *name;
  int32_t offset;
  uint16_t step;
  AxpBitField field;
  uint8_t min_code;
  uint8_t max_code;
};

/// A single-bit setting that can be switched on and off, addressed by name.
struct AxpFlag {
  const char *name;
  AxpBitField field;
};

/// One interrupt source. Indices are relative to the model's interrupt bases.
struct AxpEvent {
  const char *name;
  uint8_t status_index;
  uint8_t bit;
  uint8_t enable_index;
  uint8_t enable_bit;
};

/// Everything that differs between AXP chips. Subclasses own one static
/// instance of this and hand it to AxpComponent, so the shared logic never has
/// to know which chip it is talking to.
///
/// The tables are plain `constexpr` data. On ESP8266 that costs RAM, which is
/// acceptable because AXP PMICs only ship on ESP32 based boards.
struct AxpModel {
  const char *name;
  const AxpRail *rails;
  const AxpAdcChannel *adc_channels;
  const AxpEvent *events;
  const AxpStatusFlag *status_flags;
  /// Enum-like settings, each with its own value table.
  const AxpSelect *selects;
  /// Settings on a linear scale.
  const AxpNumber *numbers;
  /// Settings that are simply on or off.
  const AxpFlag *flags;
  /// One-shot controls: writing a one acts and the chip clears the bit again.
  /// `reg` is AXP_REG_NONE when the chip has no such control.
  AxpBitField power_off;
  AxpBitField restart;
  AxpBitField gauge_reset;
  uint8_t chip_id_reg;
  uint8_t chip_id;
  uint8_t status_base;
  uint8_t status_count;
  uint8_t rail_count;
  uint8_t adc_count;
  uint8_t event_count;
  uint8_t status_flag_count;
  uint8_t select_count;
  uint8_t number_count;
  uint8_t flag_count;
  uint8_t irq_status_base;
  uint8_t irq_status_count;
  uint8_t irq_enable_base;
  /// State of charge in percent, or AXP_REG_NONE when the chip has no gauge.
  uint8_t gauge_soc_reg;
};

inline bool axp_rail_has_enable(const AxpRail &rail) { return rail.enable_reg != AXP_REG_NONE; }

inline bool axp_rail_has_voltage(const AxpRail &rail) {
  return rail.vsel_reg != AXP_REG_NONE && rail.ranges != nullptr && rail.range_count != 0;
}

/// Shared implementation for the X-Powers AXP PMIC family.
///
/// The class provides the mechanisms (register access, rail control, ADC reads,
/// interrupt handling) and leaves every chip specific decision to the model
/// table. Subclasses only declare their table and, when needed, override
/// `init_chip_()`.
class AxpComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  explicit AxpComponent(const AxpModel &model) : model_(&model) {}

  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }

  const AxpModel &get_model() const { return *this->model_; }
  /// True once setup() found the expected chip ID.
  bool is_chip_ready() const { return this->chip_ready_; }

  /// Look a rail up by name; returns nullptr when this chip has no such output.
  const AxpRail *find_rail(const char *name) const;

  /// Look an ADC channel up by name; returns nullptr when this chip has none
  /// with that name.
  const AxpAdcChannel *find_adc_channel(const char *name) const;

  /// Look a status flag up by name; returns nullptr when this chip has no such
  /// flag.
  const AxpStatusFlag *find_status_flag(const char *name) const;

#ifdef AXP_POLLED_ENTITY_COUNT
  /// Register an entity so the hub refreshes it after each poll.
  void add_polled_entity(AxpPolledEntity *entity) { this->polled_entities_.push_back(entity); }
#endif

  bool set_rail_enabled(const AxpRail *rail, bool enabled);
  bool is_rail_enabled(const AxpRail *rail);
  /// Set the output voltage. Fails unless the chip can produce exactly this value.
  bool set_rail_voltage(const AxpRail *rail, uint16_t millivolt);
  bool get_rail_voltage(const AxpRail *rail, uint16_t &millivolt);

  /// Map between millivolt and the chip's register code. Both return false when
  /// the value is not representable, so callers can reject it instead of
  /// silently picking a neighbouring voltage.
  static bool encode_voltage(const AxpRail &rail, uint16_t millivolt, uint8_t &code);
  static bool decode_voltage(const AxpRail &rail, uint8_t code, uint16_t &millivolt);

  /// Pick the reachable voltage closest to `millivolt` and report what will
  /// actually come out. Used where the value is continuous, such as a dimmer.
  static bool encode_voltage_nearest(const AxpRail &rail, uint16_t millivolt, uint8_t &code, uint16_t &applied);
  bool set_rail_voltage_nearest(const AxpRail *rail, uint16_t millivolt, uint16_t *applied = nullptr);

  /// How many distinct voltages the rail can produce inside [min_mv, max_mv].
  static uint16_t count_rail_levels(const AxpRail &rail, uint16_t min_mv, uint16_t max_mv);

  /// Turn the rail's voltage ramping on or off. False when the rail has no DVM.
  bool set_rail_dvm(const AxpRail *rail, bool enabled);

  /// Read one ADC channel in its engineering unit, or NAN on a bus error.
  float read_adc(const AxpAdcChannel &channel);
  bool set_adc_enabled(const AxpAdcChannel &channel, bool enabled);

  /// Refresh the cached status registers and return whether that worked.
  bool update_status();
  /// Read a flag from the cache filled by update_status().
  bool status_bit(const AxpStatusBit &bit) const;

  /// Write the table entry closest to `value`; `applied` reports what was
  /// selected so the caller can tell an exact match from a snap.
  bool write_lut_value(const AxpLutField &field, uint16_t value, uint16_t *applied = nullptr);
  bool read_lut_value(const AxpLutField &field, uint16_t &value);

  /// Look an enum-like setting up by name; returns nullptr when this chip has
  /// no such setting.
  const AxpSelect *find_select(const char *name) const;
  /// Read the raw register code behind a setting.
  bool read_select_code(const AxpSelect &select, uint8_t &code);
  /// Write the setting's option at `index`, which is the position in its table.
  bool write_select_index(const AxpSelect &select, uint8_t index);

  /// Look a linear setting up by name; returns nullptr when this chip has none.
  const AxpNumber *find_number(const char *name) const;
  bool read_number_code(const AxpNumber &number, uint8_t &code);
  /// Write a register code for a linear setting, rejecting codes it cannot hold.
  bool write_number_code(const AxpNumber &number, uint8_t code);
  /// The value a code stands for.
  static int32_t decode_number(const AxpNumber &number, uint8_t code);
  /// The code closest to `value`, clamped to the codes the setting can hold.
  static uint8_t encode_number(const AxpNumber &number, int32_t value);

  /// Look a single-bit setting up by name; returns nullptr when this chip has
  /// none with that name.
  const AxpFlag *find_flag(const char *name) const;
  /// Read a flag's state. False only when the bus read failed.
  bool read_flag(const AxpFlag &flag, bool &on);
  bool write_flag(const AxpFlag &flag, bool on);

  /// Charger and fuel gauge controls. Each returns false when the chip has no
  /// such control or the register access failed.
  bool set_charging_enabled(bool enabled);
  bool is_charging_enabled();
  /// Charge current in mA.
  bool set_charge_current(uint16_t milliamps, uint16_t *applied = nullptr);
  bool get_charge_current(uint16_t &milliamps);
  /// Charge target voltage in mV.
  bool set_charge_voltage(uint16_t millivolt, uint16_t *applied = nullptr);
  bool get_charge_voltage(uint16_t &millivolt);
  bool set_gauge_enabled(bool enabled);
  bool is_gauge_enabled();
  /// State of charge in percent, read straight from the chip's fuel gauge.
  bool read_state_of_charge(uint8_t &percent);

  /// One-shot chip controls. Each returns false when the chip has no such
  /// control or the register access failed.
  ///
  /// `power_off()` removes the PMIC's outputs, which stops the MCU with it, so
  /// nothing after it in the same action runs. `restart()` power-cycles the
  /// SoC through the PMIC's own sequence. `reset_gauge()` asks the fuel gauge to
  /// recalibrate, which is what it needs after a battery swap.
  bool power_off();
  bool restart();
  bool reset_gauge();

  /// Attach the PMIC interrupt line. Call before setup().
  void set_irq_pin(InternalGPIOPin *pin) { this->irq_pin_ = pin; }

  /// How often the interrupt status registers are read when no IRQ pin is
  /// wired. Zero disables polling, leaving the events to the IRQ pin.
  void set_event_poll_interval(uint32_t interval) { this->event_poll_interval_ = interval; }

  /// Subscribe to interrupt events. The callback receives a mask where bit `i`
  /// matches `model.events[i]` and runs on the main loop, never in the ISR.
  ///
  /// Events only reach the callback once something enabled them with
  /// `add_events_enabled()`; the hub then drains them from the IRQ pin when one
  /// is wired and from the status registers otherwise.
  template<typename F> void add_on_event(F &&callback) { this->event_callback_.add(std::forward<F>(callback)); }

  /// Index of an interrupt event in `model.events`, or -1 when the chip has no
  /// event with that name.
  int16_t find_event_index(const char *name) const;

  /// Enable exactly the events in `mask`, disabling all others.
  bool set_events_enabled(uint64_t mask);
  /// Add the events in `mask` to those already enabled. The first call also
  /// drops whatever the chip latched before anything was listening.
  bool add_events_enabled(uint64_t mask);
  /// Read the interrupt status registers, clear the bits that were set, and
  /// report them as a mask.
  bool read_and_clear_events(uint64_t &mask);

 protected:
  /// Chip specific preparation, run at the end of setup(). Return false to mark
  /// the component failed. The default does nothing.
  virtual bool init_chip_() { return true; }

  bool read_reg_(uint8_t reg, uint8_t &value);
  bool write_reg_(uint8_t reg, uint8_t value);
  bool update_reg_(uint8_t reg, uint8_t mask, uint8_t value);
  bool read_regs_(uint8_t reg, uint8_t *data, size_t length);
  bool write_regs_(uint8_t reg, const uint8_t *data, size_t length);
  bool read_field_(const AxpBitField &field, uint8_t &code);
  bool write_field_(const AxpBitField &field, uint8_t code);

  /// Record a bus or protocol failure without repeating an already pending warning.
  void report_error_(const LogString *message);

  /// Whether loop() should read the interrupt status registers this pass.
  bool should_drain_events_();

  /// Write a one-shot control's bit so the chip performs the action. `action`
  /// names it for the log line used when the chip has no such control.
  bool trigger_control_(const AxpBitField &field, const char *action);

  const AxpModel *model_;
  bool chip_ready_{false};
  uint8_t status_[AXP_MAX_STATUS_REGS]{};
  bool status_valid_{false};
  InternalGPIOPin *irq_pin_{nullptr};
  volatile bool irq_pending_{false};
  /// Events something subscribed to, and therefore the only ones the chip reports.
  uint64_t events_enabled_mask_{0};
  uint32_t event_poll_interval_{100};
  uint32_t last_event_poll_{0};
  LazyCallbackManager<void(uint64_t)> event_callback_;

#ifdef AXP_POLLED_ENTITY_COUNT
  StaticVector<AxpPolledEntity *, AXP_POLLED_ENTITY_COUNT> polled_entities_;
#endif

 private:
  static void irq_isr_(AxpComponent *arg);
};

}  // namespace esphome::axp_base
