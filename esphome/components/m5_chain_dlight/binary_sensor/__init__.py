import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_THRESHOLD
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_DLIGHT_ID, M5ChainDLight, m5_chain_dlight_ns

DEPENDENCIES = ["m5_chain_dlight"]

M5ChainDLightBinarySensor = m5_chain_dlight_ns.class_(
    "M5ChainDLightBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainDLight),
)

CONF_LOW_LIGHT = "low_light"
CONF_HIGH_LIGHT = "high_light"

# Largest lux threshold the device accepts
MAX_THRESHOLD = 2516580


def _alarm_schema(icon: str) -> cv.Schema:
    """On while the light level is past `threshold` (lux); the device reports each crossing.

    No `light` device class: Home Assistant would show "Light detected" for a dark alarm and
    "No light" for a room below the bright threshold.
    """
    return binary_sensor.binary_sensor_schema(
        M5ChainDLightBinarySensor, icon=icon
    ).extend(
        {
            cv.Required(CONF_THRESHOLD): cv.int_range(min=0, max=MAX_THRESHOLD),
        }
    )


def _band(config: ConfigType) -> tuple[int, int]:
    """The device keeps one lux band; a side with no binary sensor is pushed to the end of the range."""
    low = config.get(CONF_LOW_LIGHT, {}).get(CONF_THRESHOLD, 0)
    high = config.get(CONF_HIGH_LIGHT, {}).get(CONF_THRESHOLD, MAX_THRESHOLD)
    return low, high


def _validate_band(config: ConfigType) -> ConfigType:
    low, high = _band(config)
    if low >= high:
        raise cv.Invalid(
            f"The '{CONF_LOW_LIGHT}' threshold must be below the '{CONF_HIGH_LIGHT}' threshold"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5_CHAIN_DLIGHT_ID): cv.use_id(M5ChainDLight),
            cv.Optional(CONF_LOW_LIGHT): _alarm_schema("mdi:weather-night"),
            cv.Optional(CONF_HIGH_LIGHT): _alarm_schema("mdi:white-balance-sunny"),
        }
    ),
    cv.has_at_least_one_key(CONF_LOW_LIGHT, CONF_HIGH_LIGHT),
    _validate_band,
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_CHAIN_DLIGHT_ID])
    sensors = binary_sensor.sub_binary_sensors(config, parent=hub)
    await sensors(CONF_LOW_LIGHT, hub.set_low_light_binary_sensor)
    await sensors(CONF_HIGH_LIGHT, hub.set_high_light_binary_sensor)
    cg.add(hub.set_thresholds(*_band(config)))
