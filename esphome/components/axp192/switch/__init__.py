import esphome.codegen as cg
from esphome.components import switch
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
from esphome.components.axp_base.rails import (
    CONF_RAIL,
    final_validate_rail_claims,
    rail_schema,
)
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import RAIL_CHANNELS, axp192_ns

DEPENDENCIES = ["axp192"]

CONF_CHARGING = "charging"
CONF_GAUGE = "gauge"

AXP192Switch = axp192_ns.class_("AXP192Switch", switch.Switch, cg.Component)
AXP192FlagSwitch = axp192_ns.class_("AXP192FlagSwitch", switch.Switch, cg.Component)

#: The chip's on/off settings, one switch entity each. The keys must match the
#: FLAGS table in axp192.cpp; a test pins the two together.
FLAGS = {
    CONF_CHARGING: {
        "icon": "mdi:battery-charging",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    CONF_GAUGE: {"icon": "mdi:battery-sync", "entity_category": ENTITY_CATEGORY_CONFIG},
    "battery_monitor": {
        "icon": "mdi:battery-heart-variant",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "over_temperature_shutdown": {
        "icon": "mdi:thermometer-alert",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "vbus_vhold_enable": {
        "icon": "mdi:usb-port",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "vbus_ignore_n_vbusen": {
        "icon": "mdi:electric-switch",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "backup_battery_charging": {
        "icon": "mdi:battery-clock",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "charge_external_channel": {
        "icon": "mdi:current-dc",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "dcdc1_force_pwm": {
        "icon": "mdi:sine-wave",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "dcdc2_force_pwm": {
        "icon": "mdi:sine-wave",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
    "dcdc3_force_pwm": {
        "icon": "mdi:sine-wave",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
}

_AXP_ID = {cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent)}

RAIL_ENTRY_SCHEMA = (
    switch.switch_schema(AXP192Switch)
    .extend(rail_schema(RAIL_CHANNELS))
    .extend(_AXP_ID)
)

FLAG_ENTRY_SCHEMA = cv.Schema(
    {
        **_AXP_ID,
        **{
            cv.Optional(key): switch.switch_schema(
                AXP192FlagSwitch,
                # The chip keeps these set across a reboot, so unless a restore
                # mode is asked for the switch only reports what it finds.
                default_restore_mode="DISABLED",
                **settings,
            )
            for key, settings in FLAGS.items()
        },
    }
)


def _validate_entry(config: ConfigType) -> ConfigType:
    if CONF_RAIL in config:
        return RAIL_ENTRY_SCHEMA(config)
    # Any key at all means the entry is meant to be a setting, so let the flag
    # schema report a misspelled name as an unknown option.
    if len(config) > (1 if CONF_AXP_ID in config else 0):
        return FLAG_ENTRY_SCHEMA(config)
    raise cv.Invalid(f"'{CONF_RAIL}' or one of {', '.join(FLAGS)} is required")


CONFIG_SCHEMA = _validate_entry

FINAL_VALIDATE_SCHEMA = final_validate_rail_claims("axp192", child_keys=tuple(FLAGS))


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_AXP_ID])

    if (rail := config.get(CONF_RAIL)) is not None:
        var = await switch.new_switch(config)
        await cg.register_component(var, config)
        await cg.register_parented(var, hub)
        cg.add(var.set_rail_name(rail))
        return

    # `sub_switches(...)(key, None)` needs the ESPHome 2026.11 SubEntities API, so
    # the entities are created directly; `new_switch` keeps the same signature on
    # 2026.10 and later.
    for key in FLAGS:
        if (conf := config.get(key)) is None:
            continue
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_flag_name(key))
