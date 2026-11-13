"""Shared pieces for the X-Powers AXP power management ICs.

This component is not configurable on its own. It provides the C++ base class
that the per-chip components (`axp192`, `axp2101`, ...) build on, plus the
helpers the platform components reuse.

The entity classes themselves live in each chip's platform directories, so a
configuration that only has the hub pulls in no platform component at all.
"""

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_IRQ_PIN
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "axp_base"

axp_base_ns = cg.esphome_ns.namespace("axp_base")
AxpComponent = axp_base_ns.class_("AxpComponent", cg.PollingComponent, i2c.I2CDevice)

#: Key used by every AXP entity to point at the PMIC it belongs to. It is
#: optional when the configuration only has a single AXP hub.
CONF_AXP_ID = "axp_id"
CONF_EVENT_POLL_INTERVAL = "event_poll_interval"

#: Below this the poll would run on every pass of the 16 ms main loop anyway.
MIN_EVENT_POLL_INTERVAL = cv.TimePeriod(milliseconds=20)


def event_poll_interval(value) -> cv.TimePeriod:
    """Time between reads of the interrupt status registers without an IRQ pin."""
    if value == "never":
        # Zero tells the base class to leave the status registers alone.
        return cv.TimePeriod(milliseconds=0)
    value = cv.positive_time_period_milliseconds(value)
    if value < MIN_EVENT_POLL_INTERVAL:
        raise cv.Invalid(
            f"'event_poll_interval' must be at least {MIN_EVENT_POLL_INTERVAL} or 'never'"
        )
    return value


#: Extend this in a platform schema that belongs to one AXP hub.
AXP_BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
    }
)


def axp_component_schema(
    component_class: MockObjClass,
    default_address: int,
    default_update_interval: str = "60s",
) -> cv.Schema:
    """Build the schema shared by every AXP chip component.

    :param component_class: the chip's C++ class, used to declare its ID.
    :param default_address: I2C address the chip uses when none is given.
    :param default_update_interval: how often the status registers are polled.
    """
    return (
        cv.Schema(
            {
                cv.GenerateID(): cv.declare_id(component_class),
                # Active low on every AXP chip, so the base class watches for a
                # falling edge. `inverted:` only changes how the pin reads.
                cv.Optional(CONF_IRQ_PIN): pins.internal_gpio_input_pin_schema,
                # Only used when no IRQ pin is wired: the chip cannot tell us
                # about an event by itself, so the status registers are read
                # instead. Cheap enough at this rate, and only once something
                # subscribes to events.
                cv.Optional(
                    CONF_EVENT_POLL_INTERVAL, default="100ms"
                ): event_poll_interval,
            }
        )
        .extend(cv.polling_component_schema(default_update_interval))
        .extend(i2c.i2c_device_schema(default_address))
    )


async def register_axp(var: MockObj, config: ConfigType) -> None:
    """Register a PMIC hub: component, I2C device and optional interrupt pin."""
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_event_poll_interval(config[CONF_EVENT_POLL_INTERVAL]))
    if (irq_pin := config.get(CONF_IRQ_PIN)) is not None:
        cg.add(var.set_irq_pin(await cg.gpio_pin_expression(irq_pin)))


async def get_axp(config: ConfigType) -> MockObj:
    """Fetch the PMIC an entity config points at."""
    return await cg.get_variable(config[CONF_AXP_ID])
