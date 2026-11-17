import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_HIGH, CONF_LOW, CONF_SOURCE, DEVICE_CLASS_PROBLEM
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_ENV_ID, M5ChainEnv, m5_chain_env_ns

DEPENDENCIES = ["m5_chain_env"]

M5ChainEnvBinarySensor = m5_chain_env_ns.class_(
    "M5ChainEnvBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainEnv),
)

CONF_TEMPERATURE_ALARM = "temperature_alarm"
CONF_HUMIDITY_ALARM = "humidity_alarm"
CONF_PRESSURE_ALARM = "pressure_alarm"
CONF_ALTITUDE_ALARM = "altitude_alarm"

TEMPERATURE_SOURCES = {"aht20": False, "spa06": True}


def _validate_low_below_high(config: ConfigType) -> ConfigType:
    if config[CONF_LOW] >= config[CONF_HIGH]:
        raise cv.Invalid(f"'{CONF_LOW}' must be below '{CONF_HIGH}'")
    return config


def _alarm_schema(icon: str, low_high: dict) -> cv.Schema:
    """An alarm that the device raises when the reading leaves the low to high band."""
    return cv.All(
        binary_sensor.binary_sensor_schema(
            M5ChainEnvBinarySensor, device_class=DEVICE_CLASS_PROBLEM, icon=icon
        ).extend(low_high),
        _validate_low_below_high,
    )


# Defaults are the device's own factory thresholds.
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5_CHAIN_ENV_ID): cv.use_id(M5ChainEnv),
            cv.Optional(CONF_TEMPERATURE_ALARM): _alarm_schema(
                "mdi:thermometer-alert",
                {
                    # Whole degrees Celsius
                    cv.Optional(CONF_LOW, default=10): cv.int_range(min=0, max=255),
                    cv.Optional(CONF_HIGH, default=40): cv.int_range(min=0, max=255),
                    cv.Optional(CONF_SOURCE, default="aht20"): cv.enum(
                        TEMPERATURE_SOURCES, lower=True
                    ),
                },
            ),
            cv.Optional(CONF_HUMIDITY_ALARM): _alarm_schema(
                "mdi:water-alert",
                {
                    # Percent relative humidity
                    cv.Optional(CONF_LOW, default=20): cv.int_range(min=0, max=100),
                    cv.Optional(CONF_HIGH, default=80): cv.int_range(min=0, max=100),
                },
            ),
            cv.Optional(CONF_PRESSURE_ALARM): _alarm_schema(
                "mdi:gauge",
                {
                    # hPa
                    cv.Optional(CONF_LOW, default=950): cv.float_range(
                        min=0, max=6553.5
                    ),
                    cv.Optional(CONF_HIGH, default=1050): cv.float_range(
                        min=0, max=6553.5
                    ),
                },
            ),
            cv.Optional(CONF_ALTITUDE_ALARM): _alarm_schema(
                "mdi:altimeter",
                {
                    # Metres
                    cv.Optional(CONF_LOW, default=20): cv.float_range(
                        min=-20000, max=20000
                    ),
                    cv.Optional(CONF_HIGH, default=1000): cv.float_range(
                        min=-20000, max=20000
                    ),
                },
            ),
        }
    ),
    cv.has_at_least_one_key(
        CONF_TEMPERATURE_ALARM,
        CONF_HUMIDITY_ALARM,
        CONF_PRESSURE_ALARM,
        CONF_ALTITUDE_ALARM,
    ),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_CHAIN_ENV_ID])
    sensors = binary_sensor.sub_binary_sensors(config, parent=hub)

    if conf := config.get(CONF_TEMPERATURE_ALARM):
        await sensors.create(conf, hub.set_temperature_alarm_binary_sensor)
        cg.add(
            hub.set_temperature_alarm(
                conf[CONF_LOW], conf[CONF_HIGH], conf[CONF_SOURCE]
            )
        )
    if conf := config.get(CONF_HUMIDITY_ALARM):
        await sensors.create(conf, hub.set_humidity_alarm_binary_sensor)
        cg.add(hub.set_humidity_alarm(conf[CONF_LOW], conf[CONF_HIGH]))
    if conf := config.get(CONF_PRESSURE_ALARM):
        await sensors.create(conf, hub.set_pressure_alarm_binary_sensor)
        cg.add(
            hub.set_pressure_alarm(
                round(conf[CONF_LOW] * 10), round(conf[CONF_HIGH] * 10)
            )
        )
    if conf := config.get(CONF_ALTITUDE_ALARM):
        await sensors.create(conf, hub.set_altitude_alarm_binary_sensor)
        cg.add(
            hub.set_altitude_alarm(
                round(conf[CONF_LOW] * 100), round(conf[CONF_HIGH] * 100)
            )
        )
