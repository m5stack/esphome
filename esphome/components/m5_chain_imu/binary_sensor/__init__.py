import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HIGH,
    CONF_LOW,
    CONF_MOTION,
    CONF_ROTATION,
    CONF_THRESHOLD,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_MOVING,
    DEVICE_CLASS_PROBLEM,
)
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_IMU_ID, M5ChainImu, m5_chain_imu_ns

DEPENDENCIES = ["m5_chain_imu"]

M5ChainImuBinarySensor = m5_chain_imu_ns.class_(
    "M5ChainImuBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainImu),
)

CONF_TEMPERATURE_ALARM = "temperature_alarm"

# The device takes both motion thresholds in one command. Their unit is not documented; 40 is the value
# M5Stack's own example uses.
DEFAULT_MOTION_THRESHOLD = 40


def _threshold_schema(device_class: str, icon: str) -> cv.Schema:
    return binary_sensor.binary_sensor_schema(
        M5ChainImuBinarySensor, device_class=device_class, icon=icon
    ).extend(
        {
            cv.Optional(CONF_THRESHOLD, default=DEFAULT_MOTION_THRESHOLD): cv.uint8_t,
        }
    )


def _validate_low_below_high(config: ConfigType) -> ConfigType:
    if config[CONF_LOW] >= config[CONF_HIGH]:
        raise cv.Invalid(f"'{CONF_LOW}' must be below '{CONF_HIGH}'")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_M5_CHAIN_IMU_ID): cv.use_id(M5ChainImu),
            # On while the acceleration is past its threshold
            cv.Optional(CONF_MOTION): _threshold_schema(DEVICE_CLASS_MOTION, "mdi:run"),
            # On while the angular rate is past its threshold
            cv.Optional(CONF_ROTATION): _threshold_schema(
                DEVICE_CLASS_MOVING, "mdi:rotate-3d-variant"
            ),
            # On while the IMU temperature is outside the low to high band, whole degrees Celsius
            cv.Optional(CONF_TEMPERATURE_ALARM): cv.All(
                binary_sensor.binary_sensor_schema(
                    M5ChainImuBinarySensor,
                    device_class=DEVICE_CLASS_PROBLEM,
                    icon="mdi:thermometer-alert",
                ).extend(
                    {
                        cv.Required(CONF_LOW): cv.uint8_t,
                        cv.Required(CONF_HIGH): cv.uint8_t,
                    }
                ),
                _validate_low_below_high,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_MOTION, CONF_ROTATION, CONF_TEMPERATURE_ALARM),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_CHAIN_IMU_ID])
    sensors = binary_sensor.sub_binary_sensors(config, parent=hub)

    motion = config.get(CONF_MOTION)
    rotation = config.get(CONF_ROTATION)
    if motion is not None:
        await sensors.create(motion, hub.set_motion_binary_sensor)
        cg.add(hub.enable_motion_event())
    if rotation is not None:
        await sensors.create(rotation, hub.set_rotation_binary_sensor)
        cg.add(hub.enable_rotation_event())
    if motion is not None or rotation is not None:
        cg.add(
            hub.set_motion_thresholds(
                (motion or {}).get(CONF_THRESHOLD, DEFAULT_MOTION_THRESHOLD),
                (rotation or {}).get(CONF_THRESHOLD, DEFAULT_MOTION_THRESHOLD),
            )
        )
    if (conf := config.get(CONF_TEMPERATURE_ALARM)) is not None:
        await sensors.create(conf, hub.set_temperature_alarm_binary_sensor)
        cg.add(hub.set_temperature_alarm(conf[CONF_LOW], conf[CONF_HIGH]))
