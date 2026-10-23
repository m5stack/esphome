import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_NUM_LEDS, CONF_OUTPUT_ID

from .. import BASE_SCHEMA, CONF_M5_VOICE_PYRAMID_KIT_ID, m5_voice_pyramid_kit_ns

DEPENDENCIES = ["m5_voice_pyramid_kit"]

CONF_START_LED = "start_led"
CONF_REFRESH_INTERVAL = "refresh_interval"
CONF_REVERSE = "reverse"

M5VoicePyramidKitLight = m5_voice_pyramid_kit_ns.class_(
    "M5VoicePyramidKitLight", light.AddressableLight
)


def validate_led_range(config):
    if config[CONF_START_LED] + config[CONF_NUM_LEDS] > 14:
        raise cv.Invalid(
            "start_led + num_leds must not exceed 14; each kit channel controls 14 LEDs"
        )
    return config


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5VoicePyramidKitLight),
            cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=2),
            cv.Optional(CONF_START_LED, default=0): cv.int_range(min=0, max=13),
            cv.Optional(CONF_NUM_LEDS, default=14): cv.int_range(min=1, max=14),
            cv.Optional(CONF_REVERSE, default=False): cv.boolean,
            cv.Optional(
                CONF_REFRESH_INTERVAL, default="33ms"
            ): cv.positive_time_period_microseconds,
        }
    ).extend(BASE_SCHEMA),
    validate_led_range,
)


async def to_code(config):
    var = await light.new_light(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_VOICE_PYRAMID_KIT_ID])

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(var.set_start_led(config[CONF_START_LED]))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_reverse(config[CONF_REVERSE]))
    cg.add(var.set_refresh_interval(config[CONF_REFRESH_INTERVAL].total_microseconds))
