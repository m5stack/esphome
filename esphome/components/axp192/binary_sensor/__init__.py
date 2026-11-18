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

from .. import axp192_ns

DEPENDENCIES = ["axp192"]

AXP192StatusBinarySensor = axp192_ns.class_(
    "AXP192StatusBinarySensor", binary_sensor.BinarySensor, cg.Component
)

status = partial(_status, AXP192StatusBinarySensor)

# The configuration key is also the status flag name, so it must match the
# STATUS_FLAGS table in axp192.cpp. Only the flags this chip reports are listed;
# the OTG VBUS session bits of REG 04H sit outside the cached block and are left
# to the vbus_session_* events.
BINARY_SENSORS = {
    "acin_present": status(
        device_class=DEVICE_CLASS_PLUG,
        icon="mdi:power-plug",
    ),
    "acin_good": status(
        icon="mdi:power-plug-outline",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "vbus_present": status(
        device_class=DEVICE_CLASS_PLUG,
        icon="mdi:usb",
    ),
    "vbus_good": status(
        icon="mdi:usb-port",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "vbus_above_vhold": status(
        icon="mdi:usb-port",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "battery_charging": status(
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
        icon="mdi:battery-charging",
    ),
    "battery_discharging": status(
        icon="mdi:battery-minus",
    ),
    "battery_present": status(
        icon=ICON_BATTERY,
    ),
    "acin_vbus_shorted": status(
        icon="mdi:electric-switch",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "boot_from_acin_vbus": status(
        icon="mdi:power",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "over_temperature": status(
        device_class=DEVICE_CLASS_PROBLEM,
        icon="mdi:thermometer-alert",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "battery_active_mode": status(
        icon="mdi:battery-sync",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "charge_current_limited": status(
        icon="mdi:current-ac",
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "power_on_mode_b": status(
        icon="mdi:power-settings",
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
