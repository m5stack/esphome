from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_MODE,
    CONF_ID,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_RESET,
)
from esphome.core import TimePeriod
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]
DOMAIN = "si12t"

CONF_FIRST_TOUCH = "first_touch"
CONF_INTR_LEVEL = "intr_level"
CONF_RESPONSE_CYCLES = "response_cycles"
CONF_SI12T_ID = "si12t_id"
CONF_SLEEP_MODE = "sleep_mode"

si12t_ns = cg.esphome_ns.namespace("si12t")
SI12TComponent = si12t_ns.class_("SI12TComponent", cg.PollingComponent, i2c.I2CDevice)

# Datasheet 12.2.2: MS is 0 for autoselect of the fast and slow modes and 1 for fast mode
CFIG_MS_FAST = 1 << 7
# Datasheet 12.2.2, FTC[1:0]
FIRST_TOUCH_CODES = {5: 0b00, 10: 0b01, 15: 0b10, 20: 0b11}
# Datasheet 12.2.2, ILC[1:0]. 0b10 repeats 0b00 and is left out.
INTR_LEVEL_CODES = {
    "medium_high": 0b00,
    "low_medium_high": 0b01,
    "high": 0b11,
}
# Datasheet 12.2.2: the response cycle is RTC[2:0] + 2
RESPONSE_CYCLE_OFFSET = 2
# Datasheet 12.2.3: bits 1:0 of CTRL must stay 1, bit 2 is SLEEP and bit 3 is SRST
CTRL_RESERVED = 0b11
CTRL_SLEEP = 1 << 2


def _validate_first_touch(value: TimePeriod) -> TimePeriod:
    """Check that the first touch time fits the FTC[1:0] field."""
    if value.total_seconds not in FIRST_TOUCH_CODES:
        options = ", ".join(f"{allowed}s" for allowed in FIRST_TOUCH_CODES)
        raise cv.Invalid(f"Must be one of {options}")
    return value


def _reject_inverted_interrupt_pin(value: ConfigType) -> ConfigType:
    """Reject inversion, which would flip the meaning of the open drain interrupt line."""
    if value.get(CONF_INVERTED):
        raise cv.Invalid(
            f"'{CONF_INVERTED}: true' is not supported on '{CONF_INTERRUPT_PIN}'; "
            "the interrupt output is an open drain line that is low while a channel is touched"
        )
    return value


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SI12TComponent),
            cv.Optional(CONF_AUTO_MODE, default=True): cv.boolean,
            cv.Optional(CONF_FIRST_TOUCH, default="10s"): cv.All(
                cv.positive_time_period_seconds, _validate_first_touch
            ),
            cv.Optional(CONF_INTR_LEVEL, default="medium_high"): cv.enum(
                INTR_LEVEL_CODES, lower=True
            ),
            cv.Optional(CONF_RESPONSE_CYCLES, default=5): cv.int_range(min=2, max=9),
            cv.Optional(CONF_RESET, default=True): cv.boolean,
            cv.Optional(CONF_SLEEP_MODE, default=True): cv.boolean,
            cv.Optional(CONF_INTERRUPT_PIN): cv.All(
                pins.internal_gpio_input_pullup_pin_schema,
                _reject_inverted_interrupt_pin,
            ),
        }
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(i2c.i2c_device_schema(0x68))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    config_register = (
        (0 if config[CONF_AUTO_MODE] else CFIG_MS_FAST)
        | (FIRST_TOUCH_CODES[config[CONF_FIRST_TOUCH].total_seconds] << 5)
        | (INTR_LEVEL_CODES[config[CONF_INTR_LEVEL]] << 3)
        | (config[CONF_RESPONSE_CYCLES] - RESPONSE_CYCLE_OFFSET)
    )
    cg.add(var.set_config_register(config_register))
    cg.add(
        var.set_control_register(
            CTRL_RESERVED | (CTRL_SLEEP if config[CONF_SLEEP_MODE] else 0)
        )
    )
    cg.add(var.set_software_reset(config[CONF_RESET]))
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
