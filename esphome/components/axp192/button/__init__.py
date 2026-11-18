import esphome.codegen as cg
from esphome.components import button
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_POWER

from .. import CONF_RESET_GAUGE, CONF_SHUTDOWN, axp192_ns

DEPENDENCIES = ["axp192"]

AXP192Button = axp192_ns.class_("AXP192Button", button.Button, cg.Component)
ChipOperation = axp192_ns.enum("ChipOperation", is_class=True)  # `enum class` in C++

#: One-shot control -> (button settings, the enum value the entity runs).
#:
#: There is no restart button: the AXP192 has no register that starts the SoC
#: again, so `CHIP_OPERATIONS` has no entry for it either.
BUTTONS = {
    CONF_SHUTDOWN: (
        {"icon": ICON_POWER, "entity_category": ENTITY_CATEGORY_CONFIG},
        ChipOperation.AXP192_CHIP_OPERATION_POWER_OFF,
    ),
    CONF_RESET_GAUGE: (
        {"icon": "mdi:battery-sync", "entity_category": ENTITY_CATEGORY_CONFIG},
        ChipOperation.AXP192_CHIP_OPERATION_RESET_GAUGE,
    ),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
        **{
            cv.Optional(key): button.button_schema(AXP192Button, **settings)
            for key, (settings, _) in BUTTONS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AXP_ID])

    for key, (_, operation) in BUTTONS.items():
        if (conf := config.get(key)) is None:
            continue
        var = await button.new_button(conf)
        await cg.register_parented(var, hub)
        cg.add(var.set_operation(operation))
