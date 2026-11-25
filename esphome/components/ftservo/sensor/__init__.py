import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT,
    CONF_POSITION,
    CONF_SPEED,
    CONF_TEMPERATURE,
    CONF_VOLTAGE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_STEPS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import CONF_FTSERVO_ID, FTSERVO_BASE_SCHEMA

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["ftservo"]

CONF_LOAD = "load"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_POSITION): sensor.sensor_schema(
            unit_of_measurement=UNIT_STEPS,
            icon="mdi:axis-arrow",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement="steps/s",
            icon="mdi:speedometer",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LOAD): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            icon="mdi:weight-gram",
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CURRENT): sensor.sensor_schema(
            icon="mdi:current-dc",
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
).extend(FTSERVO_BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    servo = await cg.get_variable(config[CONF_FTSERVO_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_POSITION, servo.set_position_sensor)
    await sensors(CONF_SPEED, servo.set_speed_sensor)
    await sensors(CONF_LOAD, servo.set_load_sensor)
    await sensors(CONF_VOLTAGE, servo.set_voltage_sensor)
    await sensors(CONF_CURRENT, servo.set_current_sensor)
    await sensors(CONF_TEMPERATURE, servo.set_temperature_sensor)
