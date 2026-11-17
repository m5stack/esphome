import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "m5_chain_base", "sensor"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_env"
MULTI_CONF = True

CONF_M5_CHAIN_ENV_ID = "m5_chain_env_id"
CONF_SEA_LEVEL_PRESSURE = "sea_level_pressure"

m5_chain_env_ns = cg.esphome_ns.namespace("m5_chain_env")
M5ChainEnv = m5_chain_env_ns.class_("M5ChainEnv", m5_chain_base.M5ChainBase)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5ChainEnv),
            # Sea-level reference for the altitude reading, in hPa.
            cv.Optional(CONF_SEA_LEVEL_PRESSURE): cv.float_range(min=300, max=1100),
        }
    )
    .extend(m5_chain_base.BASE_SCHEMA)
    # Temperature, humidity and pressure change slowly, so poll far less often than the input devices.
    .extend({cv.Optional(CONF_UPDATE_INTERVAL, default="10s"): cv.update_interval})
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
    if (pressure := config.get(CONF_SEA_LEVEL_PRESSURE)) is not None:
        cg.add(var.set_sea_level_pressure(round(pressure * 10)))
