import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import STATE_CLASS_MEASUREMENT, UNIT_DEGREES
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_PITCH, CONF_STACKCHAN_MOTION_ID, CONF_YAW

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["stackchan_motion"]

CONF_DIRECTION_X = "direction_x"
CONF_DIRECTION_Y = "direction_y"
CONF_DIRECTION_Z = "direction_z"

YAW_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon="mdi:rotate-orbit",
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
PITCH_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon="mdi:angle-acute",
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
DIRECTION_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=3,
    state_class=STATE_CLASS_MEASUREMENT,
)


def direction_schema(icon: str) -> cv.Schema:
    return DIRECTION_SCHEMA.extend({cv.Optional("icon", default=icon): cv.icon})


CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_YAW): YAW_SCHEMA,
        cv.Optional(CONF_PITCH): PITCH_SCHEMA,
        cv.Optional(CONF_DIRECTION_X): direction_schema("mdi:axis-x-arrow"),
        cv.Optional(CONF_DIRECTION_Y): direction_schema("mdi:axis-y-arrow"),
        cv.Optional(CONF_DIRECTION_Z): direction_schema("mdi:axis-z-arrow"),
    }
).extend(BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    motion = await cg.get_variable(config[CONF_STACKCHAN_MOTION_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_YAW, motion.set_yaw_sensor)
    await sensors(CONF_PITCH, motion.set_pitch_sensor)
    await sensors(CONF_DIRECTION_X, motion.set_direction_x_sensor)
    await sensors(CONF_DIRECTION_Y, motion.set_direction_y_sensor)
    await sensors(CONF_DIRECTION_Z, motion.set_direction_z_sensor)
