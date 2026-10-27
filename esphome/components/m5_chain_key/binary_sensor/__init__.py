import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import M5ChainKey, m5_chain_key_ns

DEPENDENCIES = ["m5_chain_key"]

CONF_M5_CHAIN_KEY_ID = "m5_chain_key_id"

M5ChainKeyBinarySensor = m5_chain_key_ns.class_(
    "M5ChainKeyBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainKey),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(M5ChainKeyBinarySensor).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_KEY_ID): cv.use_id(M5ChainKey),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_KEY_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_key_sensor(var))
