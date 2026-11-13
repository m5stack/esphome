import esphome.codegen as cg
from esphome.components import select
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import EntityDef, Setting, add_setting_entities, aw88298_ns, schema

CONF_BOOST_DEGLITCH = "boost_deglitch"
CONF_BOOST_MODE = "boost_mode"

AW88298Select = aw88298_ns.class_("AW88298Select", select.Select, cg.Component)

#: The options are the order the datasheet lists them in, because the setting value is the index.
BOOST_MODES = ["pass_through", "force_boost", "smart_boost_1", "smart_boost_2"]
BOOST_DEGLITCH = ["0.33ms", "1.4ms", "5.6ms", "21.3ms", "44ms", "88ms", "352ms", "1.4s"]

SELECTS = {
    CONF_BOOST_MODE: EntityDef(
        AW88298Select,
        Setting.SETTING_BOOST_MODE,
        {"entity_category": ENTITY_CATEGORY_CONFIG, "icon": "mdi:sine-wave"},
        {"options": BOOST_MODES},
    ),
    CONF_BOOST_DEGLITCH: EntityDef(
        AW88298Select,
        Setting.SETTING_BOOST_DEGLITCH,
        {"entity_category": ENTITY_CATEGORY_CONFIG, "icon": "mdi:timer-sand"},
        {"options": BOOST_DEGLITCH},
    ),
}

CONFIG_SCHEMA = schema(
    SELECTS,
    lambda definition: select.select_schema(definition.class_, **definition.options),
)


async def _create(conf: ConfigType, definition: EntityDef):
    return await select.new_select(conf, options=definition.create_options["options"])


async def to_code(config: ConfigType) -> None:
    await add_setting_entities(config, SELECTS, _create)
