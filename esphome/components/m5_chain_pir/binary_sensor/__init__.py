import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_MOTION
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_PIR_ID, M5ChainPir, m5_chain_pir_ns

DEPENDENCIES = ["m5_chain_pir"]

M5ChainPirBinarySensor = m5_chain_pir_ns.class_(
    "M5ChainPirBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainPir),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    M5ChainPirBinarySensor,
    device_class=DEVICE_CLASS_MOTION,
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_PIR_ID): cv.use_id(M5ChainPir),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_PIR_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_presence_sensor(var))
