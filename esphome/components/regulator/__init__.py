"""Power regulator platform.

A regulator is a power resource: something a component can switch on and off,
and optionally set the output voltage of. It is deliberately not tied to the
hardware that produces it, so a GPIO-driven load switch and a rail inside a
PMIC are the same kind of object to the rest of ESPHome.

This is the provider layer only. Each platform registers its regulators with
`RegulatorCore`; a consumer layer that lets other components ask for a
regulator by ID comes later.

The providers this component ships with live in the platform files:

* `gpio.regulator` - a load switch driven by a GPIO pin.
* `axp2101.regulator` - one rail of an AXP2101 PMIC.
"""

from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAX_VOLTAGE, CONF_STARTUP_DELAY, CONF_VOLTAGE
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DOMAIN = "regulator"
IS_PLATFORM_COMPONENT = True

CONF_ALWAYS_ON = "always_on"
CONF_BOOT_ON = "boot_on"
CONF_MIN_VOLTAGE = "min_voltage"

regulator_ns = cg.esphome_ns.namespace("regulator")
Regulator = regulator_ns.class_("Regulator", cg.Component)
RegulatorPtr = Regulator.operator("ptr")
RegulatorCore = regulator_ns.class_("RegulatorCore")

#: Code generation names a static class method, which `MockObj` renders with
#: the `::` scope operator.
_register_regulator = cg.MockObj(str(RegulatorCore), "::").register_regulator
_request_regulator_slot = cg.slot_counter("REGULATOR_COUNT")


def voltage_mv(value: Any) -> int:
    """Parse a voltage into millivolts.

    Bare integers are millivolts, matching the AXP rail options. Values with a
    unit are accepted too, so both `3300` and `3300mV` mean 3.3 V.
    """
    if isinstance(value, bool):
        raise cv.Invalid(f"Expected a voltage, got {value!r}")
    if isinstance(value, int):
        millivolt = value
    elif isinstance(value, str):
        try:
            millivolt = cv.int_(value)
        except cv.Invalid:
            millivolt = int(round(cv.voltage(value) * 1000))
    else:
        raise cv.Invalid(f"Expected a voltage such as 3300mV or 3.3V, got {value!r}")
    if millivolt <= 0:
        raise cv.Invalid("Voltage must be greater than zero")
    return millivolt


#: Options shared by every regulator platform.
REGULATOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_VOLTAGE): voltage_mv,
        cv.Optional(CONF_MIN_VOLTAGE): voltage_mv,
        cv.Optional(CONF_MAX_VOLTAGE): voltage_mv,
        # always_on implies boot_on; the validator below fills boot_on in.
        cv.Optional(CONF_BOOT_ON, default=False): cv.boolean,
        cv.Optional(CONF_ALWAYS_ON, default=False): cv.boolean,
        cv.Optional(
            CONF_STARTUP_DELAY, default="0ms"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)

#: Options a multi-rail platform entry can give as defaults for all its rails.
REGULATOR_DEFAULT_KEYS = (
    CONF_VOLTAGE,
    CONF_MIN_VOLTAGE,
    CONF_MAX_VOLTAGE,
    CONF_BOOT_ON,
    CONF_ALWAYS_ON,
    CONF_STARTUP_DELAY,
)

#: The same options without defaults. A platform entry uses this when one YAML
#: entry declares several regulators, so only the keys the user actually wrote
#: are copied into the individual rails.
REGULATOR_ENTRY_OPTIONS = {
    cv.Optional(CONF_VOLTAGE): voltage_mv,
    cv.Optional(CONF_MIN_VOLTAGE): voltage_mv,
    cv.Optional(CONF_MAX_VOLTAGE): voltage_mv,
    cv.Optional(CONF_BOOT_ON): cv.boolean,
    cv.Optional(CONF_ALWAYS_ON): cv.boolean,
    cv.Optional(CONF_STARTUP_DELAY): cv.positive_time_period_milliseconds,
}


def _validate_regulator(config: ConfigType) -> ConfigType:
    if config[CONF_ALWAYS_ON]:
        config[CONF_BOOT_ON] = True

    min_mv = config.get(CONF_MIN_VOLTAGE)
    max_mv = config.get(CONF_MAX_VOLTAGE)
    if min_mv is not None and max_mv is not None and min_mv >= max_mv:
        raise cv.Invalid(f"'{CONF_MIN_VOLTAGE}' must be below '{CONF_MAX_VOLTAGE}'")
    if (voltage := config.get(CONF_VOLTAGE)) is not None:
        if min_mv is not None and voltage < min_mv:
            raise cv.Invalid(f"'{CONF_VOLTAGE}' must not be below '{CONF_MIN_VOLTAGE}'")
        if max_mv is not None and voltage > max_mv:
            raise cv.Invalid(f"'{CONF_VOLTAGE}' must not be above '{CONF_MAX_VOLTAGE}'")
    return config


def regulator_schema(
    regulator_class: MockObjClass, extra: dict | cv.Schema | None = None
) -> Any:
    """Build a platform schema for one regulator class.

    :param regulator_class: the C++ class the platform creates.
    :param extra: platform-specific options to add.
    """
    schema = REGULATOR_SCHEMA.extend({cv.GenerateID(): cv.declare_id(regulator_class)})
    if extra:
        schema = schema.extend(extra)
    return cv.All(schema, _validate_regulator)


async def register_regulator(var: MockObj, config: ConfigType) -> None:
    """Register one regulator with the component list and the registry."""
    await cg.register_component(var, config)
    _request_regulator_slot()

    cg.add(var.set_regulator_name(config[CONF_ID].id))
    cg.add(var.set_boot_on(config[CONF_BOOT_ON]))
    cg.add(var.set_always_on(config[CONF_ALWAYS_ON]))
    cg.add(var.set_startup_delay(config[CONF_STARTUP_DELAY].total_milliseconds))
    if (voltage := config.get(CONF_VOLTAGE)) is not None:
        cg.add(var.set_startup_voltage(voltage))
    # Explicit checks instead of cg.add_optional(): the component also ships as
    # an external component on releases whose codegen has no such helper.
    if (min_voltage := config.get(CONF_MIN_VOLTAGE)) is not None:
        cg.add(var.set_min_voltage(min_voltage))
    if (max_voltage := config.get(CONF_MAX_VOLTAGE)) is not None:
        cg.add(var.set_max_voltage(max_voltage))

    cg.add(_register_regulator(var))


async def to_code(config: list[ConfigType]) -> None:
    cg.add_define("USE_REGULATOR")
    cg.add_global(regulator_ns.using)
