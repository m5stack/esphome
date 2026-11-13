from functools import partial

import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.axp_base import CONF_AXP_ID
from esphome.components.axp_base.entities import (
    CONF_DIE_TEMPERATURE,
    CONF_SYSTEM_VOLTAGE,
    CONF_TS_VOLTAGE,
    CONF_VBUS_VOLTAGE,
    adc as _adc,
    add_entities,
    forward_update_interval,
    schema,
    state_of_charge as _state_of_charge,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BATTERY,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from .. import axp2101_ns

DEPENDENCIES = ["axp2101"]

AXP2101Sensor = axp2101_ns.class_("AXP2101Sensor", sensor.Sensor, cg.Component)
AXP2101AdcSensor = axp2101_ns.class_("AXP2101AdcSensor", AXP2101Sensor)
AXP2101SocSensor = axp2101_ns.class_("AXP2101SocSensor", AXP2101Sensor)

adc = partial(_adc, AXP2101AdcSensor)
state_of_charge = partial(_state_of_charge, AXP2101SocSensor)

# Every measurement the AXP2101 can produce, keyed by the YAML option. The
# channel names must match the table in axp2101.cpp; a mismatch fails at setup
# with a "not an ADC channel of this chip" message rather than publishing the
# wrong value.
SENSORS = {
    CONF_BATTERY_VOLTAGE: adc(
        "battery_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=3,
    ),
    CONF_BATTERY_LEVEL: state_of_charge(
        unit_of_measurement=UNIT_PERCENT,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=0,
        icon=ICON_BATTERY,
    ),
    CONF_VBUS_VOLTAGE: adc(
        "vbus_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_SYSTEM_VOLTAGE: adc(
        "system_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_TS_VOLTAGE: adc(
        "ts_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        accuracy_decimals=4,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_DIE_TEMPERATURE: adc(
        "die_temperature",
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=1,
        icon=ICON_THERMOMETER,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = schema(
    SENSORS,
    lambda definition: sensor.sensor_schema(definition.class_, **definition.options),
    # Accepted so configurations written for the older component still work.
    {cv.Optional(CONF_UPDATE_INTERVAL): cv.update_interval},
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AXP_ID])
    await add_entities(
        config,
        SENSORS,
        parent,
        sensor.new_sensor,
        lambda var, channel: cg.add(var.set_channel_name(channel)),
    )
    forward_update_interval(config, parent)
