import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_NCIR2_ID, M5UnitNCIR2, m5_unit_ncir2_ns

DEPENDENCIES = ["m5_unit_ncir2"]

M5UnitNCIR2Light = m5_unit_ncir2_ns.class_(
    "M5UnitNCIR2Light", light.LightOutput, cg.Parented.template(M5UnitNCIR2)
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5UnitNCIR2Light),
        cv.GenerateID(CONF_M5_UNIT_NCIR2_ID): cv.use_id(M5UnitNCIR2),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_NCIR2_ID])
