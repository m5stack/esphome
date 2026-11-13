import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_NUM_LEDS, CONF_OUTPUT_ID
from esphome.types import ConfigType

from .. import (
    CONF_M5_UNIT_8SERVOS2_CHAIN_ID,
    SERVOS2_CHANNEL_COUNT,
    SERVOS2_RGB_TOTAL_MAX,
    M5Unit8Servos2Chain,
    declare_rgb_light,
    m5_unit_8servos2_chain_ns,
    register_rgb_light,
)

CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["m5_unit_8servos2_chain"]

M5Unit8Servos2Light = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2Light",
    light.AddressableLight,
    cg.Component,
    cg.Parented.template(M5Unit8Servos2Chain),
)


def _validate(config: ConfigType) -> ConfigType:
    declare_rgb_light(
        config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID],
        config[CONF_CHANNEL],
        config[CONF_NUM_LEDS],
    )
    return config


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5Unit8Servos2Light),
            cv.Required(CONF_CHANNEL): cv.int_range(
                min=0, max=SERVOS2_CHANNEL_COUNT - 1
            ),
            cv.Required(CONF_NUM_LEDS): cv.int_range(min=1, max=SERVOS2_RGB_TOTAL_MAX),
            cv.GenerateID(CONF_M5_UNIT_8SERVOS2_CHAIN_ID): cv.use_id(
                M5Unit8Servos2Chain
            ),
        }
    ),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    register_rgb_light(
        config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID],
        config[CONF_CHANNEL],
        config[CONF_NUM_LEDS],
        var,
    )
