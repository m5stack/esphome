import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_GYROSCOPE_X,
    CONF_GYROSCOPE_Y,
    CONF_GYROSCOPE_Z,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION_X,
    ICON_ACCELERATION_Y,
    ICON_ACCELERATION_Z,
    ICON_GYROSCOPE_X,
    ICON_GYROSCOPE_Y,
    ICON_GYROSCOPE_Z,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREE_PER_SECOND,
    UNIT_METER_PER_SECOND_SQUARED,
)
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_IMU_ID, M5ChainImu, m5_chain_imu_ns

DEPENDENCIES = ["m5_chain_imu"]

M5ChainImuSensor = m5_chain_imu_ns.class_(
    "M5ChainImuSensor", sensor.Sensor, cg.Parented.template(M5ChainImu)
)


def _accel_schema(icon: str) -> cv.Schema:
    return sensor.sensor_schema(
        M5ChainImuSensor,
        unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
        icon=icon,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _gyro_schema(icon: str) -> cv.Schema:
    return sensor.sensor_schema(
        M5ChainImuSensor,
        unit_of_measurement=UNIT_DEGREE_PER_SECOND,
        icon=icon,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )


SENSOR_KEYS = (
    CONF_ACCELERATION_X,
    CONF_ACCELERATION_Y,
    CONF_ACCELERATION_Z,
    CONF_GYROSCOPE_X,
    CONF_GYROSCOPE_Y,
    CONF_GYROSCOPE_Z,
    CONF_TEMPERATURE,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5_CHAIN_IMU_ID): cv.use_id(M5ChainImu),
            cv.Optional(CONF_ACCELERATION_X): _accel_schema(ICON_ACCELERATION_X),
            cv.Optional(CONF_ACCELERATION_Y): _accel_schema(ICON_ACCELERATION_Y),
            cv.Optional(CONF_ACCELERATION_Z): _accel_schema(ICON_ACCELERATION_Z),
            cv.Optional(CONF_GYROSCOPE_X): _gyro_schema(ICON_GYROSCOPE_X),
            cv.Optional(CONF_GYROSCOPE_Y): _gyro_schema(ICON_GYROSCOPE_Y),
            cv.Optional(CONF_GYROSCOPE_Z): _gyro_schema(ICON_GYROSCOPE_Z),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                M5ChainImuSensor,
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ),
    cv.has_at_least_one_key(*SENSOR_KEYS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_CHAIN_IMU_ID])
    sensors = sensor.sub_sensors(config, parent=hub)
    await sensors(CONF_ACCELERATION_X, hub.set_acceleration_x_sensor)
    await sensors(CONF_ACCELERATION_Y, hub.set_acceleration_y_sensor)
    await sensors(CONF_ACCELERATION_Z, hub.set_acceleration_z_sensor)
    await sensors(CONF_GYROSCOPE_X, hub.set_gyroscope_x_sensor)
    await sensors(CONF_GYROSCOPE_Y, hub.set_gyroscope_y_sensor)
    await sensors(CONF_GYROSCOPE_Z, hub.set_gyroscope_z_sensor)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
