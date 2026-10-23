import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import BASE_SCHEMA, CONF_M5_VOICE_PYRAMID_KIT_ID, m5_voice_pyramid_kit_ns

DEPENDENCIES = ["m5_voice_pyramid_kit"]

M5VoicePyramidKitSensor = m5_voice_pyramid_kit_ns.class_(
    "M5VoicePyramidKitSensor", sensor.Sensor, cg.Component
)
M5VoicePyramidKitSensorType = m5_voice_pyramid_kit_ns.enum(
    "M5VoicePyramidKitSensorType"
)

CONF_SWIPE_EVENT = "swipe_event"
CONF_USB_VOLTAGE = "usb_voltage"

SENSOR_TYPES = {
    CONF_SWIPE_EVENT: M5VoicePyramidKitSensorType.SWIPE_EVENT,
    CONF_USB_VOLTAGE: M5VoicePyramidKitSensorType.USB_VOLTAGE,
}

CONFIG_SCHEMA = cv.typed_schema(
    {
        sensor_type: sensor.sensor_schema(M5VoicePyramidKitSensor).extend(BASE_SCHEMA)
        for sensor_type in SENSOR_TYPES
    },
    key=CONF_TYPE,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_VOICE_PYRAMID_KIT_ID])

    cg.add(var.set_type(SENSOR_TYPES[config[CONF_TYPE]]))
