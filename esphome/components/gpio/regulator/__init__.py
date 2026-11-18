"""GPIO regulator platform: a load switch driven by a GPIO pin."""

from esphome import pins
import esphome.codegen as cg
from esphome.components import regulator
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN
from esphome.types import ConfigType

from .. import gpio_ns

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["regulator"]

GPIORegulator = gpio_ns.class_("GPIORegulator", regulator.Regulator)

_VOLTAGE_KEYS = (
    regulator.CONF_VOLTAGE,
    regulator.CONF_MIN_VOLTAGE,
    regulator.CONF_MAX_VOLTAGE,
)


def _reject_voltage(config: ConfigType) -> ConfigType:
    for key in _VOLTAGE_KEYS:
        if key in config:
            raise cv.Invalid(
                f"'{key}' is not supported by a GPIO regulator: it switches a "
                f"fixed supply and cannot change its voltage"
            )
    return config


CONFIG_SCHEMA = cv.All(
    _reject_voltage,
    regulator.regulator_schema(
        GPIORegulator,
        {cv.Required(CONF_PIN): pins.gpio_output_pin_schema},
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await regulator.register_regulator(var, config)
    cg.add(var.set_pin(await cg.gpio_pin_expression(config[CONF_PIN])))
