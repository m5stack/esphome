from esphome import automation
import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_MODE, CONF_STATUS, CONF_TIME
from esphome.types import ConfigType

AUTO_LOAD = ["m5_chain_base"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_buzzer"
MULTI_CONF = True

CONF_M5_CHAIN_BUZZER_ID = "m5_chain_buzzer_id"

CONF_DUTY = "duty"
CONF_NOTE = "note"

m5_chain_buzzer_ns = cg.esphome_ns.namespace("m5_chain_buzzer")
M5ChainBuzzer = m5_chain_buzzer_ns.class_("M5ChainBuzzer", m5_chain_base.M5ChainBase)
BuzzerMode = m5_chain_buzzer_ns.enum("BuzzerMode", is_class=True)
BUZZER_MODES = {
    "auto": BuzzerMode.BUZZER_MODE_AUTO,
    "manual": BuzzerMode.BUZZER_MODE_MANUAL,
    "note": BuzzerMode.BUZZER_MODE_NOTE,
}

CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(M5ChainBuzzer)})
    .extend(m5_chain_base.BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN, baud_rate=115200, data_bits=8, parity="NONE", stop_bits=1
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_chain_index(config[m5_chain_base.CONF_CHAIN_INDEX]))
    cg.add(var.set_update_interval(config[m5_chain_base.CONF_UPDATE_INTERVAL]))


_ACTION_ID = {cv.GenerateID(): cv.use_id(M5ChainBuzzer)}


def _duration_ms(_config: ConfigType, value) -> str:
    return str(value.total_milliseconds)


automation.register_apply_action(
    "m5_chain_buzzer.set_mode",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_MODE): cv.enum(BUZZER_MODES)}),
    automation.ApplyField(CONF_MODE, "set_mode", BuzzerMode),
)
automation.register_apply_action(
    "m5_chain_buzzer.play",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_FREQUENCY): cv.templatable(
                cv.int_range(min=100, max=10000)
            ),
            cv.Optional(CONF_DUTY, default=50): cv.templatable(
                cv.int_range(min=0, max=100)
            ),
            cv.Required(CONF_TIME): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
    automation.ApplyCall(
        "play_auto({},{},{})",
        (
            (CONF_FREQUENCY, cg.uint16),
            (CONF_DUTY, cg.uint8),
            (CONF_TIME, cg.uint16, _duration_ms),
        ),
    ),
)
automation.register_apply_action(
    "m5_chain_buzzer.play_note",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_NOTE): cv.templatable(cv.int_range(min=0, max=255)),
            cv.Required(CONF_TIME): cv.templatable(
                cv.positive_time_period_milliseconds
            ),
        }
    ),
    automation.ApplyCall(
        "play_note({},{})",
        ((CONF_NOTE, cg.uint8), (CONF_TIME, cg.uint16, _duration_ms)),
    ),
)
automation.register_apply_action(
    "m5_chain_buzzer.set_status",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_STATUS): cv.templatable(cv.boolean)}),
    automation.ApplyField(CONF_STATUS, "set_status", cg.bool_),
)
automation.register_apply_action(
    "m5_chain_buzzer.set_frequency",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_FREQUENCY): cv.templatable(
                cv.int_range(min=100, max=10000)
            ),
        }
    ),
    automation.ApplyField(CONF_FREQUENCY, "set_frequency", cg.uint16),
)
automation.register_apply_action(
    "m5_chain_buzzer.set_duty",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_DUTY): cv.templatable(cv.int_range(min=0, max=100)),
        }
    ),
    automation.ApplyField(CONF_DUTY, "set_duty", cg.uint8),
)
