from esphome import automation
import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_DATA,
    CONF_ID,
    CONF_LENGTH,
    CONF_LEVEL,
    CONF_MODE,
    CONF_PIN,
    CONF_SPEED,
)
from esphome.types import ConfigType

AUTO_LOAD = ["m5_chain_base"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_unit_chain_bus"
MULTI_CONF = True

CONF_M5_UNIT_CHAIN_BUS_ID = "m5_unit_chain_bus_id"
CONF_PULL = "pull"
CONF_REGISTER = "register"
CONF_REGISTER_SIZE = "register_size"
CONF_TRIGGER = "trigger"

CONF_ON_I2C_READ = "on_i2c_read"
CONF_ON_I2C_SCAN = "on_i2c_scan"
CONF_ON_PIN_LEVEL = "on_pin_level"
CONF_ON_ADC_VALUE = "on_adc_value"

m5_unit_chain_bus_ns = cg.esphome_ns.namespace("m5_unit_chain_bus")
M5UnitChainBus = m5_unit_chain_bus_ns.class_(
    "M5UnitChainBus", m5_chain_base.M5ChainBase
)

ChainBusI2CSpeed = m5_unit_chain_bus_ns.enum("ChainBusI2CSpeed", is_class=True)
ChainBusOutputMode = m5_unit_chain_bus_ns.enum("ChainBusOutputMode", is_class=True)
ChainBusPull = m5_unit_chain_bus_ns.enum("ChainBusPull", is_class=True)
ChainBusRegisterSize = m5_unit_chain_bus_ns.enum("ChainBusRegisterSize", is_class=True)
ChainBusTrigger = m5_unit_chain_bus_ns.enum("ChainBusTrigger", is_class=True)

I2C_SPEEDS = {
    "100khz": ChainBusI2CSpeed.CHAIN_BUS_I2C_SPEED_100_KHZ,
    "400khz": ChainBusI2CSpeed.CHAIN_BUS_I2C_SPEED_400_KHZ,
}
OUTPUT_MODES = {
    "push_pull": ChainBusOutputMode.CHAIN_BUS_OUTPUT_MODE_PUSH_PULL,
    "open_drain": ChainBusOutputMode.CHAIN_BUS_OUTPUT_MODE_OPEN_DRAIN,
}
PULLS = {
    "up": ChainBusPull.CHAIN_BUS_PULL_UP,
    "down": ChainBusPull.CHAIN_BUS_PULL_DOWN,
    "none": ChainBusPull.CHAIN_BUS_PULL_NONE,
}
REGISTER_SIZES = {
    "8bit": ChainBusRegisterSize.CHAIN_BUS_REGISTER_SIZE_8_BIT,
    "16bit": ChainBusRegisterSize.CHAIN_BUS_REGISTER_SIZE_16_BIT,
}
TRIGGERS = {
    "rising": ChainBusTrigger.CHAIN_BUS_TRIGGER_RISING,
    "falling": ChainBusTrigger.CHAIN_BUS_TRIGGER_FALLING,
    "rising_falling": ChainBusTrigger.CHAIN_BUS_TRIGGER_RISING_FALLING,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5UnitChainBus),
            cv.Optional(CONF_ON_I2C_READ): automation.validate_automation({}),
            cv.Optional(CONF_ON_I2C_SCAN): automation.validate_automation({}),
            cv.Optional(CONF_ON_PIN_LEVEL): automation.validate_automation({}),
            cv.Optional(CONF_ON_ADC_VALUE): automation.validate_automation({}),
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

_BYTES = cg.std_vector.template(cg.uint8)
_BYTE_LIST = cg.RawExpression("std::initializer_list<uint8_t>")

_CALLBACK_AUTOMATIONS = (
    automation.CallbackAutomation(
        CONF_ON_I2C_READ,
        "add_on_i2c_read_callback",
        [(cg.uint8, "address"), (_BYTES, "data")],
    ),
    automation.CallbackAutomation(
        CONF_ON_I2C_SCAN,
        "add_on_i2c_scan_callback",
        [(_BYTES, "addresses")],
    ),
    automation.CallbackAutomation(
        CONF_ON_PIN_LEVEL,
        "add_on_pin_level_callback",
        [(cg.uint8, "pin"), (cg.bool_, "level")],
    ),
    automation.CallbackAutomation(
        CONF_ON_ADC_VALUE,
        "add_on_adc_value_callback",
        [(cg.uint8, "pin"), (cg.uint16, "value")],
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_chain_index(config[m5_chain_base.CONF_CHAIN_INDEX]))
    cg.add(var.set_update_interval(config[m5_chain_base.CONF_UPDATE_INTERVAL]))
    await automation.build_callback_automations(var, config, _CALLBACK_AUTOMATIONS)


_ACTION_ID = {cv.GenerateID(): cv.use_id(M5UnitChainBus)}
_PIN = cv.int_range(min=1, max=2)
_I2C_DATA = cv.All([cv.uint8_t], cv.Length(min=1, max=64))
_I2C_LENGTH = cv.int_range(min=1, max=64)

automation.register_apply_action(
    f"{DOMAIN}.i2c_init",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_SPEED): cv.enum(I2C_SPEEDS)}),
    automation.ApplyField(CONF_SPEED, "i2c_init", ChainBusI2CSpeed),
)
automation.register_apply_action(
    f"{DOMAIN}.i2c_write",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_ADDRESS): cv.uint8_t,
            cv.Required(CONF_DATA): _I2C_DATA,
        }
    ),
    automation.ApplyCall(
        "i2c_write({},{})",
        ((CONF_ADDRESS, cg.uint8), (CONF_DATA, _BYTE_LIST)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.i2c_read",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_ADDRESS): cv.uint8_t,
            cv.Required(CONF_LENGTH): _I2C_LENGTH,
        }
    ),
    automation.ApplyCall(
        "i2c_read({},{})",
        ((CONF_ADDRESS, cg.uint8), (CONF_LENGTH, cg.uint8)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.i2c_mem_read",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_ADDRESS): cv.uint8_t,
            cv.Required(CONF_REGISTER): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_REGISTER_SIZE, default="8bit"): cv.enum(REGISTER_SIZES),
            cv.Required(CONF_LENGTH): _I2C_LENGTH,
        }
    ),
    automation.ApplyCall(
        "i2c_mem_read({},{},{},{})",
        (
            (CONF_ADDRESS, cg.uint8),
            (CONF_REGISTER, cg.uint16),
            (CONF_REGISTER_SIZE, ChainBusRegisterSize),
            (CONF_LENGTH, cg.uint8),
        ),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.i2c_mem_write",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_ADDRESS): cv.uint8_t,
            cv.Required(CONF_REGISTER): cv.int_range(min=0, max=65535),
            cv.Optional(CONF_REGISTER_SIZE, default="8bit"): cv.enum(REGISTER_SIZES),
            cv.Required(CONF_DATA): _I2C_DATA,
        }
    ),
    automation.ApplyCall(
        "i2c_mem_write({},{},{},{})",
        (
            (CONF_ADDRESS, cg.uint8),
            (CONF_REGISTER, cg.uint16),
            (CONF_REGISTER_SIZE, ChainBusRegisterSize),
            (CONF_DATA, _BYTE_LIST),
        ),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.i2c_scan",
    cv.Schema(_ACTION_ID),
    automation.ApplyCall("i2c_scan()"),
)
automation.register_apply_action(
    f"{DOMAIN}.set_output_mode",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_PIN): _PIN,
            cv.Optional(CONF_MODE, default="push_pull"): cv.enum(OUTPUT_MODES),
            cv.Optional(CONF_PULL, default="none"): cv.enum(PULLS),
        }
    ),
    automation.ApplyCall(
        "set_output_mode({},{},{})",
        (
            (CONF_PIN, cg.uint8),
            (CONF_MODE, ChainBusOutputMode),
            (CONF_PULL, ChainBusPull),
        ),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_output_level",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_PIN): _PIN,
            cv.Required(CONF_LEVEL): cv.boolean,
        }
    ),
    automation.ApplyCall(
        "set_output_level({},{})",
        ((CONF_PIN, cg.uint8), (CONF_LEVEL, cg.bool_)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.get_output_level",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_PIN): _PIN}),
    automation.ApplyCall("get_output_level({})", ((CONF_PIN, cg.uint8),)),
)
automation.register_apply_action(
    f"{DOMAIN}.set_input_mode",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_PIN): _PIN,
            cv.Optional(CONF_PULL, default="none"): cv.enum(PULLS),
        }
    ),
    automation.ApplyCall(
        "set_input_mode({},{})",
        ((CONF_PIN, cg.uint8), (CONF_PULL, ChainBusPull)),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.get_input_level",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_PIN): _PIN}),
    automation.ApplyCall("get_input_level({})", ((CONF_PIN, cg.uint8),)),
)
automation.register_apply_action(
    f"{DOMAIN}.set_nvic_mode",
    cv.Schema(
        {
            **_ACTION_ID,
            cv.Required(CONF_PIN): _PIN,
            cv.Optional(CONF_PULL, default="none"): cv.enum(PULLS),
            cv.Optional(CONF_TRIGGER, default="rising"): cv.enum(TRIGGERS),
        }
    ),
    automation.ApplyCall(
        "set_nvic_mode({},{},{})",
        (
            (CONF_PIN, cg.uint8),
            (CONF_PULL, ChainBusPull),
            (CONF_TRIGGER, ChainBusTrigger),
        ),
    ),
)
automation.register_apply_action(
    f"{DOMAIN}.set_adc_mode",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_PIN): _PIN}),
    automation.ApplyCall("set_adc_mode({})", ((CONF_PIN, cg.uint8),)),
)
automation.register_apply_action(
    f"{DOMAIN}.get_adc_value",
    cv.Schema({**_ACTION_ID, cv.Required(CONF_PIN): _PIN}),
    automation.ApplyCall("get_adc_value({})", ((CONF_PIN, cg.uint8),)),
)
