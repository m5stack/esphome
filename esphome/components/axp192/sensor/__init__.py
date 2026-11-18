from functools import partial

import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.axp_base import CONF_AXP_ID
from esphome.components.axp_base.entities import (
    CONF_DIE_TEMPERATURE,
    CONF_TS_VOLTAGE,
    CONF_VBUS_VOLTAGE,
    adc as _adc,
    add_entities,
    forward_update_interval,
    schema,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BATTERY_VOLTAGE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_VOLT,
    UNIT_WATT,
)

from .. import axp192_ns

DEPENDENCIES = ["axp192"]

AXP192Sensor = axp192_ns.class_("AXP192Sensor", sensor.Sensor, cg.Component)
AXP192AdcSensor = axp192_ns.class_("AXP192AdcSensor", AXP192Sensor)

adc = partial(_adc, AXP192AdcSensor)

CONF_ACIN_VOLTAGE = "acin_voltage"
CONF_ACIN_CURRENT = "acin_current"
CONF_VBUS_CURRENT = "vbus_current"
CONF_APS_VOLTAGE = "aps_voltage"
CONF_BATTERY_CHARGE_CURRENT = "battery_charge_current"
CONF_BATTERY_DISCHARGE_CURRENT = "battery_discharge_current"
CONF_BATTERY_POWER = "battery_power"

# Every measurement the AXP192 can produce, keyed by the YAML option. The channel
# names must match the table in axp192.cpp; a mismatch fails at setup with a
# "not an ADC channel of this chip" message rather than publishing the wrong
# value.
#
# The GPIO0-3 channels are readable through the hub API but are not offered here:
# measuring one means switching the pin to the ADC function, which would take it
# away from whatever uses it as a digital pin.
SENSORS = {
    CONF_BATTERY_VOLTAGE: adc(
        "battery_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=3,
    ),
    CONF_BATTERY_CHARGE_CURRENT: adc(
        "battery_charge_current",
        unit_of_measurement=UNIT_AMPERE,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_BATTERY_DISCHARGE_CURRENT: adc(
        "battery_discharge_current",
        unit_of_measurement=UNIT_AMPERE,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_BATTERY_POWER: adc(
        "battery_power",
        unit_of_measurement=UNIT_WATT,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_ACIN_VOLTAGE: adc(
        "acin_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_ACIN_CURRENT: adc(
        "acin_current",
        unit_of_measurement=UNIT_AMPERE,
        device_class=DEVICE_CLASS_CURRENT,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_VBUS_VOLTAGE: adc(
        "vbus_voltage",
        unit_of_measurement=UNIT_VOLT,
        device_class=DEVICE_CLASS_VOLTAGE,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_VBUS_CURRENT: adc(
        "vbus_current",
        unit_of_measurement=UNIT_AMPERE,
        device_class=DEVICE_CLASS_CURRENT,
        accuracy_decimals=3,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    CONF_APS_VOLTAGE: adc(
        "aps_voltage",
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
