from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.axp_base import AxpComponent, axp_component_schema, register_axp
from esphome.components.axp_base.rails import final_validate_rail_claims
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RESTART

AUTO_LOAD = ["axp_base"]
CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "axp2101"

axp2101_ns = cg.esphome_ns.namespace("axp2101")
AXP2101 = axp2101_ns.class_("AXP2101", AxpComponent)

CONF_SHUTDOWN = "shutdown"
CONF_RESET_GAUGE = "reset_gauge"

#: One-shot operations the chip performs on command, mapped to the hub method
#: each one calls. The keys are also the button keys and the action names, so
#: `axp2101.shutdown` and the `shutdown:` button do the same thing.
CHIP_OPERATIONS = {
    CONF_SHUTDOWN: "power_off()",
    CONF_RESTART: "restart()",
    CONF_RESET_GAUGE: "reset_gauge()",
}

AXP_ACTION_SCHEMA = maybe_simple_id({cv.Required(CONF_ID): cv.use_id(AxpComponent)})

for _operation, _call in CHIP_OPERATIONS.items():
    automation.register_apply_action(
        f"{DOMAIN}.{_operation}", AXP_ACTION_SCHEMA, automation.ApplyCall(_call)
    )

# The rails this chip can switch, in datasheet order. Kept here so the switch and
# output platforms can validate `rail:` before the C++ model is involved; a test
# pins this list against the RAILS table in axp2101.cpp.
RAIL_CHANNELS = [
    "DCDC1",
    "DCDC2",
    "DCDC3",
    "DCDC4",
    "DCDC5",
    "ALDO1",
    "ALDO2",
    "ALDO3",
    "ALDO4",
    "BLDO1",
    "BLDO2",
    "CPUSLDO",
    "DLDO1",
    "DLDO2",
]

# The interrupt sources of this chip, in datasheet order. Kept here so the event
# platform can validate `events:` before the C++ model is involved; a test pins
# this list against the EVENTS table in axp2101.cpp.
EVENT_NAMES = [
    "soc_warning_level",
    "soc_shutdown_level",
    "gauge_watchdog_timeout",
    "gauge_new_soc",
    "battery_over_temperature_charging",
    "battery_under_temperature_charging",
    "battery_over_temperature_working",
    "battery_under_temperature_working",
    "vbus_insert",
    "vbus_remove",
    "battery_insert",
    "battery_remove",
    "power_key_short_press",
    "power_key_long_press",
    "power_key_negative_edge",
    "power_key_positive_edge",
    "watchdog_expired",
    "ldo_over_current",
    "batfet_over_current",
    "charge_done",
    "charge_started",
    "die_over_temperature",
    "charger_safety_timer",
    "battery_over_voltage",
]

CONFIG_SCHEMA = axp_component_schema(AXP2101, default_address=0x34)

FINAL_VALIDATE_SCHEMA = final_validate_rail_claims(DOMAIN)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await register_axp(var, config)
