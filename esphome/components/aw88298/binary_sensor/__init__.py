import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.const import DEVICE_CLASS_PROBLEM, ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from .. import (
    CONF_AW88298_ID,
    EntityDef,
    StatusFlag,
    aw88298_ns,
    request_status_sensor_slot,
    schema,
)

CONF_AMPLIFIER_SWITCHING = "amplifier_switching"
CONF_BOOST_ADAPTIVE = "boost_adaptive"
CONF_BOOST_OVER_CURRENT = "boost_over_current"
CONF_BOOST_OVER_VOLTAGE = "boost_over_voltage"
CONF_BOOST_OVER_VOLTAGE_2 = "boost_over_voltage_2"
CONF_BOOST_STARTED = "boost_started"
CONF_CLIPPING = "clipping"
CONF_CLOCK_STABLE = "clock_stable"
CONF_FAULT = "fault"
CONF_NO_CLOCK = "no_clock"
CONF_OVER_CURRENT = "over_current"
CONF_OVER_TEMPERATURE = "over_temperature"
CONF_PLL_LOCKED = "pll_locked"
CONF_PRE_CLIPPING = "pre_clipping"
CONF_UNDER_VOLTAGE = "under_voltage"

AW88298StatusBinarySensor = aw88298_ns.class_(
    "AW88298StatusBinarySensor", binary_sensor.BinarySensor
)

#: One entity per status bit of SYSST, plus the combined fault state.
STATUS_SENSORS = {
    CONF_BOOST_OVER_VOLTAGE_2: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_BOOST_OVER_VOLTAGE_2
    ),
    CONF_UNDER_VOLTAGE: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_UNDER_VOLTAGE
    ),
    CONF_BOOST_ADAPTIVE: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_BOOST_ADAPTIVE
    ),
    CONF_BOOST_OVER_CURRENT: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_BOOST_OVER_CURRENT
    ),
    CONF_BOOST_OVER_VOLTAGE: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_BOOST_OVER_VOLTAGE
    ),
    CONF_BOOST_STARTED: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_BOOST_STARTED
    ),
    CONF_AMPLIFIER_SWITCHING: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_AMPLIFIER_SWITCHING
    ),
    CONF_CLIPPING: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_CLIPPING
    ),
    CONF_NO_CLOCK: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_NO_CLOCK
    ),
    CONF_CLOCK_STABLE: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_CLOCK_STABLE
    ),
    CONF_OVER_CURRENT: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_OVER_CURRENT
    ),
    CONF_PRE_CLIPPING: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_PRE_CLIPPING
    ),
    CONF_OVER_TEMPERATURE: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_OVER_TEMPERATURE
    ),
    CONF_PLL_LOCKED: EntityDef(
        AW88298StatusBinarySensor, StatusFlag.STATUS_FLAG_PLL_LOCKED
    ),
    # The combined fault state reads the same register, so it is an entity like the bits.
    CONF_FAULT: EntityDef(
        AW88298StatusBinarySensor,
        StatusFlag.STATUS_FLAG_FAULT,
        {"device_class": DEVICE_CLASS_PROBLEM},
    ),
}


def _options(definition: EntityDef) -> dict:
    """Status bits are diagnostic; the fault state is what a user should look at first."""
    return {"entity_category": ENTITY_CATEGORY_DIAGNOSTIC, **definition.options}


CONFIG_SCHEMA = schema(
    STATUS_SENSORS,
    lambda definition: binary_sensor.binary_sensor_schema(
        definition.class_, **_options(definition)
    ),
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_AW88298_ID])
    for key, definition in STATUS_SENSORS.items():
        if (conf := config.get(key)) is None:
            continue
        var = await binary_sensor.new_binary_sensor(conf)
        request_status_sensor_slot()
        cg.add(parent.add_status_binary_sensor(definition.source, var))
