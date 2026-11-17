import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ALTITUDE,
    CONF_HUMIDITY,
    CONF_PRESSURE,
    CONF_TEMPERATURE,
    DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HECTOPASCAL,
    UNIT_METER,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_ENV_ID, M5ChainEnv, m5_chain_env_ns

DEPENDENCIES = ["m5_chain_env"]

M5ChainEnvSensor = m5_chain_env_ns.class_(
    "M5ChainEnvSensor", sensor.Sensor, cg.Parented.template(M5ChainEnv)
)

CONF_BAROMETER_TEMPERATURE = "barometer_temperature"

_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    M5ChainEnvSensor,
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5_CHAIN_ENV_ID): cv.use_id(M5ChainEnv),
            # AHT20 readings
            cv.Optional(CONF_TEMPERATURE): _TEMPERATURE_SCHEMA,
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                M5ChainEnvSensor,
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            # SPA06 readings
            cv.Optional(CONF_PRESSURE): sensor.sensor_schema(
                M5ChainEnvSensor,
                unit_of_measurement=UNIT_HECTOPASCAL,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ATMOSPHERIC_PRESSURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
                M5ChainEnvSensor,
                unit_of_measurement=UNIT_METER,
                icon="mdi:altimeter",
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_BAROMETER_TEMPERATURE): _TEMPERATURE_SCHEMA,
        }
    ),
    cv.has_at_least_one_key(
        CONF_TEMPERATURE,
        CONF_HUMIDITY,
        CONF_PRESSURE,
        CONF_ALTITUDE,
        CONF_BAROMETER_TEMPERATURE,
    ),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_CHAIN_ENV_ID])
    sensors = sensor.sub_sensors(config, parent=hub)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, hub.set_humidity_sensor)
    await sensors(CONF_PRESSURE, hub.set_pressure_sensor)
    await sensors(CONF_ALTITUDE, hub.set_altitude_sensor)
    await sensors(CONF_BAROMETER_TEMPERATURE, hub.set_barometer_temperature_sensor)
