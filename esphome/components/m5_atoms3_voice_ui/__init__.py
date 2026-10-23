import esphome.codegen as cg
from esphome.components import display, globals as globals_component
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
from esphome.core import ID, HexInt
import esphome.final_validate as fv
from esphome.types import ConfigType

DEPENDENCIES = ["display", "esp32"]
CODEOWNERS = ["@m5stack"]
DOMAIN = "m5_atoms3_voice_ui"

CONF_DISPLAY_ID = "display_id"
CONF_VOICE_ASSISTANT_ID = "voice_assistant_id"
CONF_PHASE_ID = "phase_id"
CONF_ANIMATIONS = "animations"
CONF_PHASE_MAPPING = "phase_mapping"
CONF_BACKGROUND_COLOR = "background_color"
CONF_FRAME_DELAY = "frame_delay"

voice_ui_ns = cg.esphome_ns.namespace("m5_atoms3_voice_ui")
M5Atoms3VoiceUI = voice_ui_ns.class_("M5Atoms3VoiceUI", cg.Component)
VoiceAssistant = cg.esphome_ns.namespace("voice_assistant").class_("VoiceAssistant")
GlobalsComponent = globals_component.globals_ns.class_("GlobalsComponent")

PHASE_DEFAULTS = {
    "idle": 1,
    "listening": 2,
    "thinking": 3,
    "replying": 4,
    "loading": 10,
    "error": 11,
    "muted": 12,
    "timer_finished": 20,
}
ANIMATION_KEYS = tuple(PHASE_DEFAULTS)
REQUIRED_ANIMATIONS = ANIMATION_KEYS[:6]
GFX_COMPONENT_REF = "^3.0.0"


def validate_phase_mapping(config: ConfigType) -> ConfigType:
    if len(set(config.values())) != len(config):
        raise cv.Invalid("Each voice assistant phase must have a unique value")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(M5Atoms3VoiceUI),
        cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.Display),
        cv.Optional(CONF_VOICE_ASSISTANT_ID): cv.use_id(VoiceAssistant),
        cv.Required(CONF_PHASE_ID): cv.use_id(GlobalsComponent),
        cv.Optional(CONF_BACKGROUND_COLOR, default=0x10131A): cv.hex_int_range(
            min=0, max=0xFFFFFF
        ),
        cv.Optional(
            CONF_FRAME_DELAY, default="33ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_PHASE_MAPPING, default={}): cv.All(
            cv.Schema(
                {
                    cv.Optional(key, default=value): cv.int_
                    for key, value in PHASE_DEFAULTS.items()
                }
            ),
            validate_phase_mapping,
        ),
        cv.Required(CONF_ANIMATIONS): cv.Schema(
            {
                **{cv.Required(key): cv.file_ for key in REQUIRED_ANIMATIONS},
                cv.Optional("muted"): cv.file_,
                cv.Optional("timer_finished"): cv.file_,
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


def final_validate(config: ConfigType) -> None:
    fv.id_declaration_match_schema({cv.Required(CONF_TYPE): cv.one_of("int")})(
        config[CONF_PHASE_ID]
    )
    if CONF_VOICE_ASSISTANT_ID not in config:
        raise cv.Invalid(
            "voice_assistant_id is required", path=[CONF_VOICE_ASSISTANT_ID]
        )
    metadata = display.get_display_metadata(config[CONF_DISPLAY_ID])
    if metadata.rotation != 0:
        raise cv.Invalid(
            "esp_emote_gfx draws in panel coordinates; set the display rotation to 0",
            path=[CONF_DISPLAY_ID],
        )
    if metadata.has_writer:
        raise cv.Invalid(
            "Remove lambda:, pages:, auto_clear_enabled: true or show_test_card: true "
            "from the display config; esp_emote_gfx owns the panel",
            path=[CONF_DISPLAY_ID],
        )


FINAL_VALIDATE_SCHEMA = final_validate


def eaf_words(data: bytes) -> tuple[list[HexInt], int]:
    """Pack the EAF blob into 32-bit words so the embedded array is 4-byte aligned.

    The ESP32 build embeds these arrays in flash; the decoder performs 32-bit
    loads on the file header, which must be naturally aligned.
    """
    size = len(data)
    padded = data + b"\0" * ((-size) % 4)
    words = [
        HexInt(int.from_bytes(padded[index : index + 4], "little"))
        for index in range(0, len(padded), 4)
    ]
    return words, size


async def to_code(config: ConfigType) -> None:
    from esphome.components.esp32 import add_idf_component

    add_idf_component(name="espressif2022/esp_emote_gfx", ref=GFX_COMPONENT_REF)

    display_component = await cg.get_variable(config[CONF_DISPLAY_ID])
    phase = await cg.get_variable(config[CONF_PHASE_ID])
    assistant = cg.nullptr
    if (assistant_id := config.get(CONF_VOICE_ASSISTANT_ID)) is not None:
        assistant = await cg.get_variable(assistant_id)

    var = cg.new_Pvariable(
        config[CONF_ID],
        display_component,
        cg.RawExpression(f"&{phase}->value()"),
        assistant,
    )
    await cg.register_component(var, config)

    metadata = display.get_display_metadata(config[CONF_DISPLAY_ID])
    byte_order = (
        "big_endian" if metadata.byte_order is cv.UNDEFINED else metadata.byte_order
    )
    cg.add(var.set_big_endian(byte_order == "big_endian"))
    cg.add(var.set_background_color(config[CONF_BACKGROUND_COLOR]))
    frame_delay = config[CONF_FRAME_DELAY].total_milliseconds
    cg.add(var.set_frame_rate(max(1, round(1000 / frame_delay))))

    for index, key in enumerate(ANIMATION_KEYS):
        cg.add(var.set_phase_mapping(index, config[CONF_PHASE_MAPPING][key]))
        if (path := config[CONF_ANIMATIONS].get(key)) is None:
            continue
        words, size = eaf_words(path.read_bytes())
        array_id = ID(
            f"{config[CONF_ID]}_{key}_eaf", is_declaration=True, type=cg.uint32
        )
        array = cg.progmem_array(array_id, words)
        cg.add(var.set_animation_source(index, array, size))
