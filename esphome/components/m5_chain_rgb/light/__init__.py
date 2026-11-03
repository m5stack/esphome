import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_RGB_ID, M5ChainRGB, m5_chain_rgb_ns

DEPENDENCIES = ["m5_chain_rgb"]

M5ChainRGBLight = m5_chain_rgb_ns.class_("M5ChainRGBLight", light.AddressableLight)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5ChainRGBLight),
        cv.GenerateID(CONF_M5_CHAIN_RGB_ID): cv.use_id(M5ChainRGB),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_CHAIN_RGB_ID])
