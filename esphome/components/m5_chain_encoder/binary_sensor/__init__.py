import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_ENCODER_ID, M5ChainEncoder, m5_chain_encoder_ns

DEPENDENCIES = ["m5_chain_encoder"]

M5ChainEncoderBinarySensor = m5_chain_encoder_ns.class_(
    "M5ChainEncoderBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainEncoder),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(M5ChainEncoderBinarySensor).extend(
    {cv.GenerateID(CONF_M5_CHAIN_ENCODER_ID): cv.use_id(M5ChainEncoder)}
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_ENCODER_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_button_sensor(var))
