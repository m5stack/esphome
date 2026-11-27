import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANGLE,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_POSITION,
    CONF_SPEED,
    CONF_STEP,
    CONF_TIME,
    UNIT_DEGREES,
    UNIT_STEPS,
)
from esphome.types import ConfigType

from .. import CONF_FTSERVO_ID, FTSERVO_BASE_SCHEMA

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["ftservo"]

CONF_FOLLOW_FEEDBACK = "follow_feedback"

POSITION_SCHEMA = number.number_schema(
    number.ApplyNumber,
    icon="mdi:gamepad",
    unit_of_measurement=UNIT_STEPS,
).extend(
    {
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=1023.0): cv.float_,
        cv.Optional(CONF_STEP, default=10.0): cv.positive_not_null_float,
    }
)

ANGLE_SCHEMA = number.number_schema(
    number.ApplyNumber,
    icon="mdi:angle-acute",
    unit_of_measurement=UNIT_DEGREES,
).extend(
    {
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=360.0): cv.float_,
        cv.Optional(CONF_STEP, default=1.0): cv.positive_not_null_float,
        cv.Optional(CONF_FOLLOW_FEEDBACK, default=True): cv.boolean,
    }
)

SPEED_SCHEMA = number.number_schema(
    number.ApplyNumber,
    icon="mdi:speedometer",
    unit_of_measurement="steps/s",
).extend(
    {
        cv.Optional(CONF_MIN_VALUE, default=1.0): cv.positive_not_null_float,
        cv.Optional(CONF_MAX_VALUE, default=1000.0): cv.positive_not_null_float,
        cv.Optional(CONF_STEP, default=10.0): cv.positive_not_null_float,
    }
)

TIME_SCHEMA = number.number_schema(
    number.ApplyNumber,
    icon="mdi:timer-outline",
    unit_of_measurement="ms",
).extend(
    {
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.positive_float,
        cv.Optional(CONF_MAX_VALUE, default=9999.0): cv.positive_not_null_float,
        cv.Optional(CONF_STEP, default=100.0): cv.positive_not_null_float,
    }
)


def validate_value_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid(f"'{CONF_MAX_VALUE}' must be greater than '{CONF_MIN_VALUE}'")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POSITION): cv.All(POSITION_SCHEMA, validate_value_range),
        cv.Optional(CONF_ANGLE): cv.All(ANGLE_SCHEMA, validate_value_range),
        cv.Optional(CONF_SPEED): cv.All(SPEED_SCHEMA, validate_value_range),
        cv.Optional(CONF_TIME): cv.All(TIME_SCHEMA, validate_value_range),
    }
).extend(FTSERVO_BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    servo = await cg.get_variable(config[CONF_FTSERVO_ID])
    numbers = number.sub_numbers(config, parent=servo)

    if (conf := config.get(CONF_POSITION)) is not None:
        await numbers(
            CONF_POSITION,
            servo.set_position_number,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
            apply="move_to(value)",
            publish="on_success",
        )
    if (conf := config.get(CONF_ANGLE)) is not None:
        await numbers(
            CONF_ANGLE,
            servo.set_angle_number,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
            apply="move_to_angle(value)",
        )
        cg.add(servo.set_angle_follow_feedback(conf[CONF_FOLLOW_FEEDBACK]))
    if (conf := config.get(CONF_SPEED)) is not None:
        await numbers(
            CONF_SPEED,
            None,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
            apply="set_default_speed(value)",
            publish="after",
        )
    if (conf := config.get(CONF_TIME)) is not None:
        await numbers(
            CONF_TIME,
            None,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
            apply="set_default_time(value)",
            publish="after",
        )
