import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_ANGLE_ID, M5ChainAngle, m5_chain_angle_ns

DEPENDENCIES = ["m5_chain_angle"]

M5ChainAngleSensor = m5_chain_angle_ns.class_(
    "M5ChainAngleSensor", sensor.Sensor, cg.Parented.template(M5ChainAngle)
)
AngleSensorType = m5_chain_angle_ns.enum("AngleSensorType", is_class=True)
SENSOR_TYPES = {
    "adc_12": AngleSensorType.ANGLE_SENSOR_ADC_12,
    "adc_8": AngleSensorType.ANGLE_SENSOR_ADC_8,
}

CONFIG_SCHEMA = sensor.sensor_schema(
    M5ChainAngleSensor, icon="mdi:rotate-3d-variant"
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_ANGLE_ID): cv.use_id(M5ChainAngle),
        cv.Optional(CONF_TYPE, default="adc_12"): cv.enum(SENSOR_TYPES),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_ANGLE_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_angle_sensor(SENSOR_TYPES[config[CONF_TYPE]], var))
