from collections.abc import Iterable, Iterator
import logging
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import gpio_expander, i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT_PIN,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PIN,
    CONF_PLATFORM,
    CONF_RESET,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["gpio_expander"]
CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "aw9523b"
MULTI_CONF = True

aw9523b_ns = cg.esphome_ns.namespace("aw9523b")
AW9523BComponent = aw9523b_ns.class_("AW9523BComponent", cg.Component, i2c.I2CDevice)
AW9523BGPIOPin = aw9523b_ns.class_(
    "AW9523BGPIOPin", cg.GPIOPin, cg.Parented.template(AW9523BComponent)
)

CONF_AW9523B_ID = "aw9523b_id"
CONF_P0_DRIVE_MODE = "p0_drive_mode"
CONF_LED_MAX_CURRENT = "led_max_current"

AW9523BP0DriveMode = aw9523b_ns.enum("AW9523BP0DriveMode")

P0_DRIVE_MODES = {
    "OPEN_DRAIN": AW9523BP0DriveMode.OPEN_DRAIN,
    "PUSH_PULL": AW9523BP0DriveMode.PUSH_PULL,
}

AW9523BLEDMaxCurrent = aw9523b_ns.enum("AW9523BLEDMaxCurrent")

LED_MAX_CURRENTS = {
    "37mA": AW9523BLEDMaxCurrent.CURRENT_MAX,
    "27.75mA": AW9523BLEDMaxCurrent.CURRENT_3QUARTERS,
    "18.5mA": AW9523BLEDMaxCurrent.CURRENT_HALF,
    "9.25mA": AW9523BLEDMaxCurrent.CURRENT_1QUARTER,
}

# The hub reads its own input registers when INTN asserts; the generic GPIO binary sensor still
# reports "falling back to polling mode" for expander pins, but those reads are served from the
# cache that the hub refreshes on the interrupt.
BASE_SCHEMA = cv.Schema({cv.GenerateID(CONF_AW9523B_ID): cv.use_id(AW9523BComponent)})

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.declare_id(AW9523BComponent),
            cv.Optional(CONF_RESET, default=True): cv.boolean,
            cv.Optional(CONF_P0_DRIVE_MODE, default="OPEN_DRAIN"): cv.enum(
                P0_DRIVE_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_LED_MAX_CURRENT, default="37mA"): cv.enum(
                LED_MAX_CURRENTS
            ),
            cv.Optional(CONF_INTERRUPT_PIN): gpio_expander.validate_interrupt_pin,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x58))
)


def validate_mode(value: ConfigType) -> ConfigType:
    """Pins are either an input or an output, never both."""
    if value[CONF_INPUT] == value[CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


def _remove_use_interrupt(config: ConfigType) -> ConfigType:
    """Drop the `use_interrupt` option, which the hub now decides on its own.

    Every input pin is watched through `interrupt_pin` when the hub has one, so the option no longer
    selects anything, and rejecting it would break configurations written for the previous behaviour.
    """
    if "use_interrupt" in config:
        _LOGGER.warning(
            "The 'use_interrupt' option of '%s' pins is ignored: an input pin is watched through "
            "'%s' when the hub has one, and polled otherwise",
            DOMAIN,
            CONF_INTERRUPT_PIN,
        )
        config.pop("use_interrupt")
    return config


AW9523B_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        AW9523BGPIOPin,
        cv.int_range(min=0, max=15),
        modes=[CONF_INPUT, CONF_OUTPUT],
        mode_validator=validate_mode,
    )
    .extend(BASE_SCHEMA)
    # Legacy per-pin option, accepted so existing configurations keep loading but no longer used.
    .extend({cv.Optional("use_interrupt"): cv.boolean}),
    _remove_use_interrupt,
)


@pins.PIN_SCHEMA_REGISTRY.register(CONF_AW9523B_ID, AW9523B_PIN_SCHEMA)
async def aw9523b_pin_to_code(config: ConfigType) -> cg.MockObj:
    var = cg.new_Pvariable(config[CONF_ID])
    parent = await cg.get_variable(config[CONF_AW9523B_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_NUMBER]))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var


def _iter_config_dicts(node: Any) -> Iterator[ConfigType]:
    if isinstance(node, dict):
        yield node
        for value in node.values():
            yield from _iter_config_dicts(value)
    elif isinstance(node, list):
        for value in node:
            yield from _iter_config_dicts(value)


def _belongs_to_hub(item: ConfigType, hub_id: str, single_hub: bool) -> bool:
    """Whether an entry of our own schemas refers to this hub.

    The `aw9523b_id` key is always present on a validated entry, either holding an ID or a nameless
    one when it was omitted. Its absence means the entry is not ours at all, for example a pin of an
    internal GPIO.
    """
    if CONF_AW9523B_ID not in item:
        return False
    explicit = item[CONF_AW9523B_ID]
    # An omitted id is resolved to a nameless ID, which belongs to the only hub there is.
    if explicit is None or str(explicit) == "":
        return single_hub
    return str(explicit) == hub_id


def _find_pin_uses(hub_id: str, single_hub: bool) -> tuple[set[int], dict[int, int]]:
    """Collect the pins used as GPIO and the pins driven as a PWM output."""
    gpio_pins: set[int] = set()
    output_pins: dict[int, int] = {}

    for item in _iter_config_dicts(fv.full_config.get()):
        pin_conf = item.get(CONF_PIN)
        if isinstance(pin_conf, dict) and CONF_NUMBER in pin_conf:
            # `pin:` of any platform that takes an expander pin
            if _belongs_to_hub(pin_conf, hub_id, single_hub):
                gpio_pins.add(pin_conf[CONF_NUMBER])
        elif (
            item.get(CONF_PLATFORM) == DOMAIN
            and isinstance(pin_conf, int)
            and _belongs_to_hub(item, hub_id, single_hub)
        ):
            # an `output: platform: aw9523b` entry
            output_pins[pin_conf] = output_pins.get(pin_conf, 0) + 1

    return gpio_pins, output_pins


def final_validate_pins(config: ConfigType) -> ConfigType:
    """Reject pins that the output platform and a GPIO pin both want.

    A pin switched to LED mode drives its PWM instead of being a GPIO, so the GPIO would silently
    read the wrong level. The chip also only has one brightness register per pin, so two outputs
    on the same pin cannot work either.
    """
    hub_id = str(config[CONF_ID])
    hubs: Iterable[Any] = fv.full_config.get().get(DOMAIN) or []
    if isinstance(hubs, dict):
        hubs = [hubs]
    single_hub = len(list(hubs)) == 1

    gpio_pins, output_pins = _find_pin_uses(hub_id, single_hub)

    if shared := sorted(output_pins.keys() & gpio_pins):
        raise cv.Invalid(
            f"Pin(s) {shared} of the AW9523B hub '{hub_id}' are used both as an output "
            "and as a GPIO pin. A pin in LED (PWM) mode cannot be read or written as a GPIO; "
            "remove one of the two entries."
        )

    if duplicates := sorted(pin for pin, count in output_pins.items() if count > 1):
        raise cv.Invalid(
            f"Pin(s) {duplicates} of the AW9523B hub '{hub_id}' are used by more than one "
            "`output` entry. Each pin has a single brightness register."
        )

    return config


FINAL_VALIDATE_SCHEMA = final_validate_pins


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))

    cg.add(var.set_p0_drive_mode(config[CONF_P0_DRIVE_MODE]))
    cg.add(var.set_led_max_current(config[CONF_LED_MAX_CURRENT]))
    cg.add(var.set_reset(config[CONF_RESET]))
