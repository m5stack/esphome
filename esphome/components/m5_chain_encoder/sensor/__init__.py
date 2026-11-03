import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_ENCODER_ID, M5ChainEncoder, m5_chain_encoder_ns

DEPENDENCIES = ["m5_chain_encoder"]

M5ChainEncoderSensor = m5_chain_encoder_ns.class_(
    "M5ChainEncoderSensor", sensor.Sensor, cg.Parented.template(M5ChainEncoder)
)
EncoderSensorType = m5_chain_encoder_ns.enum("EncoderSensorType", is_class=True)
SENSOR_TYPES = {
    "value": EncoderSensorType.ENCODER_SENSOR_VALUE,
    "increment": EncoderSensorType.ENCODER_SENSOR_INCREMENT,
    "ab_status": EncoderSensorType.ENCODER_SENSOR_AB_STATUS,
}

CONFIG_SCHEMA = sensor.sensor_schema(M5ChainEncoderSensor).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_ENCODER_ID): cv.use_id(M5ChainEncoder),
        cv.Optional(CONF_TYPE, default="value"): cv.enum(SENSOR_TYPES),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_ENCODER_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_encoder_sensor(SENSOR_TYPES[config[CONF_TYPE]], var))
