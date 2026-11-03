import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_MIC_ID, M5ChainMic, m5_chain_mic_ns

DEPENDENCIES = ["m5_chain_mic"]

M5ChainMicBinarySensor = m5_chain_mic_ns.class_(
    "M5ChainMicBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainMic),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    M5ChainMicBinarySensor,
    icon="mdi:volume-vibrate",
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_MIC_ID): cv.use_id(M5ChainMic),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_MIC_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_trigger_sensor(var))
