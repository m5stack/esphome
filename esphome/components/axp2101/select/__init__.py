import esphome.codegen as cg
from esphome.components import select
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_OPTION, ENTITY_CATEGORY_CONFIG

from .. import axp2101_ns

DEPENDENCIES = ["axp2101"]

AXP2101Select = axp2101_ns.class_("AXP2101Select", select.Select, cg.Component)

#: Options of each setting, in the order of its value table in axp2101.cpp. A
#: test pins the labels against those tables, so a wrong value here fails the
#: unit tests instead of silently selecting the neighbouring current.
SETTINGS = {
    "charge_current": [
        "100mA",
        "125mA",
        "150mA",
        "175mA",
        "200mA",
        "300mA",
        "400mA",
        "500mA",
        "600mA",
        "700mA",
        "800mA",
        "900mA",
        "1000mA",
    ],
    "charge_voltage": ["4.0V", "4.1V", "4.2V", "4.35V", "4.4V"],
    "input_current_limit": [
        "100mA",
        "500mA",
        "900mA",
        "1000mA",
        "1500mA",
        "2000mA",
    ],
    "power_key_long_press": ["1s", "1.5s", "2s", "2.5s"],
}


def _initial_option(options: list[str]):
    """A label from the setting's own option list, applied during setup()."""

    def validate(value) -> str:
        value = cv.string_strict(value)
        if value not in options:
            raise cv.Invalid(f"'{value}' is not one of: {', '.join(options)}")
        return value

    return validate


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
        **{
            cv.Optional(name): select.select_schema(
                AXP2101Select, entity_category=ENTITY_CATEGORY_CONFIG
            ).extend({cv.Optional(CONF_INITIAL_OPTION): _initial_option(options)})
            for name, options in SETTINGS.items()
        },
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_AXP_ID])

    # `sub_selects(...)(name, None, options=...)` relies on the ESPHome 2026.11
    # SubEntities API, so the entity is created directly: `new_select` has the
    # same signature on 2026.10 and later.
    for name, options in SETTINGS.items():
        if (conf := config.get(name)) is None:
            continue
        var = await select.new_select(conf, options=options)
        await cg.register_parented(var, hub)
        await cg.register_component(var, conf)
        cg.add(var.set_select_name(name))
        if (initial := conf.get(CONF_INITIAL_OPTION)) is not None:
            cg.add(var.set_initial_option_index(options.index(initial)))
