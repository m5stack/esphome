import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_AW9523B_ID, aw9523b_ns

DEPENDENCIES = ["aw9523b"]

AW9523BFloatOutput = aw9523b_ns.class_(
    "AW9523BFloatOutput", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(AW9523BFloatOutput),
        # Only pins driven in LED (PWM) mode; the final validation rejects a pin that is also
        # configured as a GPIO pin or by another output.
        cv.Required(CONF_PIN): cv.int_range(min=0, max=15),
    }
).extend(BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_AW9523B_ID])
    await output.register_output(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_pin(config[CONF_PIN]))
