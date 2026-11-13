import esphome.codegen as cg
from esphome.components import button
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
import esphome.config_validation as cv
from esphome.const import (
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_POWER,
    ICON_RESTART,
)

from .. import CHIP_OPERATIONS, CONF_RESET_GAUGE, CONF_SHUTDOWN

DEPENDENCIES = ["axp2101"]

#: Settings for the button that runs each operation. All three take the device
#: down or recalibrate it, so they sit under the configuration category.
BUTTON_SETTINGS = {
    CONF_SHUTDOWN: {"icon": ICON_POWER, "entity_category": ENTITY_CATEGORY_CONFIG},
    CONF_RESTART: {
        "icon": ICON_RESTART,
        "entity_category": ENTITY_CATEGORY_CONFIG,
        "device_class": DEVICE_CLASS_RESTART,
    },
    CONF_RESET_GAUGE: {
        "icon": "mdi:battery-sync",
        "entity_category": ENTITY_CATEGORY_CONFIG,
    },
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
        **{
            cv.Optional(key): button.button_schema(button.ApplyButton, **settings)
            for key, settings in BUTTON_SETTINGS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AXP_ID])

    # The buttons keep no state, so they run the hub method directly instead of
    # going through a class of their own.
    buttons = button.sub_buttons(config, parent=hub)
    for key, call in CHIP_OPERATIONS.items():
        await buttons(key, None, apply=call)
