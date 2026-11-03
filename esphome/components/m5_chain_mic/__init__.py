from datetime import timedelta

from esphome import automation
import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "m5_chain_base", "sensor"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_mic"
MULTI_CONF = True

CONF_M5_CHAIN_MIC_ID = "m5_chain_mic_id"
CONF_REPORT_MODE = "report_mode"
CONF_TRIGGER_INTERVAL = "trigger_interval"
CONF_SAVE_TO_FLASH = "save_to_flash"

m5_chain_mic_ns = cg.esphome_ns.namespace("m5_chain_mic")
M5ChainMic = m5_chain_mic_ns.class_("M5ChainMic", m5_chain_base.M5ChainBase)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5ChainMic),
        }
    )
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


_ACTION_ID = {cv.GenerateID(): cv.use_id(M5ChainMic)}


def _interval_ms(_config: ConfigType, value) -> str:
    return str(value.total_milliseconds)


def _validate_trigger_interval(value: timedelta) -> timedelta:
    milliseconds = value.total_milliseconds
    if not 300 <= milliseconds <= 1000:
        raise cv.Invalid("The trigger interval must be between 300ms and 1000ms")
    return value


automation.register_apply_action(
    f"{DOMAIN}.set_threshold",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_THRESHOLD): cv.templatable(cv.int_range(min=0, max=4095)),
            cv.Optional(CONF_SAVE_TO_FLASH, default=False): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyCall(
        "set_threshold({},{})",
        ((CONF_THRESHOLD, cg.uint16), (CONF_SAVE_TO_FLASH, cg.bool_)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_report_mode",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_REPORT_MODE): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyField(CONF_REPORT_MODE, "set_report_mode", cg.bool_),
)
automation.register_apply_action(
    f"{DOMAIN}.set_trigger_interval",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_TRIGGER_INTERVAL): cv.templatable(
                cv.All(cv.positive_time_period_milliseconds, _validate_trigger_interval)
            ),
        }
    ),
    automation.ApplyCall(
        "set_trigger_interval({})",
        ((CONF_TRIGGER_INTERVAL, cg.uint16, _interval_ms),),
    ),
)
