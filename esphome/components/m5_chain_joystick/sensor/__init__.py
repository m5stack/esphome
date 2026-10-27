import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import M5ChainJoystick, m5_chain_joystick_ns

DEPENDENCIES = ["m5_chain_joystick"]

CONF_M5_CHAIN_JOYSTICK_ID = "m5_chain_joystick_id"
CONF_AXIS = "axis"
CONF_RANGE_INDEX = "range_index"

M5ChainJoystickSensor = m5_chain_joystick_ns.class_(
    "M5ChainJoystickSensor", sensor.Sensor, cg.Parented.template(M5ChainJoystick)
)
JoystickSensorType = m5_chain_joystick_ns.enum("JoystickSensorType", is_class=True)

SENSOR_TYPES = {
    "adc_16": JoystickSensorType.JOYSTICK_SENSOR_ADC_16,
    "adc_8": JoystickSensorType.JOYSTICK_SENSOR_ADC_8,
    "mapped_int16": JoystickSensorType.JOYSTICK_SENSOR_MAPPED_16,
    "mapped_int8": JoystickSensorType.JOYSTICK_SENSOR_MAPPED_8,
    "mapping_range": JoystickSensorType.JOYSTICK_SENSOR_MAPPING_RANGE,
}
AXES = {"x": 0, "y": 1}

CONFIG_SCHEMA = sensor.sensor_schema(
    M5ChainJoystickSensor, icon="mdi:axis-arrow"
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_JOYSTICK_ID): cv.use_id(M5ChainJoystick),
        cv.Optional(CONF_TYPE, default="mapped_int16"): cv.enum(SENSOR_TYPES),
        cv.Optional(CONF_AXIS, default="x"): cv.enum(AXES),
        cv.Optional(CONF_RANGE_INDEX, default=0): cv.int_range(min=0, max=7),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_JOYSTICK_ID])
    await cg.register_parented(var, hub)
    cg.add(
        hub.set_joystick_sensor(
            SENSOR_TYPES[config[CONF_TYPE]],
            AXES[config[CONF_AXIS]],
            config[CONF_RANGE_INDEX],
            var,
        )
    )
