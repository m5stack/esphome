import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

AUTO_LOAD = ["output"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_base"
MULTI_CONF = True
MULTI_CONF_NO_DEFAULT = True

CONF_CHAIN_INDEX = "chain_index"

m5_chain_base_ns = cg.esphome_ns.namespace("m5_chain_base")
M5ChainBase = m5_chain_base_ns.class_("M5ChainBase", cg.Component, uart.UARTDevice)

BASE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHAIN_INDEX, default=1): cv.uint8_t,
        cv.Optional(CONF_UPDATE_INTERVAL, default="100ms"): cv.update_interval,
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5ChainBase),
        }
    )
    .extend(BASE_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "m5_chain_base",
    baud_rate=115200,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_chain_index(config[CONF_CHAIN_INDEX]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
