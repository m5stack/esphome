import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_ACSSR_ID, M5UnitACSSR, m5_unit_acssr_ns

DEPENDENCIES = ["m5_unit_acssr"]

M5UnitACSSRLight = m5_unit_acssr_ns.class_(
    "M5UnitACSSRLight", light.LightOutput, cg.Parented.template(M5UnitACSSR)
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5UnitACSSRLight),
        cv.GenerateID(CONF_M5_UNIT_ACSSR_ID): cv.use_id(M5UnitACSSR),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_ACSSR_ID])
