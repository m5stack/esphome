import esphome.codegen as cg
from esphome.components import number
from esphome.const import (
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_AMPERE,
    UNIT_MILLISECOND,
    UNIT_PERCENT,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import EntityDef, Setting, add_setting_entities, aw88298_ns, schema

CONF_AGC_ATTACK_THRESHOLD = "agc_attack_threshold"
CONF_AGC_ATTACK_TIME = "agc_attack_time"
CONF_AGC_HOLD_TIME = "agc_hold_time"
CONF_AGC_RELEASE_THRESHOLD = "agc_release_threshold"
CONF_AGC_RELEASE_TIME = "agc_release_time"
CONF_BOOST_ATTACK_THRESHOLD = "boost_attack_threshold"
CONF_BOOST_CURRENT_LIMIT = "boost_current_limit"
CONF_BOOST_RELEASE_THRESHOLD = "boost_release_threshold"
CONF_BOOST_VOLTAGE = "boost_voltage"

AW88298Number = aw88298_ns.class_("AW88298Number", number.Number, cg.Component)

#: The numeric settings of the register map, with the range the register can hold. The step is
#: the size of one code, so every value the entity can take is one the device can hold.
NUMBERS = {
    CONF_BOOST_VOLTAGE: EntityDef(
        AW88298Number,
        Setting.SETTING_BOOST_VOLTAGE,
        {
            "unit_of_measurement": UNIT_VOLT,
            "device_class": DEVICE_CLASS_VOLTAGE,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 5.0, "max_value": 10.25, "step": 0.125},
    ),
    CONF_BOOST_CURRENT_LIMIT: EntityDef(
        AW88298Number,
        Setting.SETTING_BOOST_CURRENT_LIMIT,
        {
            "unit_of_measurement": UNIT_AMPERE,
            "device_class": DEVICE_CLASS_CURRENT,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 1.5, "max_value": 4.25, "step": 0.25},
    ),
    CONF_BOOST_ATTACK_THRESHOLD: EntityDef(
        AW88298Number,
        Setting.SETTING_BOOST_ATTACK_THRESHOLD,
        {
            "unit_of_measurement": UNIT_PERCENT,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 1, "max_value": 100, "step": 1},
    ),
    CONF_BOOST_RELEASE_THRESHOLD: EntityDef(
        AW88298Number,
        Setting.SETTING_BOOST_RELEASE_THRESHOLD,
        {
            "unit_of_measurement": UNIT_PERCENT,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 1, "max_value": 100, "step": 1},
    ),
    CONF_AGC_ATTACK_THRESHOLD: EntityDef(
        AW88298Number,
        Setting.SETTING_AGC_ATTACK_THRESHOLD,
        {
            "unit_of_measurement": UNIT_PERCENT,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 1, "max_value": 100, "step": 1},
    ),
    CONF_AGC_RELEASE_THRESHOLD: EntityDef(
        AW88298Number,
        Setting.SETTING_AGC_RELEASE_THRESHOLD,
        {
            "unit_of_measurement": UNIT_PERCENT,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 1, "max_value": 100, "step": 1},
    ),
    CONF_AGC_ATTACK_TIME: EntityDef(
        AW88298Number,
        Setting.SETTING_AGC_ATTACK_TIME,
        {
            "unit_of_measurement": UNIT_MILLISECOND,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 0.1, "max_value": 1365.0, "step": 0.1},
    ),
    CONF_AGC_RELEASE_TIME: EntityDef(
        AW88298Number,
        Setting.SETTING_AGC_RELEASE_TIME,
        {
            "unit_of_measurement": UNIT_MILLISECOND,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 0.1, "max_value": 1365.0, "step": 0.1},
    ),
    CONF_AGC_HOLD_TIME: EntityDef(
        AW88298Number,
        Setting.SETTING_AGC_HOLD_TIME,
        {
            "unit_of_measurement": UNIT_MILLISECOND,
            "entity_category": ENTITY_CATEGORY_CONFIG,
        },
        {"min_value": 1.4, "max_value": 339.0, "step": 1.4},
    ),
}

CONFIG_SCHEMA = schema(
    NUMBERS,
    lambda definition: number.number_schema(definition.class_, **definition.options),
)


async def _create(conf: ConfigType, definition: EntityDef):
    return await number.new_number(conf, **definition.create_options)


async def to_code(config: ConfigType) -> None:
    await add_setting_entities(config, NUMBERS, _create)
