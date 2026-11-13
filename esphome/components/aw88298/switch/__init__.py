import esphome.codegen as cg
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import EntityDef, Setting, add_setting_entities, aw88298_ns, schema

CONF_AGC = "agc"
CONF_AGC_RMS = "agc_rms"
CONF_HDCC = "hdcc"
CONF_INPUT_ATTENUATION = "input_attenuation"
CONF_RECEIVER_MODE = "receiver_mode"

AW88298Switch = aw88298_ns.class_("AW88298Switch", switch.Switch, cg.Component)

#: The on/off settings of the register map that are useful to change at runtime.
SWITCHES = {
    CONF_HDCC: EntityDef(AW88298Switch, Setting.SETTING_HDCC),
    CONF_AGC: EntityDef(AW88298Switch, Setting.SETTING_AGC),
    CONF_AGC_RMS: EntityDef(AW88298Switch, Setting.SETTING_AGC_RMS),
    CONF_INPUT_ATTENUATION: EntityDef(AW88298Switch, Setting.SETTING_INPUT_ATTENUATION),
    CONF_RECEIVER_MODE: EntityDef(AW88298Switch, Setting.SETTING_RECEIVER_MODE),
}

CONFIG_SCHEMA = schema(
    SWITCHES,
    lambda definition: switch.switch_schema(
        definition.class_,
        # The register keeps its value across a reboot, so without a restore mode the switch
        # only reports what the amplifier is set to.
        default_restore_mode="DISABLED",
        entity_category=ENTITY_CATEGORY_CONFIG,
    ),
)


async def _create(conf: ConfigType, definition: EntityDef):
    return await switch.new_switch(conf)


async def to_code(config: ConfigType) -> None:
    await add_setting_entities(config, SWITCHES, _create)
