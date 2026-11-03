import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_JOYSTICK_ID, M5ChainJoystick, m5_chain_joystick_ns

DEPENDENCIES = ["m5_chain_joystick"]

M5ChainJoystickBinarySensor = m5_chain_joystick_ns.class_(
    "M5ChainJoystickBinarySensor",
    binary_sensor.BinarySensor,
    cg.Parented.template(M5ChainJoystick),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(M5ChainJoystickBinarySensor).extend(
    {cv.GenerateID(CONF_M5_CHAIN_JOYSTICK_ID): cv.use_id(M5ChainJoystick)}
)


async def to_code(config: ConfigType) -> None:
    var = await binary_sensor.new_binary_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_JOYSTICK_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_button_sensor(var))
