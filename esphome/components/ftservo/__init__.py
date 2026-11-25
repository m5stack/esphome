from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.const import CONF_ENABLED
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_INVERTED,
    CONF_MAX_SPEED,
    CONF_ON_ERROR,
    CONF_POSITION,
    CONF_RANGE,
    CONF_SPEED,
    CONF_TIME,
    CONF_UPDATE_INTERVAL,
)
from esphome.core import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DOMAIN = "ftservo"
IS_PLATFORM_COMPONENT = True

CONF_FTSERVO_ID = "ftservo_id"  # The servo instance an entity or automation refers to
CONF_DEFAULT_SPEED = "default_speed"
CONF_DEFAULT_TIME = "default_time"
CONF_MAX_POSITION = "max_position"
CONF_MAX_TIME = "max_time"
CONF_MIN_POSITION = "min_position"
CONF_ON_MOVE = "on_move"
CONF_ON_MOVE_DONE = "on_move_done"
CONF_RELEASE_AFTER_MOVE = "release_after_move"
CONF_ZERO_POSITION = "zero_position"

# Feedback fields a servo model can provide, mirrored from ftservo.h
CAPABILITY_POSITION = 1 << 0
CAPABILITY_SPEED = 1 << 1
CAPABILITY_LOAD = 1 << 2
CAPABILITY_VOLTAGE = 1 << 3
CAPABILITY_TEMPERATURE = 1 << 4
CAPABILITY_CURRENT = 1 << 5
CAPABILITY_MOVING = 1 << 6

CAPABILITIES_DEFAULT = (
    CAPABILITY_POSITION
    | CAPABILITY_SPEED
    | CAPABILITY_LOAD
    | CAPABILITY_VOLTAGE
    | CAPABILITY_TEMPERATURE
    | CAPABILITY_MOVING
)

ftservo_ns = cg.esphome_ns.namespace("ftservo")
FTServo = ftservo_ns.class_("FTServo", cg.Component)


def validate_servo_config(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_POSITION] >= config[CONF_MAX_POSITION]:
        raise cv.Invalid(
            f"'{CONF_MIN_POSITION}' must be smaller than '{CONF_MAX_POSITION}'"
        )
    if config[CONF_DEFAULT_SPEED] > config[CONF_MAX_SPEED]:
        raise cv.Invalid(
            f"'{CONF_DEFAULT_SPEED}' must not be greater than '{CONF_MAX_SPEED}'"
        )
    if config[CONF_DEFAULT_TIME] > config[CONF_MAX_TIME]:
        raise cv.Invalid(
            f"'{CONF_DEFAULT_TIME}' must not be greater than '{CONF_MAX_TIME}'"
        )
    return config


SERVO_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_RANGE, default=360.0): cv.float_range(min=1.0, max=3600.0),
        cv.Optional(CONF_ZERO_POSITION, default=0): cv.int_range(min=0, max=1023),
        cv.Optional(CONF_MIN_POSITION, default=0): cv.int_range(min=0, max=1023),
        cv.Optional(CONF_MAX_POSITION, default=1023): cv.int_range(min=0, max=1023),
        cv.Optional(CONF_MAX_SPEED, default=1000): cv.int_range(min=1, max=65535),
        cv.Optional(CONF_MAX_TIME, default=9999): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_DEFAULT_SPEED, default=500): cv.int_range(min=1, max=65535),
        cv.Optional(CONF_DEFAULT_TIME, default=0): cv.int_range(min=0, max=65535),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.Optional(CONF_RELEASE_AFTER_MOVE, default=False): cv.boolean,
        cv.Optional(
            CONF_UPDATE_INTERVAL, default="200ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ON_MOVE): automation.validate_automation({}),
        cv.Optional(CONF_ON_MOVE_DONE): automation.validate_automation({}),
        cv.Optional(CONF_ON_ERROR): automation.validate_automation({}),
    }
)
SERVO_SCHEMA.add_extra(validate_servo_config)

FTSERVO_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FTSERVO_ID): cv.use_id(FTServo),
    }
)


async def register_ftservo(
    var: MockObj, config: ConfigType, capabilities: int = CAPABILITIES_DEFAULT
) -> None:
    """Apply the shared servo options and automations to a concrete servo instance."""
    cg.add(var.set_range_degrees(config[CONF_RANGE]))
    cg.add(var.set_zero_position(config[CONF_ZERO_POSITION]))
    cg.add(var.set_min_position(config[CONF_MIN_POSITION]))
    cg.add(var.set_max_position(config[CONF_MAX_POSITION]))
    cg.add(var.set_max_speed(config[CONF_MAX_SPEED]))
    cg.add(var.set_max_time(config[CONF_MAX_TIME]))
    cg.add(var.set_default_speed(config[CONF_DEFAULT_SPEED]))
    cg.add(var.set_default_time(config[CONF_DEFAULT_TIME]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_release_after_move(config[CONF_RELEASE_AFTER_MOVE]))
    cg.add(var.set_capabilities(capabilities))

    for conf in config.get(CONF_ON_MOVE, []):
        await automation.build_callback_automation(
            var, "add_on_move_callback", [], conf
        )
    for conf in config.get(CONF_ON_MOVE_DONE, []):
        await automation.build_callback_automation(
            var, "add_on_move_done_callback", [], conf
        )
    for conf in config.get(CONF_ON_ERROR, []):
        await automation.build_callback_automation(
            var, "add_on_error_callback", [(cg.uint8, "code")], conf
        )


FTSERVO_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FTServo),
        cv.Required(CONF_POSITION): cv.templatable(cv.int_),
        cv.Optional(CONF_SPEED, default=0): cv.templatable(cv.positive_int),
        cv.Optional(CONF_TIME, default=0): cv.templatable(cv.positive_int),
    }
)

FTSERVO_ANGLE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FTServo),
        cv.Required(CONF_ANGLE): cv.templatable(cv.float_),
        cv.Optional(CONF_SPEED, default=0): cv.templatable(cv.positive_int),
        cv.Optional(CONF_TIME, default=0): cv.templatable(cv.positive_int),
    }
)

FTSERVO_TORQUE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FTServo),
        cv.Required(CONF_ENABLED): cv.templatable(cv.boolean),
    }
)

FTSERVO_ID_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(FTServo),
    }
)

automation.register_apply_action(
    "ftservo.move",
    FTSERVO_ACTION_SCHEMA,
    automation.ApplyCall(
        "move_to({}, {}, {})",
        (
            (CONF_POSITION, cg.int_),
            (CONF_SPEED, cg.uint16),
            (CONF_TIME, cg.uint16),
        ),
    ),
)

automation.register_apply_action(
    "ftservo.move_angle",
    FTSERVO_ANGLE_ACTION_SCHEMA,
    automation.ApplyCall(
        "move_to_angle({}, {}, {})",
        (
            (CONF_ANGLE, cg.float_),
            (CONF_SPEED, cg.uint16),
            (CONF_TIME, cg.uint16),
        ),
    ),
)

automation.register_apply_action(
    "ftservo.set_torque",
    FTSERVO_TORQUE_ACTION_SCHEMA,
    automation.ApplyCall("set_torque({})", ((CONF_ENABLED, cg.bool_),)),
)

automation.register_apply_action(
    "ftservo.stop",
    FTSERVO_ID_SCHEMA,
    automation.ApplyCall("stop()"),
)

automation.register_apply_condition(
    "ftservo.is_moving",
    FTSERVO_ID_SCHEMA,
    "is_moving()",
)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(ftservo_ns.using)
