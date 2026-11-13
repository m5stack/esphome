from dataclasses import dataclass, field

from esphome import automation
import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_CHANNEL,
    CONF_COUNT,
    CONF_ID,
    CONF_INDEX,
    CONF_LEVEL,
    CONF_MODE,
    CONF_REFRESH,
)
from esphome.core import CORE, ID
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

AUTO_LOAD = ["light", "m5_chain_base", "number", "output", "switch"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_unit_8servos2_chain"
MULTI_CONF = True

CONF_ANGLES = "angles"
CONF_DUTY = "duty"
CONF_DUTIES = "duties"
CONF_LEVELS = "levels"
CONF_PULL = "pull"
CONF_M5_UNIT_8SERVOS2_CHAIN_ID = "m5_unit_8servos2_chain_id"

SERVOS2_CHANNEL_COUNT = 8
SERVOS2_RGB_TOTAL_MAX = 16

m5_unit_8servos2_chain_ns = cg.esphome_ns.namespace("m5_unit_8servos2_chain")
M5Unit8Servos2Chain = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2Chain", m5_chain_base.M5ChainBase
)

Servos2Mode = m5_unit_8servos2_chain_ns.enum("Servos2Mode", is_class=True)
Servos2Pull = m5_unit_8servos2_chain_ns.enum("Servos2Pull", is_class=True)

MODES = {
    "input": Servos2Mode.SERVOS2_MODE_INPUT,
    "output": Servos2Mode.SERVOS2_MODE_OUTPUT,
    "adc": Servos2Mode.SERVOS2_MODE_ADC,
    "servo": Servos2Mode.SERVOS2_MODE_SERVO,
    "rgb": Servos2Mode.SERVOS2_MODE_RGB,
    "pwm": Servos2Mode.SERVOS2_MODE_PWM,
}
PULLS = {
    "none": Servos2Pull.SERVOS2_PULL_NONE,
    "up": Servos2Pull.SERVOS2_PULL_UP,
    "down": Servos2Pull.SERVOS2_PULL_DOWN,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5Unit8Servos2Chain),
        }
    )
    .extend(m5_chain_base.BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=115200,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_chain_index(config[m5_chain_base.CONF_CHAIN_INDEX]))
    cg.add(var.set_update_interval(config[m5_chain_base.CONF_UPDATE_INTERVAL]))


@dataclass
class M5Unit8Servos2ChainData:
    """Channel usage across the sub-platforms of one chain hub."""

    channel_functions: dict[tuple[str, int], str] = field(default_factory=dict)
    rgb_totals: dict[str, int] = field(default_factory=dict)
    rgb_lights: list[tuple[str, int, int, MockObj]] = field(default_factory=list)
    rgb_job_added: bool = False


def _get_data() -> M5Unit8Servos2ChainData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = M5Unit8Servos2ChainData()
    return CORE.data[DOMAIN]


def claim_channel(
    hub_id: ID, channel: int, function: str, *, allow_duplicate: bool = False
) -> None:
    """Reject two entities that would drive one channel in different modes.

    Servo and PWM channels accept a value entity next to a control entity (a number
    beside an output). Digital and RGB channels hold one entity each.
    """
    key = (str(hub_id), channel)
    existing = _get_data().channel_functions.get(key)
    if existing is not None:
        if existing != function:
            raise cv.Invalid(
                f"Channel {channel} is already used as '{existing}'; a channel can "
                f"only serve one function at a time"
            )
        if not allow_duplicate:
            raise cv.Invalid(
                f"Channel {channel} already has a '{function}' entity; only one is allowed"
            )
    _get_data().channel_functions[key] = function


def declare_rgb_light(hub_id: ID, channel: int, num_leds: int) -> None:
    """Claim a light channel and reject a layout that needs more than the unit's 16 buffer entries."""
    claim_channel(hub_id, channel, "rgb", allow_duplicate=False)
    data = _get_data()
    key = str(hub_id)
    total = data.rgb_totals.get(key, 0) + num_leds
    if total > SERVOS2_RGB_TOTAL_MAX:
        raise cv.Invalid(
            f"The unit has only {SERVOS2_RGB_TOTAL_MAX} RGB buffer entries; "
            f"the configured lights need {total}"
        )
    data.rgb_totals[key] = total


def register_rgb_light(hub_id: ID, channel: int, num_leds: int, var: MockObj) -> None:
    """Collect the light outputs so the buffers can be given packed offsets at the end."""
    data = _get_data()
    data.rgb_lights.append((str(hub_id), channel, num_leds, var))
    if not data.rgb_job_added:
        data.rgb_job_added = True
        CORE.add_job(_finalize_rgb_offsets)


@coroutine_with_priority(CoroPriority.FINAL)
async def _finalize_rgb_offsets() -> None:
    """Pack each hub's light buffers into the unit's 16-entry global RGB buffer, in channel order."""
    by_hub: dict[str, list[tuple[int, int, MockObj]]] = {}
    for hub_key, channel, num_leds, var in _get_data().rgb_lights:
        by_hub.setdefault(hub_key, []).append((channel, num_leds, var))
    for entries in by_hub.values():
        offset = 0
        for channel, num_leds, var in sorted(entries, key=lambda entry: entry[0]):
            if offset + num_leds > SERVOS2_RGB_TOTAL_MAX:
                raise cv.Invalid(
                    f"m5_unit_8servos2_chain: channel {channel} needs RGB indices up to "
                    f"{offset + num_leds - 1}, but the unit only has {SERVOS2_RGB_TOTAL_MAX}"
                )
            cg.add(var.set_offset(offset))
            offset += num_leds


_ACTION_ID = {cv.GenerateID(): cv.use_id(M5Unit8Servos2Chain)}
_CHANNEL = cv.int_range(min=0, max=SERVOS2_CHANNEL_COUNT - 1)
_ANGLE = cv.int_range(min=0, max=180)
_DUTY = cv.int_range(min=0, max=100)
_BYTE_LIST = cg.RawExpression("std::initializer_list<uint8_t>")
_BOOL_LIST = cg.RawExpression("std::initializer_list<bool>")


def _all(values):
    """Exactly one value per channel."""
    return cv.All([values], cv.Length(min=8, max=8))


automation.register_apply_action(
    f"{DOMAIN}.set_mode",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_CHANNEL): _CHANNEL,
            cv.Required(CONF_MODE): cv.enum(MODES),
        }
    ),
    automation.ApplyCall(
        "set_mode({},{})",
        ((CONF_CHANNEL, cg.uint8), (CONF_MODE, Servos2Mode)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_servo_angle",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_CHANNEL): _CHANNEL,
            cv.Required(CONF_ANGLE): _ANGLE,
        }
    ),
    automation.ApplyCall(
        "set_servo_angle({},{})",
        ((CONF_CHANNEL, cg.uint8), (CONF_ANGLE, cg.uint8)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_servo_angles",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_ANGLES): _all(_ANGLE)}),
    automation.ApplyCall("set_servo_angles({})", ((CONF_ANGLES, _BYTE_LIST),)),
)
automation.register_apply_action(
    f"{DOMAIN}.set_pwm_duty",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_CHANNEL): _CHANNEL,
            cv.Required(CONF_DUTY): _DUTY,
        }
    ),
    automation.ApplyCall(
        "set_pwm_duty({},{})",
        ((CONF_CHANNEL, cg.uint8), (CONF_DUTY, cg.uint8)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_pwm_duties",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_DUTIES): _all(_DUTY)}),
    automation.ApplyCall("set_pwm_duties({})", ((CONF_DUTIES, _BYTE_LIST),)),
)
automation.register_apply_action(
    f"{DOMAIN}.set_output_level",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_CHANNEL): _CHANNEL,
            cv.Required(CONF_LEVEL): cv.boolean,
        }
    ),
    automation.ApplyCall(
        "set_output_level({},{})",
        ((CONF_CHANNEL, cg.uint8), (CONF_LEVEL, cg.bool_)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_output_levels",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_LEVELS): _all(cv.boolean)}),
    automation.ApplyCall("set_output_levels({})", ((CONF_LEVELS, _BOOL_LIST),)),
)
automation.register_apply_action(
    f"{DOMAIN}.set_input_pull",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_CHANNEL): _CHANNEL,
            cv.Required(CONF_PULL): cv.enum(PULLS),
        }
    ),
    automation.ApplyCall(
        "set_input_pull({},{})",
        ((CONF_CHANNEL, cg.uint8), (CONF_PULL, Servos2Pull)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_rgb_config",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_CHANNEL): _CHANNEL,
            cv.Required(CONF_COUNT): cv.int_range(min=1, max=16),
            cv.Optional(CONF_REFRESH, default=True): cv.boolean,
        }
    ),
    automation.ApplyCall(
        "set_rgb_config({},{},{})",
        ((CONF_CHANNEL, cg.uint8), (CONF_COUNT, cg.uint8), (CONF_REFRESH, cg.bool_)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_rgb",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_INDEX): cv.int_range(min=0, max=15),
            cv.Required("red"): cv.int_range(min=0, max=255),
            cv.Required("green"): cv.int_range(min=0, max=255),
            cv.Required("blue"): cv.int_range(min=0, max=255),
        }
    ),
    automation.ApplyCall(
        "set_rgb({},{},{},{})",
        (
            (CONF_INDEX, cg.uint8),
            ("red", cg.uint8),
            ("green", cg.uint8),
            ("blue", cg.uint8),
        ),
    ),
)
