import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIMEOUT
from esphome.core import TimePeriod
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["uart", "ftservo"]
DOMAIN = "scs9009"
MULTI_CONF = True

CONF_MIN_INTERVAL = "min_interval"
CONF_SCS9009_ID = "scs9009_id"


def validate_timeout(value: TimePeriod) -> TimePeriod:
    if value.total_milliseconds < 1:
        raise cv.Invalid("timeout must be at least 1ms")
    return value


scs9009_ns = cg.esphome_ns.namespace("scs9009")
SCS9009Component = scs9009_ns.class_("SCS9009Component", cg.Component, uart.UARTDevice)

BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SCS9009_ID): cv.use_id(SCS9009Component),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SCS9009Component),
        cv.Optional(CONF_TIMEOUT, default="10ms"): cv.All(
            cv.positive_time_period_milliseconds, validate_timeout
        ),
        cv.Optional(
            CONF_MIN_INTERVAL, default="2ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_timeout(config[CONF_TIMEOUT].total_milliseconds))
    cg.add(var.set_min_interval(config[CONF_MIN_INTERVAL].total_milliseconds))
