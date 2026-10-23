from esphome import automation, pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_ID,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_UPDATE_INTERVAL,
)

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

CONF_M5_VOICE_PYRAMID_KIT_ID = "m5_voice_pyramid_kit_id"
CONF_TOUCH_POLLING = "touch_polling"
CONF_SWIPE_TIMEOUT = "swipe_timeout"
CONF_VOLTAGE_UPDATE_INTERVAL = "voltage_update_interval"
CONF_CHANNEL1_BRIGHTNESS = "channel1_brightness"
CONF_CHANNEL2_BRIGHTNESS = "channel2_brightness"
CONF_BRIGHTNESS = "brightness"
CONF_ENABLED = "enabled"

m5_voice_pyramid_kit_ns = cg.esphome_ns.namespace("m5_voice_pyramid_kit")
M5VoicePyramidKit = m5_voice_pyramid_kit_ns.class_(
    "M5VoicePyramidKit", cg.PollingComponent, i2c.I2CDevice
)
M5VoicePyramidKitPin = m5_voice_pyramid_kit_ns.class_(
    "M5VoicePyramidKitPin", cg.GPIOPin
)

ResetSpkAction = m5_voice_pyramid_kit_ns.class_("ResetSpkAction", automation.Action)
SetBrightnessAction = m5_voice_pyramid_kit_ns.class_(
    "SetBrightnessAction", automation.Action
)
SaveBrightnessAction = m5_voice_pyramid_kit_ns.class_(
    "SaveBrightnessAction", automation.Action
)
SetTouchPollingAction = m5_voice_pyramid_kit_ns.class_(
    "SetTouchPollingAction", automation.Action
)

BASE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_M5_VOICE_PYRAMID_KIT_ID): cv.use_id(M5VoicePyramidKit),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5VoicePyramidKit),
            cv.Optional(CONF_UPDATE_INTERVAL, default="30ms"): cv.update_interval,
            cv.Optional(CONF_TOUCH_POLLING, default=True): cv.boolean,
            cv.Optional(
                CONF_SWIPE_TIMEOUT, default="500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_VOLTAGE_UPDATE_INTERVAL, default="5s"): cv.update_interval,
            cv.Optional(CONF_CHANNEL1_BRIGHTNESS, default=100): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(CONF_CHANNEL2_BRIGHTNESS, default=100): cv.int_range(
                min=0, max=100
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x1A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add(var.set_touch_polling_enabled(config[CONF_TOUCH_POLLING]))
    cg.add(var.set_swipe_timeout(config[CONF_SWIPE_TIMEOUT].total_milliseconds))
    cg.add(
        var.set_voltage_update_interval(
            config[CONF_VOLTAGE_UPDATE_INTERVAL].total_milliseconds
        )
    )
    cg.add(var.set_initial_brightness(1, config[CONF_CHANNEL1_BRIGHTNESS]))
    cg.add(var.set_initial_brightness(2, config[CONF_CHANNEL2_BRIGHTNESS]))


def validate_pin_number(value):
    if value != 0:
        raise cv.Invalid(
            "The M5Stack Voice Pyramid Kit only exposes virtual pin 0 (SPK reset)."
        )
    return value


def validate_pin_mode(value):
    if not value.get(CONF_OUTPUT, False):
        raise cv.Invalid("The M5Stack Voice Pyramid Kit virtual pin is output-only.")
    return value


M5_VOICE_PYRAMID_KIT_PIN_SCHEMA = pins.gpio_base_schema(
    M5VoicePyramidKitPin,
    validate_pin_number,
    modes=[CONF_OUTPUT],
    mode_validator=validate_pin_mode,
).extend(BASE_SCHEMA)


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_M5_VOICE_PYRAMID_KIT_ID, M5_VOICE_PYRAMID_KIT_PIN_SCHEMA
)
async def m5_voice_pyramid_kit_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5_VOICE_PYRAMID_KIT_ID])
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var


M5_VOICE_PYRAMID_KIT_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5VoicePyramidKit),
    }
)


@automation.register_action(
    "m5_voice_pyramid_kit.reset_spk",
    ResetSpkAction,
    M5_VOICE_PYRAMID_KIT_ACTION_SCHEMA,
    synchronous=True,
)
async def reset_spk_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


M5_VOICE_PYRAMID_KIT_SET_BRIGHTNESS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5VoicePyramidKit),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=2),
        cv.Required(CONF_BRIGHTNESS): cv.templatable(cv.int_range(min=0, max=100)),
    }
)


@automation.register_action(
    "m5_voice_pyramid_kit.set_brightness",
    SetBrightnessAction,
    M5_VOICE_PYRAMID_KIT_SET_BRIGHTNESS_SCHEMA,
    synchronous=True,
)
async def set_brightness_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    brightness = await cg.templatable(config[CONF_BRIGHTNESS], args, cg.uint8)
    cg.add(var.set_brightness(brightness))
    return var


M5_VOICE_PYRAMID_KIT_SAVE_BRIGHTNESS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5VoicePyramidKit),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=2),
    }
)


@automation.register_action(
    "m5_voice_pyramid_kit.save_brightness",
    SaveBrightnessAction,
    M5_VOICE_PYRAMID_KIT_SAVE_BRIGHTNESS_SCHEMA,
    synchronous=True,
)
async def save_brightness_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    return var


M5_VOICE_PYRAMID_KIT_SET_TOUCH_POLLING_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(M5VoicePyramidKit),
        cv.Required(CONF_ENABLED): cv.templatable(cv.boolean),
    }
)


@automation.register_action(
    "m5_voice_pyramid_kit.set_touch_polling",
    SetTouchPollingAction,
    M5_VOICE_PYRAMID_KIT_SET_TOUCH_POLLING_SCHEMA,
    synchronous=True,
)
async def set_touch_polling_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    enabled = await cg.templatable(config[CONF_ENABLED], args, cg.bool_)
    cg.add(var.set_enabled(enabled))
    return var
