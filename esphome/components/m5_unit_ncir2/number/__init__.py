import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_MILLISECOND,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_NCIR2_ID, M5UnitNCIR2, m5_unit_ncir2_ns

DEPENDENCIES = ["m5_unit_ncir2"]

M5UnitNCIR2Number = m5_unit_ncir2_ns.class_(
    "M5UnitNCIR2Number", number.Number, cg.Parented.template(M5UnitNCIR2)
)
Setting = m5_unit_ncir2_ns.enum("Setting", is_class=True)

_TEMPERATURE = {
    "unit_of_measurement": UNIT_CELSIUS,
    "device_class": DEVICE_CLASS_TEMPERATURE,
    "entity_category": ENTITY_CATEGORY_CONFIG,
}
_FREQUENCY = {"unit_of_measurement": UNIT_HERTZ, "device_class": DEVICE_CLASS_FREQUENCY}
_INTERVAL = {
    "unit_of_measurement": UNIT_MILLISECOND,
    "device_class": DEVICE_CLASS_DURATION,
    "entity_category": ENTITY_CATEGORY_CONFIG,
}
_DUTY = {"unit_of_measurement": UNIT_PERCENT, "icon": "mdi:volume-high"}
_ALARM_FREQUENCY = {**_FREQUENCY, "entity_category": ENTITY_CATEGORY_CONFIG}
_ALARM_DUTY = {**_DUTY, "entity_category": ENTITY_CATEGORY_CONFIG}

# key: (setting, schema arguments, min, max, step)
NUMBERS = {
    "low_alarm_temperature": (
        Setting.SETTING_LOW_ALARM_TEMPERATURE,
        _TEMPERATURE,
        -70.0,
        380.0,
        0.1,
    ),
    "high_alarm_temperature": (
        Setting.SETTING_HIGH_ALARM_TEMPERATURE,
        _TEMPERATURE,
        -70.0,
        380.0,
        0.1,
    ),
    "low_alarm_frequency": (
        Setting.SETTING_LOW_ALARM_FREQUENCY,
        _ALARM_FREQUENCY,
        20,
        20000,
        1,
    ),
    "high_alarm_frequency": (
        Setting.SETTING_HIGH_ALARM_FREQUENCY,
        _ALARM_FREQUENCY,
        20,
        20000,
        1,
    ),
    "low_alarm_interval": (Setting.SETTING_LOW_ALARM_INTERVAL, _INTERVAL, 1, 5000, 1),
    "high_alarm_interval": (Setting.SETTING_HIGH_ALARM_INTERVAL, _INTERVAL, 1, 5000, 1),
    "low_alarm_duty": (Setting.SETTING_LOW_ALARM_DUTY, _ALARM_DUTY, 0, 100, 1),
    "high_alarm_duty": (Setting.SETTING_HIGH_ALARM_DUTY, _ALARM_DUTY, 0, 100, 1),
    # The tone the buzzer switch sounds
    "buzzer_frequency": (Setting.SETTING_BUZZER_FREQUENCY, _FREQUENCY, 20, 20000, 1),
    "buzzer_duty": (Setting.SETTING_BUZZER_DUTY, _DUTY, 0, 100, 1),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_M5_UNIT_NCIR2_ID): cv.use_id(M5UnitNCIR2),
        **{
            cv.Optional(key): number.number_schema(M5UnitNCIR2Number, **schema)
            for key, (_, schema, _, _, _) in NUMBERS.items()
        },
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_UNIT_NCIR2_ID])
    numbers = number.sub_numbers(config, parent=hub)
    for key, (setting, _, min_value, max_value, step) in NUMBERS.items():
        await numbers(
            key,
            lambda var, setting=setting: hub.set_setting_number(setting, var),
            setting,
            min_value=min_value,
            max_value=max_value,
            step=step,
        )
