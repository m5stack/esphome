from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import ftservo
from esphome.components.ftservo import CONF_DEFAULT_SPEED, CONF_DEFAULT_TIME
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_SPEED, CONF_TIME, CONF_X, CONF_Y
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["ftservo"]
DOMAIN = "stackchan_motion"
MULTI_CONF = True

CONF_STACKCHAN_MOTION_ID = "stackchan_motion_id"
CONF_YAW_SERVO = "yaw_servo"
CONF_PITCH_SERVO = "pitch_servo"
CONF_MIN_YAW = "min_yaw"
CONF_MAX_YAW = "max_yaw"
CONF_MIN_PITCH = "min_pitch"
CONF_MAX_PITCH = "max_pitch"
CONF_HOME_YAW = "home_yaw"
CONF_HOME_PITCH = "home_pitch"
CONF_NUDGE_STEP = "nudge_step"
CONF_YAW = "yaw"
CONF_PITCH = "pitch"
CONF_Z = "z"

stackchan_motion_ns = cg.esphome_ns.namespace("stackchan_motion")
StackchanMotion = stackchan_motion_ns.class_("StackchanMotion", cg.Component)

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_STACKCHAN_MOTION_ID): cv.use_id(StackchanMotion),
    }
)


def validate_ranges(config: ConfigType) -> ConfigType:
    for low, high in ((CONF_MIN_YAW, CONF_MAX_YAW), (CONF_MIN_PITCH, CONF_MAX_PITCH)):
        if config[low] >= config[high]:
            raise cv.Invalid(f"'{low}' must be smaller than '{high}'")
    for key, low, high in (
        (CONF_HOME_YAW, CONF_MIN_YAW, CONF_MAX_YAW),
        (CONF_HOME_PITCH, CONF_MIN_PITCH, CONF_MAX_PITCH),
    ):
        if not config[low] <= config[key] <= config[high]:
            raise cv.Invalid(f"'{key}' must be within '{low}' and '{high}'")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(StackchanMotion),
            cv.Required(CONF_YAW_SERVO): cv.use_id(ftservo.FTServo),
            cv.Required(CONF_PITCH_SERVO): cv.use_id(ftservo.FTServo),
            cv.Optional(CONF_MIN_YAW, default=-90.0): cv.float_,
            cv.Optional(CONF_MAX_YAW, default=90.0): cv.float_,
            cv.Optional(CONF_MIN_PITCH, default=-45.0): cv.float_,
            cv.Optional(CONF_MAX_PITCH, default=45.0): cv.float_,
            cv.Optional(CONF_HOME_YAW, default=0.0): cv.float_,
            cv.Optional(CONF_HOME_PITCH, default=0.0): cv.float_,
            cv.Optional(CONF_NUDGE_STEP, default=10.0): cv.positive_not_null_float,
            cv.Optional(CONF_DEFAULT_SPEED, default=0): cv.uint16_t,
            cv.Optional(CONF_DEFAULT_TIME, default=0): cv.uint16_t,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_ranges,
)

LOOK_AT_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(StackchanMotion),
        cv.Required(CONF_YAW): cv.templatable(cv.float_),
        cv.Required(CONF_PITCH): cv.templatable(cv.float_),
        cv.Optional(CONF_SPEED, default=0): cv.templatable(cv.uint16_t),
        cv.Optional(CONF_TIME, default=0): cv.templatable(cv.uint16_t),
    }
)

LOOK_DIRECTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(StackchanMotion),
        cv.Required(CONF_X): cv.templatable(cv.float_),
        cv.Required(CONF_Y): cv.templatable(cv.float_),
        cv.Required(CONF_Z): cv.templatable(cv.float_),
        cv.Optional(CONF_SPEED, default=0): cv.templatable(cv.uint16_t),
        cv.Optional(CONF_TIME, default=0): cv.templatable(cv.uint16_t),
    }
)

NUDGE_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(StackchanMotion),
        cv.Required(CONF_YAW): cv.templatable(cv.float_),
        cv.Required(CONF_PITCH): cv.templatable(cv.float_),
    }
)

MOTION_ID_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(StackchanMotion),
    }
)

automation.register_apply_action(
    "stackchan_motion.look_at",
    LOOK_AT_SCHEMA,
    automation.ApplyCall(
        "look_at({}, {}, {}, {})",
        (
            (CONF_YAW, cg.float_),
            (CONF_PITCH, cg.float_),
            (CONF_SPEED, cg.uint16),
            (CONF_TIME, cg.uint16),
        ),
    ),
)

automation.register_apply_action(
    "stackchan_motion.look_direction",
    LOOK_DIRECTION_SCHEMA,
    automation.ApplyCall(
        "look_direction({}, {}, {}, {}, {})",
        (
            (CONF_X, cg.float_),
            (CONF_Y, cg.float_),
            (CONF_Z, cg.float_),
            (CONF_SPEED, cg.uint16),
            (CONF_TIME, cg.uint16),
        ),
    ),
)

automation.register_apply_action(
    "stackchan_motion.nudge",
    NUDGE_SCHEMA,
    automation.ApplyCall(
        "nudge({}, {})",
        (
            (CONF_YAW, cg.float_),
            (CONF_PITCH, cg.float_),
        ),
    ),
)

automation.register_apply_action(
    "stackchan_motion.home",
    MOTION_ID_SCHEMA,
    automation.ApplyCall("home()"),
)

automation.register_apply_action(
    "stackchan_motion.stop",
    MOTION_ID_SCHEMA,
    automation.ApplyCall("stop()"),
)

automation.register_apply_condition(
    "stackchan_motion.is_moving",
    MOTION_ID_SCHEMA,
    "is_moving()",
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_yaw_servo(await cg.get_variable(config[CONF_YAW_SERVO])))
    cg.add(var.set_pitch_servo(await cg.get_variable(config[CONF_PITCH_SERVO])))
    cg.add(var.set_min_yaw(config[CONF_MIN_YAW]))
    cg.add(var.set_max_yaw(config[CONF_MAX_YAW]))
    cg.add(var.set_min_pitch(config[CONF_MIN_PITCH]))
    cg.add(var.set_max_pitch(config[CONF_MAX_PITCH]))
    cg.add(var.set_home_yaw(config[CONF_HOME_YAW]))
    cg.add(var.set_home_pitch(config[CONF_HOME_PITCH]))
    cg.add(var.set_nudge_step(config[CONF_NUDGE_STEP]))
    cg.add(var.set_default_speed(config[CONF_DEFAULT_SPEED]))
    cg.add(var.set_default_time(config[CONF_DEFAULT_TIME]))
