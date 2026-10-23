import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from .. import CONF_SI5351_ID, Si5351Component, si5351_ns, validate_clock_pin

DEPENDENCIES = ["si5351"]

Si5351Output = si5351_ns.class_("Si5351Output", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(Si5351Output),
        cv.GenerateID(CONF_SI5351_ID): cv.use_id(Si5351Component),
        cv.Required(CONF_CHANNEL): validate_clock_pin,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_SI5351_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
