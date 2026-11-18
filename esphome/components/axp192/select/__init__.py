import esphome.codegen as cg
from esphome.components import select
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_OPTION, ENTITY_CATEGORY_CONFIG

from .. import axp192_ns

DEPENDENCIES = ["axp192"]

AXP192Select = axp192_ns.class_("AXP192Select", select.Select, cg.Component)

#: Options of each setting, in the order of its value table in axp192.cpp. A
#: test pins the labels against those tables, so a wrong value here fails the
#: unit tests instead of silently selecting the neighbouring current.
SETTINGS = {
    "charge_current": [
        "100mA",
        "190mA",
        "280mA",
        "360mA",
        "450mA",
        "550mA",
        "630mA",
        "700mA",
        "780mA",
        "880mA",
        "960mA",
        "1000mA",
        "1080mA",
        "1160mA",
        "1240mA",
        "1320mA",
    ],
    "charge_voltage": ["4.1V", "4.15V", "4.2V", "4.36V"],
    "charge_termination_current": ["10%", "15%"],
    "vbus_current_limit": ["none", "500mA", "100mA"],
    "vbus_vhold": ["4.0V", "4.1V", "4.2V", "4.3V", "4.4V", "4.5V", "4.6V", "4.7V"],
    "precharge_timeout": ["30min", "40min", "50min", "60min"],
    "constant_current_timeout": ["7h", "8h", "9h", "10h"],
    "power_key_on_time": ["128ms", "512ms", "1s", "2s"],
    "power_key_long_press": ["1s", "1.5s", "2s", "2.5s"],
    "power_key_off_time": ["4s", "6s", "8s", "10s"],
    "backup_battery_voltage": ["3.1V", "3.0V", "2.5V"],
    "backup_battery_current": ["50uA", "100uA", "200uA", "400uA"],
    "vbus_valid_voltage": ["4.0V", "4.15V", "4.45V", "4.55V"],
}

#: Settings whose value is a plain label rather than a number, so the test that
#: compares the labels against the value table skips them.
UNLABELLED_VALUES = {"vbus_current_limit": {"none": 0}}


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
                AXP192Select, entity_category=ENTITY_CATEGORY_CONFIG
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
