import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv

from .. import BASE_SCHEMA, CONF_M5_VOICE_PYRAMID_KIT_ID, m5_voice_pyramid_kit_ns

DEPENDENCIES = ["m5_voice_pyramid_kit"]

CONF_TOUCH = "touch"

M5VoicePyramidKitTouch = m5_voice_pyramid_kit_ns.class_(
    "M5VoicePyramidKitTouch", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(M5VoicePyramidKitTouch)
    .extend(
        {
            cv.Required(CONF_TOUCH): cv.int_range(min=1, max=4),
        }
    )
    .extend(BASE_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_VOICE_PYRAMID_KIT_ID])

    cg.add(var.set_touch(config[CONF_TOUCH]))
