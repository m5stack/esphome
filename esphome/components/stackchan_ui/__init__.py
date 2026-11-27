from esphome import automation
import esphome.codegen as cg
from esphome.components import font
from esphome.components.lvgl.defines import add_define, add_lv_use
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID, CONF_TEXT
from esphome.core import ID
from esphome.cpp_generator import MockObj, TemplateArguments
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["lvgl", "font"]
AUTO_LOAD = ["lvgl"]
DOMAIN = "stackchan_ui"

CONF_FONT_ID = "font_id"
CONF_PRIMARY_COLOR = "primary_color"
CONF_SECONDARY_COLOR = "secondary_color"
CONF_PANEL_WIDTH = "panel_width"
CONF_PANEL_HEIGHT = "panel_height"
CONF_ENABLE_BLINK = "enable_blink"
CONF_ENABLE_BREATH = "enable_breath"
CONF_ENABLE_RANDOM = "enable_random"
CONF_BLINK_OPEN_INTERVAL = "blink_open_interval"
CONF_BLINK_CLOSE_INTERVAL = "blink_close_interval"
CONF_BREATH_CYCLE = "breath_cycle"
CONF_BREATH_UPDATE_INTERVAL = "breath_update_interval"
CONF_BREATH_AMPLITUDE = "breath_amplitude"
CONF_RANDOM_INTERVAL_MIN = "random_interval_min"
CONF_RANDOM_INTERVAL_MAX = "random_interval_max"
CONF_HEAD_PET_MIN = "head_pet_min"
CONF_HEAD_PET_MAX = "head_pet_max"
CONF_HEAD_PET_RESTORE_DELAY = "head_pet_restore_delay"
CONF_SHAKE_DURATION = "shake_duration"
CONF_SPEAKING_INTERVAL = "speaking_interval"
CONF_EMOTION = "emotion"
CONF_SPEAK = "speak"
CONF_ON_HEAD_PET = "on_head_pet"
CONF_ON_HEAD_PET_RESTORE = "on_head_pet_restore"
CONF_ON_SHAKE = "on_shake"

stackchan_ui_ns = cg.esphome_ns.namespace("stackchan_ui")
StackChanUI = stackchan_ui_ns.class_("StackChanUI", cg.Component)
SetEmotionAction = stackchan_ui_ns.class_("SetEmotionAction", automation.Action)
SetSpeechAction = stackchan_ui_ns.class_("SetSpeechAction", automation.Action)


def validate_intervals(config: ConfigType) -> ConfigType:
    if config[CONF_RANDOM_INTERVAL_MIN] > config[CONF_RANDOM_INTERVAL_MAX]:
        raise cv.Invalid(
            f"'{CONF_RANDOM_INTERVAL_MIN}' must not be greater than '{CONF_RANDOM_INTERVAL_MAX}'"
        )
    if config[CONF_HEAD_PET_MIN] > config[CONF_HEAD_PET_MAX]:
        raise cv.Invalid(
            f"'{CONF_HEAD_PET_MIN}' must not be greater than '{CONF_HEAD_PET_MAX}'"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(StackChanUI),
            cv.Required(CONF_FONT_ID): cv.use_id(font.Font),
            cv.Optional(CONF_PRIMARY_COLOR, default=0xFFFFFF): cv.hex_int_range(
                min=0, max=0xFFFFFF
            ),
            cv.Optional(CONF_SECONDARY_COLOR, default=0x000000): cv.hex_int_range(
                min=0, max=0xFFFFFF
            ),
            cv.Optional(CONF_PANEL_WIDTH, default=320): cv.int_range(min=1, max=4096),
            cv.Optional(CONF_PANEL_HEIGHT, default=240): cv.int_range(min=1, max=4096),
            cv.Optional(CONF_ENABLE_BLINK, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_BREATH, default=True): cv.boolean,
            cv.Optional(CONF_ENABLE_RANDOM, default=True): cv.boolean,
            cv.Optional(
                CONF_BLINK_OPEN_INTERVAL, default="5200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_BLINK_CLOSE_INTERVAL, default="200ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_BREATH_CYCLE, default="6600ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_BREATH_UPDATE_INTERVAL, default="600ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_BREATH_AMPLITUDE, default=16): cv.int_range(
                min=0, max=100
            ),
            cv.Optional(
                CONF_RANDOM_INTERVAL_MIN, default="2000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_RANDOM_INTERVAL_MAX, default="6000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_HEAD_PET_MIN, default="1500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_HEAD_PET_MAX, default="2500ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_HEAD_PET_RESTORE_DELAY, default="3000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SHAKE_DURATION, default="4000ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_SPEAKING_INTERVAL, default="180ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ON_HEAD_PET): automation.validate_automation({}),
            cv.Optional(CONF_ON_HEAD_PET_RESTORE): automation.validate_automation({}),
            cv.Optional(CONF_ON_SHAKE): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_intervals,
)


def final_validate(config: ConfigType) -> None:
    # These widgets and formats are used directly from C++, so the LVGL YAML
    # component cannot discover them from a widget declaration.
    add_lv_use("label", "image", "font")
    add_define("LV_DRAW_SW_SUPPORT_RGB565A8", "1")
    # Transform styles (rotation/scale) need the transparent screen path.
    add_define("LV_COLOR_SCREEN_TRANSP", "1")


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_font(await cg.get_variable(config[CONF_FONT_ID])))
    cg.add(var.set_primary_color(config[CONF_PRIMARY_COLOR]))
    cg.add(var.set_secondary_color(config[CONF_SECONDARY_COLOR]))
    cg.add(var.set_panel_size(config[CONF_PANEL_WIDTH], config[CONF_PANEL_HEIGHT]))
    cg.add(var.set_enable_blink(config[CONF_ENABLE_BLINK]))
    cg.add(var.set_enable_breath(config[CONF_ENABLE_BREATH]))
    cg.add(var.set_enable_random(config[CONF_ENABLE_RANDOM]))
    cg.add(
        var.set_blink_open_interval(config[CONF_BLINK_OPEN_INTERVAL].total_milliseconds)
    )
    cg.add(
        var.set_blink_close_interval(
            config[CONF_BLINK_CLOSE_INTERVAL].total_milliseconds
        )
    )
    cg.add(var.set_breath_cycle(config[CONF_BREATH_CYCLE].total_milliseconds))
    cg.add(
        var.set_breath_update_interval(
            config[CONF_BREATH_UPDATE_INTERVAL].total_milliseconds
        )
    )
    cg.add(var.set_breath_amplitude(config[CONF_BREATH_AMPLITUDE]))
    cg.add(
        var.set_random_interval(
            config[CONF_RANDOM_INTERVAL_MIN].total_milliseconds,
            config[CONF_RANDOM_INTERVAL_MAX].total_milliseconds,
        )
    )
    cg.add(
        var.set_head_pet_timing(
            config[CONF_HEAD_PET_MIN].total_milliseconds,
            config[CONF_HEAD_PET_MAX].total_milliseconds,
            config[CONF_HEAD_PET_RESTORE_DELAY].total_milliseconds,
        )
    )
    cg.add(var.set_shake_duration(config[CONF_SHAKE_DURATION].total_milliseconds))
    cg.add(var.set_speaking_interval(config[CONF_SPEAKING_INTERVAL].total_milliseconds))

    for conf in config.get(CONF_ON_HEAD_PET, []):
        await automation.build_callback_automation(
            var, "add_on_head_pet_callback", [], conf
        )
    for conf in config.get(CONF_ON_HEAD_PET_RESTORE, []):
        await automation.build_callback_automation(
            var, "add_on_head_pet_restore_callback", [], conf
        )
    for conf in config.get(CONF_ON_SHAKE, []):
        await automation.build_callback_automation(
            var, "add_on_shake_callback", [], conf
        )


SET_EMOTION_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(StackChanUI),
        cv.Required(CONF_EMOTION): cv.templatable(cv.string),
    },
    key=CONF_EMOTION,
)


@automation.register_action(
    "stackchan_ui.set_emotion",
    SetEmotionAction,
    SET_EMOTION_ACTION_SCHEMA,
    synchronous=True,
)
async def set_emotion_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: list,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(
        var.set_emotion(await cg.templatable(config[CONF_EMOTION], args, cg.std_string))
    )
    return var


SET_SPEECH_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(StackChanUI),
        cv.Required(CONF_TEXT): cv.templatable(cv.string),
        cv.Optional(CONF_DURATION, default="0ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SPEAK, default=False): cv.boolean,
    },
    key=CONF_TEXT,
)


@automation.register_action(
    "stackchan_ui.set_speech",
    SetSpeechAction,
    SET_SPEECH_ACTION_SCHEMA,
    synchronous=True,
)
async def set_speech_action_to_code(
    config: ConfigType,
    action_id: ID,
    template_arg: TemplateArguments,
    args: list,
) -> MockObj:
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_text(await cg.templatable(config[CONF_TEXT], args, cg.std_string)))
    cg.add(var.set_duration(config[CONF_DURATION].total_milliseconds))
    cg.add(var.set_speak(config[CONF_SPEAK]))
    return var


for _name, _call in (
    ("stackchan_ui.clear_speech", "clear_speech()"),
    ("stackchan_ui.head_pet", "head_pet()"),
    ("stackchan_ui.head_pet_hold", "head_pet_hold()"),
    ("stackchan_ui.head_pet_release", "head_pet_release()"),
    ("stackchan_ui.shake", "shake()"),
    ("stackchan_ui.start_speaking", "start_speaking()"),
    ("stackchan_ui.stop_speaking", "stop_speaking()"),
):
    automation.register_apply_action(
        _name,
        automation.maybe_simple_id({cv.GenerateID(): cv.use_id(StackChanUI)}),
        automation.ApplyCall(_call),
    )
