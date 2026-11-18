from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import number
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_MILLIVOLT, UNIT_MINUTE

from .. import axp192_ns

DEPENDENCIES = ["axp192"]

AXP192Number = axp192_ns.class_("AXP192Number", number.Number, cg.Component)

CONF_TIMER_MINUTES = "timer_minutes"


@dataclass(frozen=True)
class Setting:
    """One linearly scaled setting, in the units the model table stores."""

    min_value: float
    max_value: float
    step: float
    unit_of_measurement: str
    icon: str


#: Bounds of each setting, matching its scale in axp192.cpp. A test derives them
#: from the model table, so a wrong bound here fails the unit tests rather than
#: letting a number write a code the chip reserves.
SETTINGS: dict[str, Setting] = {
    "voff_voltage": Setting(2600, 3300, 100, UNIT_MILLIVOLT, "mdi:battery-off-outline"),
    "gpio1_adc_irq_rise": Setting(0, 2040, 8, UNIT_MILLIVOLT, "mdi:arrow-up-bold"),
    "gpio1_adc_irq_fall": Setting(0, 2040, 8, UNIT_MILLIVOLT, "mdi:arrow-down-bold"),
    CONF_TIMER_MINUTES: Setting(0, 127, 1, UNIT_MINUTE, "mdi:timer-outline"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
        **{
            cv.Optional(name): number.number_schema(
                AXP192Number,
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
