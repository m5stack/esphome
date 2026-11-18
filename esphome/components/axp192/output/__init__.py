import esphome.codegen as cg
from esphome.components import output
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
from esphome.components.axp_base.rails import CONF_RAIL, rail_schema
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MAX_POWER,
    CONF_MAX_VOLTAGE,
    CONF_MIN_POWER,
    CONF_POWER_SUPPLY,
)

from .. import RAIL_CHANNELS, axp192_ns

DEPENDENCIES = ["axp192"]

CONF_MIN_VOLTAGE = "min_voltage"

AXP192FloatOutput = axp192_ns.class_(
    "AXP192FloatOutput", output.FloatOutput, cg.Component
)

# The rail's own voltage window already sets both ends of the range, so the
# generic power scaling would be a second, conflicting scaling on top of it.
SCALING_KEYS = (CONF_MIN_POWER, CONF_MAX_POWER, output.CONF_ZERO_MEANS_ZERO)
# Inversion is meaningless for a voltage: it would map full level onto the
# lowest voltage. A power supply cannot switch a PMIC rail either.
OTHER_KEYS = (CONF_INVERTED, CONF_POWER_SUPPLY)


def _reject_unsupported_keys(config):
    for key in SCALING_KEYS:
        if key in config:
            raise cv.Invalid(
                f"'{key}' is not supported by the AXP192 output; use "
                f"'{CONF_MIN_VOLTAGE}' and '{CONF_MAX_VOLTAGE}' to set the range"
            )
    for key in OTHER_KEYS:
        if key in config:
            raise cv.Invalid(f"'{key}' is not supported by the AXP192 output")
    return config


def _validate_range(config):
    if config[CONF_MIN_VOLTAGE] >= config[CONF_MAX_VOLTAGE]:
        raise cv.Invalid(f"'{CONF_MIN_VOLTAGE}' must be below '{CONF_MAX_VOLTAGE}'")
    return config


# These are the widest windows any AXP192 rail offers. The per-rail window is
# checked on the device, where the model table has the exact figures.
VOLTAGE_RANGE = cv.int_range(min=700, max=3500)

CONFIG_SCHEMA = cv.All(
    # The rejection has to run before the schema below, because that one adds a
    # default for zero_means_zero and would trip on its own default.
    _reject_unsupported_keys,
    output.FLOAT_OUTPUT_SCHEMA.extend(rail_schema(RAIL_CHANNELS)).extend(
        {
            cv.GenerateID(): cv.declare_id(AXP192FloatOutput),
            cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
            cv.Optional(CONF_MIN_VOLTAGE, default=1800): VOLTAGE_RANGE,
            cv.Optional(CONF_MAX_VOLTAGE, default=3300): VOLTAGE_RANGE,
        }
    ),
    _validate_range,
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AXP_ID])

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    await cg.register_parented(var, parent)

    cg.add(var.set_rail_name(config[CONF_RAIL]))
    cg.add(var.set_min_voltage(config[CONF_MIN_VOLTAGE]))
    cg.add(var.set_max_voltage(config[CONF_MAX_VOLTAGE]))
