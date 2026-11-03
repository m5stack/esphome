import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "m5_chain_base", "sensor"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_joystick"
MULTI_CONF = True

CONF_M5_CHAIN_JOYSTICK_ID = "m5_chain_joystick_id"

m5_chain_joystick_ns = cg.esphome_ns.namespace("m5_chain_joystick")
M5ChainJoystick = m5_chain_joystick_ns.class_(
    "M5ChainJoystick", m5_chain_base.M5ChainBase
)

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(M5ChainJoystick)})
    .extend(m5_chain_base.BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=115200,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_chain_index(config[m5_chain_base.CONF_CHAIN_INDEX]))
    cg.add(var.set_update_interval(config[m5_chain_base.CONF_UPDATE_INTERVAL]))
