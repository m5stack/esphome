from functools import partial

import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.axp_base import CONF_AXP_ID
from esphome.components.axp_base.entities import add_entities, schema, status as _status
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_BATTERY,
)

from .. import axp2101_ns

DEPENDENCIES = ["axp2101"]

AXP2101StatusBinarySensor = axp2101_ns.class_(
    "AXP2101StatusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

status = partial(_status, AXP2101StatusBinarySensor)

# The configuration key is also the status flag name, so it must match the
# STATUS_FLAGS table in axp2101.cpp. Only the flags this chip actually reports
# are listed: it has no separate "VBUS present" bit, and over temperature is
# signalled through an interrupt rather than a status bit.
BINARY_SENSORS = {
    "battery_present": status(
        icon=ICON_BATTERY,
    ),
    "battery_charging": status(
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
        icon="mdi:battery-charging",
    ),
    "battery_discharging": status(
        icon="mdi:battery-minus",
    ),
    "vbus_good": status(
        device_class=DEVICE_CLASS_PLUG,
        icon="mdi:usb",
    ),
    "battery_active_mode": status(
        icon="mdi:battery-sync",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "thermal_regulation": status(
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:thermometer-alert",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "current_limit": status(
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:current-ac",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = schema(
    BINARY_SENSORS,
    lambda definition: binary_sensor.binary_sensor_schema(
        definition.class_, **definition.options
    ),
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AXP_ID])
    await add_entities(
        config,
        BINARY_SENSORS,
        parent,
        binary_sensor.new_binary_sensor,
        lambda var, flag: cg.add(var.set_flag_name(flag)),
    )
