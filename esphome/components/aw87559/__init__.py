import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import i2c
from esphome.const import (
    CONF_ID,
    CONF_RESET_PIN,
    CONF_STARTUP_DELAY,
)

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_AW87559_ID = "aw87559_id"
CONF_PA_GAIN = "pa_gain"

aw87559_ns = cg.esphome_ns.namespace("aw87559")
AW87559Component = aw87559_ns.class_(
    "AW87559Component", cg.Component, i2c.I2CDevice
)

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AW87559_ID): cv.use_id(AW87559Component),
    }
)

# PA_GAIN covers 0dB .. 27dB in 1.5dB steps (datasheet register 0x06).
PA_GAIN_STEP = 1.5
PA_GAIN_DEFAULT = 24.0


def validate_pa_gain(value):
    if isinstance(value, str):
        value = value.strip()
        if value.lower().endswith("db"):
            value = value[:-2].strip()
    try:
        gain = float(value)
    except (TypeError, ValueError):
        raise cv.Invalid("PA gain must be a number in dB, e.g. 24dB")

    max_gain = 18 * PA_GAIN_STEP
    if not 0.0 <= gain <= max_gain:
        raise cv.Invalid(f"PA gain must be between 0dB and {max_gain:g}dB")

    steps = round(gain / PA_GAIN_STEP)
    if abs(steps * PA_GAIN_STEP - gain) > 0.01:
        raise cv.Invalid(f"PA gain must be a multiple of {PA_GAIN_STEP}dB")
    return steps


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AW87559Component),
            cv.Optional(CONF_PA_GAIN, default=PA_GAIN_DEFAULT): validate_pa_gain,
            cv.Optional(
                CONF_STARTUP_DELAY, default="1s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x5B))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_pa_gain_code(config[CONF_PA_GAIN]))
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))

    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))
