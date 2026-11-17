import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_ID,
    CONF_INTEGRATION_TIME,
    CONF_UPDATE_INTERVAL,
)
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "m5_chain_base", "sensor"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_dlight"
MULTI_CONF = True

CONF_M5_CHAIN_DLIGHT_ID = "m5_chain_dlight_id"

m5_chain_dlight_ns = cg.esphome_ns.namespace("m5_chain_dlight")
M5ChainDLight = m5_chain_dlight_ns.class_("M5ChainDLight", m5_chain_base.M5ChainBase)

# LTR-308ALS gain codes
GAINS = {"1X": 0, "3X": 1, "6X": 2, "9X": 3, "18X": 4}
# LTR-308ALS resolution codes, named by their conversion time
INTEGRATION_TIMES = {"400ms": 0, "200ms": 1, "100ms": 2, "50ms": 3, "25ms": 4}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5ChainDLight),
            cv.Optional(CONF_GAIN): cv.enum(GAINS, upper=True),
            cv.Optional(CONF_INTEGRATION_TIME): cv.enum(INTEGRATION_TIMES, lower=True),
        }
    )
    .extend(m5_chain_base.BASE_SCHEMA)
    .extend({cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.update_interval})
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
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add_optional(var.set_gain, config.get(CONF_GAIN))
    cg.add_optional(var.set_resolution, config.get(CONF_INTEGRATION_TIME))
