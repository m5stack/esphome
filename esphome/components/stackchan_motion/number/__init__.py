import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_MAX_VALUE, CONF_MIN_VALUE, CONF_STEP, UNIT_DEGREES
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_PITCH, CONF_STACKCHAN_MOTION_ID, CONF_YAW

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["stackchan_motion"]


def angle_schema(icon: str, default_min: float, default_max: float) -> cv.Schema:
    return number.number_schema(
        number.ApplyNumber,
        icon=icon,
        unit_of_measurement=UNIT_DEGREES,
    ).extend(
        {
            cv.Optional(CONF_MIN_VALUE, default=default_min): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=default_max): cv.float_,
            cv.Optional(CONF_STEP, default=1.0): cv.positive_not_null_float,
        }
    )


def validate_value_range(config: ConfigType) -> ConfigType:
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid(f"'{CONF_MAX_VALUE}' must be greater than '{CONF_MIN_VALUE}'")
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_YAW): cv.All(
            angle_schema("mdi:rotate-orbit", -90.0, 90.0), validate_value_range
        ),
        cv.Optional(CONF_PITCH): cv.All(
            angle_schema("mdi:angle-acute", -45.0, 45.0), validate_value_range
        ),
    }
).extend(BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    motion = await cg.get_variable(config[CONF_STACKCHAN_MOTION_ID])
    numbers = number.sub_numbers(config, parent=motion)

    if (conf := config.get(CONF_YAW)) is not None:
        await numbers(
            CONF_YAW,
            motion.set_yaw_number,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
            apply="set_target_yaw(value)",
        )
    if (conf := config.get(CONF_PITCH)) is not None:
        await numbers(
            CONF_PITCH,
            motion.set_pitch_number,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
            apply="set_target_pitch(value)",
        )
