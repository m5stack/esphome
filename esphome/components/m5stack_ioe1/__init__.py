from collections.abc import Iterator
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
from esphome.components.const import CONF_PWM_FREQUENCY
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_RESET,
)
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5stack_ioe1"
MULTI_CONF = True

CONF_M5STACK_IOE1_ID = "m5stack_ioe1_id"
CONF_PWM_POLARITY = "pwm_polarity"
CONF_USE_INTERRUPT = "use_interrupt"
CONF_INTERRUPT_TYPE = "interrupt_type"

m5stack_ioe1_ns = cg.esphome_ns.namespace("m5stack_ioe1")
M5StackIOE1Component = m5stack_ioe1_ns.class_(
    "M5StackIOE1Component", cg.Component, i2c.I2CDevice
)
M5StackIOE1GPIOPin = m5stack_ioe1_ns.class_("M5StackIOE1GPIOPin", cg.GPIOPin)

M5StackIOE1PWMPolarity = m5stack_ioe1_ns.enum("M5StackIOE1PWMPolarity")

PWM_POLARITY = {
    "ACTIVE_HIGH": M5StackIOE1PWMPolarity.ACTIVE_HIGH,
    "ACTIVE_LOW": M5StackIOE1PWMPolarity.ACTIVE_LOW,
}

M5StackIOE1PWMChannel = m5stack_ioe1_ns.enum("M5StackIOE1PWMChannel")

PWM_CHANNEL = {
    "PWM_1": M5StackIOE1PWMChannel.PWM_1,
    "PWM_2": M5StackIOE1PWMChannel.PWM_2,
    "PWM_3": M5StackIOE1PWMChannel.PWM_3,
    "PWM_4": M5StackIOE1PWMChannel.PWM_4,
}

M5StackIOE1ADCChannel = m5stack_ioe1_ns.enum("M5StackIOE1ADCChannel")

ADC_CHANNEL = {
    "ADC_1": M5StackIOE1ADCChannel.ADC_1,
    "ADC_2": M5StackIOE1ADCChannel.ADC_2,
    "ADC_3": M5StackIOE1ADCChannel.ADC_3,
    "ADC_4": M5StackIOE1ADCChannel.ADC_4,
    "REF_VOLT": M5StackIOE1ADCChannel.REF_VOLT,
}

M5StackIOE1InterruptType = m5stack_ioe1_ns.enum("M5StackIOE1InterruptType")

INTERRUPT_TYPES = {
    "RISING_EDGE": M5StackIOE1InterruptType.RISING_EDGE,
    "FALLING_EDGE": M5StackIOE1InterruptType.FALLING_EDGE,
}

# Pins that share one interrupt channel inside the chip.
INTERRUPT_CONFLICT_PAIRS = ((0, 5), (1, 2), (6, 11), (7, 8), (9, 13), (10, 12))

BASE_SCHEMA = cv.Schema(
    {cv.GenerateID(CONF_M5STACK_IOE1_ID): cv.use_id(M5StackIOE1Component)}
)


def _iter_pin_configs(value: Any) -> Iterator[ConfigType]:
    """Yield every M5Stack IOE1 pin config in a validated configuration."""
    if isinstance(value, dict):
        if CONF_M5STACK_IOE1_ID in value and CONF_NUMBER in value:
            yield value
        for child in value.values():
            yield from _iter_pin_configs(child)
    elif isinstance(value, list):
        for child in value:
            yield from _iter_pin_configs(child)


def _final_validate(config: ConfigType) -> ConfigType:
    hub_id = str(config[CONF_ID])
    interrupt_pins = {
        pin[CONF_NUMBER]
        for pin in _iter_pin_configs(fv.full_config.get())
        if str(pin.get(CONF_M5STACK_IOE1_ID)) == hub_id and pin[CONF_USE_INTERRUPT]
    }
    for first, second in INTERRUPT_CONFLICT_PAIRS:
        if first in interrupt_pins and second in interrupt_pins:
            raise cv.Invalid(
                f"IO{first + 1} (pin number: {first}) and IO{second + 1} (pin number: {second}) "
                f"interrupts cannot be enabled simultaneously on hub '{hub_id}' "
                "(they share the same interrupt channel)."
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5StackIOE1Component),
            cv.Optional(CONF_RESET, default=True): cv.boolean,
            cv.Optional(CONF_PWM_FREQUENCY, default="500Hz"): cv.All(
                cv.frequency, cv.int_range(min=1, max=65535)
            ),
        }
    )
    .extend(gpio_expander.INTERRUPT_EXPANDER_SCHEMA)
    .extend(i2c.i2c_device_schema(0x6F))
)


def validate_pin_usage(pin_config: ConfigType, hub_config: ConfigType) -> None:
    if not pin_config[CONF_USE_INTERRUPT]:
        return
    if not pin_config[CONF_MODE][CONF_INPUT]:
        raise cv.Invalid(
            f"'{CONF_USE_INTERRUPT}: true' is only supported in input mode"
        )
    if CONF_INTERRUPT_PIN not in hub_config:
        raise cv.Invalid(
            f"'{CONF_USE_INTERRUPT}: true' requires '{CONF_INTERRUPT_PIN}' "
            "to be configured on the hub"
        )


M5STACK_IOE1_PIN_SCHEMA = pins.gpio_base_schema(
    M5StackIOE1GPIOPin,
    cv.int_range(min=0, max=13),
    modes=[CONF_INPUT, CONF_OUTPUT, CONF_PULLUP, CONF_PULLDOWN, CONF_OPEN_DRAIN],
    mode_validator=pins.gpio_validate_input_xor_output,
).extend(
    {
        cv.Required(CONF_M5STACK_IOE1_ID): cv.use_id(M5StackIOE1Component),
        cv.Optional(CONF_USE_INTERRUPT, default=False): cv.boolean,
        cv.Optional(CONF_INTERRUPT_TYPE, default="FALLING_EDGE"): cv.enum(
            INTERRUPT_TYPES, upper=True, space="_"
        ),
    }
)


@pins.PIN_SCHEMA_REGISTRY.register(
    CONF_M5STACK_IOE1_ID, M5STACK_IOE1_PIN_SCHEMA, validate_pin_usage
)
async def m5stack_ioe1_pin_to_code(config: ConfigType) -> MockObj:
    var = await pins.new_parented_pin(config, CONF_M5STACK_IOE1_ID)
    parent = await cg.get_variable(config[CONF_M5STACK_IOE1_ID])
    cg.add(
        parent.set_pin_interrupt(
            config[CONF_NUMBER],
            config[CONF_USE_INTERRUPT],
            config[CONF_INTERRUPT_TYPE],
        )
    )
    return var


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    cg.add(var.set_reset(config[CONF_RESET]))
    cg.add(var.set_pwm_frequency(config[CONF_PWM_FREQUENCY]))
    await cg.add_pin(var.set_interrupt_pin, config.get(CONF_INTERRUPT_PIN))
