from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.axp_base import AxpComponent, axp_component_schema, register_axp
from esphome.components.axp_base.rails import final_validate_rail_claims
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["axp_base"]
CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "axp192"

axp192_ns = cg.esphome_ns.namespace("axp192")
AXP192 = axp192_ns.class_("AXP192", AxpComponent)
AXP192GPIOPin = axp192_ns.class_("AXP192GPIOPin", cg.GPIOPin)

CONF_SHUTDOWN = "shutdown"
CONF_RESET_GAUGE = "reset_gauge"

#: One-shot operations the chip performs on command, mapped to the hub method each
#: one calls. Then the keys are also the button keys and the action names, so
#: `axp192.shutdown` and the `shutdown:` button do the same thing.
#:
#: There is no restart entry: the chip powers the SoC down through REG 32H but has
#: no register that starts it again, unlike the AXP2101.
CHIP_OPERATIONS = {
    CONF_SHUTDOWN: "power_off()",
    CONF_RESET_GAUGE: "reset_gauge()",
}

AXP_ACTION_SCHEMA = maybe_simple_id({cv.Required(CONF_ID): cv.use_id(AxpComponent)})

for _operation, _call in CHIP_OPERATIONS.items():
    automation.register_apply_action(
        f"{DOMAIN}.{_operation}", AXP_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

# The rails this chip can switch, in datasheet order. Kept here so the switch and
# output platforms can validate `rail:` before the C++ model is involved; a test
# pins this list against the RAILS table in axp192.cpp.
#:
# LDO1 is not listed: it feeds the RTC, is always on and has a fixed voltage, so
# it cannot be switched or set.
RAIL_CHANNELS = [
    "DCDC1",
    "DCDC2",
    "DCDC3",
    "LDO2",
    "LDO3",
    "LDOIO0",
    "EXTEN",
]

# The interrupt sources of this chip, in datasheet order. Kept here so the event
# platform can validate `events:` before the C++ model is involved; a test pins
# this list against the EVENTS table in axp192.cpp.
EVENT_NAMES = [
    "acin_over_voltage",
    "acin_insert",
    "acin_remove",
    "vbus_over_voltage",
    "vbus_insert",
    "vbus_remove",
    "vbus_below_vhold",
    "battery_insert",
    "battery_remove",
    "battery_enter_active_mode",
    "battery_exit_active_mode",
    "battery_charge_started",
    "battery_charge_done",
    "battery_over_temperature",
    "battery_under_temperature",
    "die_over_temperature",
    "charge_current_below_setting",
    "dcdc1_under_voltage",
    "dcdc2_under_voltage",
    "dcdc3_under_voltage",
    "power_key_short_press",
    "power_key_long_press",
    "n_oe_power_on",
    "n_oe_power_off",
    "vbus_valid",
    "vbus_invalid",
    "vbus_session_valid",
    "vbus_session_end",
    "aps_low_voltage",
]

#: GPIO0 to GPIO4. GPIO5 shares the N_RSTO pin, which is the reset line of the
#: application processor on boards that use it, so it is not offered here.
GPIO_PIN_COUNT = 5

CONFIG_SCHEMA = axp_component_schema(AXP192, default_address=0x34)

FINAL_VALIDATE_SCHEMA = final_validate_rail_claims(DOMAIN)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await register_axp(var, config)


def _validate_modes(value: ConfigType) -> ConfigType:
    # One function field per pin, so a pin is an input or an output but never
    # both. Written out rather than reusing a pins helper, so the schema keeps
    # working on the ESPHome 2026.10 API the component targets.
    if value[CONF_INPUT] == value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    if value.get(CONF_PULLDOWN) and not value[CONF_INPUT]:
        raise cv.Invalid(f"'{CONF_PULLDOWN}' is only available on an input")
    return value


def _validate_pin(pin_config: ConfigType, _parent: ConfigType) -> None:
    if pin_config[CONF_MODE].get(CONF_PULLDOWN) and pin_config[CONF_NUMBER] > 2:
        raise cv.Invalid(f"'{CONF_PULLDOWN}' is only wired for GPIO0, GPIO1 and GPIO2")


#: The AXP192's free GPIOs, so any entity that takes a pin can use them. The
#: outputs are NMOS open drain only - there is no push-pull mode - which is why
#: `open_drain` is not a choice here: `output: true` already means it.
AXP192_PIN_SCHEMA = pins.gpio_base_schema(
    AXP192GPIOPin,
    cv.int_range(min=0, max=GPIO_PIN_COUNT - 1),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLDOWN],
    mode_validator=_validate_modes,
).extend({cv.Required(DOMAIN): cv.use_id(AXP192)})


@pins.PIN_SCHEMA_REGISTRY.register(DOMAIN, AXP192_PIN_SCHEMA, _validate_pin)
async def axp192_pin_to_code(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_parent(await cg.get_variable(config[DOMAIN])))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
