from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import number
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_MILLIVOLT, UNIT_PERCENT

from .. import axp2101_ns

DEPENDENCIES = ["axp2101"]

AXP2101Number = axp2101_ns.class_("AXP2101Number", number.Number, cg.Component)


@dataclass(frozen=True)
class Setting:
    """One linearly scaled setting, in the units the model table stores."""

    min_value: float
    max_value: float
    step: float
    unit_of_measurement: str
    icon: str


#: Bounds of each setting, matching its scale in axp2101.cpp. A test derives
#: them from the model table, so a wrong bound here fails the unit tests rather
#: than letting a number write a code the chip reserves.
SETTINGS: dict[str, Setting] = {
    "battery_low_warning": Setting(
        5, 20, 1, UNIT_PERCENT, "mdi:battery-alert-variant-outline"
    ),
    "battery_low_shutdown": Setting(0, 15, 1, UNIT_PERCENT, "mdi:battery-off-outline"),
    "input_voltage_limit": Setting(3880, 5080, 80, UNIT_MILLIVOLT, "mdi:power-plug"),
    "precharge_current": Setting(0, 200, 25, "mA", "mdi:battery-plus"),
    "termination_current": Setting(0, 200, 25, "mA", "mdi:battery-check"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
        **{
            cv.Optional(name): number.number_schema(
                AXP2101Number,
                entity_category=ENTITY_CATEGORY_CONFIG,
                unit_of_measurement=setting.unit_of_measurement,
                icon=setting.icon,
            )
            for name, setting in SETTINGS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AXP_ID])

    # See select/__init__.py: `new_number` keeps the same signature across the
    # SubEntities change in ESPHome 2026.11, so it is called directly.
    for name, setting in SETTINGS.items():
        if (conf := config.get(name)) is None:
            continue
        var = await number.new_number(
            conf,
            min_value=setting.min_value,
            max_value=setting.max_value,
            step=setting.step,
        )
        await cg.register_parented(var, hub)
        await cg.register_component(var, conf)
        cg.add(var.set_number_name(name))
