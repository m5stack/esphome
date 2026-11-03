from esphome import automation
import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "m5_chain_base"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_pir"
MULTI_CONF = True

CONF_M5_CHAIN_PIR_ID = "m5_chain_pir_id"
CONF_EVENT_REPORT = "event_report"
CONF_KEEP_SECONDS = "keep_seconds"
CONF_SAVE_TO_FLASH = "save_to_flash"

m5_chain_pir_ns = cg.esphome_ns.namespace("m5_chain_pir")
M5ChainPir = m5_chain_pir_ns.class_("M5ChainPir", m5_chain_base.M5ChainBase)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5ChainPir),
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


_ACTION_ID = {cv.GenerateID(): cv.use_id(M5ChainPir)}

automation.register_apply_action(
    f"{DOMAIN}.set_event_report",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_EVENT_REPORT): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyField(CONF_EVENT_REPORT, "set_event_report", cg.bool_),
)
automation.register_apply_action(
    f"{DOMAIN}.set_keep_seconds",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_KEEP_SECONDS): cv.templatable(
                cv.int_range(min=0, max=255)
            ),
            cv.Optional(CONF_SAVE_TO_FLASH, default=False): cv.templatable(cv.boolean),
        }
    ),
    automation.ApplyCall(
        "set_keep_seconds({},{})",
        ((CONF_KEEP_SECONDS, cg.uint8), (CONF_SAVE_TO_FLASH, cg.bool_)),
    ),
)
