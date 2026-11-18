"""AXP192 regulator platform: one PMIC rail per regulator.

The `rails:` list keeps a whole PMIC in one configuration entry, while every
entry in it becomes its own `Regulator`. Options written on the entry act as
defaults for all rails, so a voltage shared by the whole chip is only written
once.
"""

import esphome.codegen as cg
from esphome.components import regulator
from esphome.components.axp_base import CONF_AXP_ID, AxpComponent
from esphome.components.axp_base.rails import CONF_RAIL, CONF_RAILS, rail_schema
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import RAIL_CHANNELS, axp192_ns

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["axp192", "regulator"]

AXP192Regulator = axp192_ns.class_("AXP192Regulator", regulator.Regulator)

RAIL_ENTRY_SCHEMA = regulator.regulator_schema(
    AXP192Regulator, rail_schema(RAIL_CHANNELS)
)


def _apply_entry_defaults(config: ConfigType) -> ConfigType:
    """Copy options written on the entry into rails that do not override them."""
    if not isinstance(config, dict):
        return config
    rails = config.get(CONF_RAILS)
    if isinstance(rails, dict):
        rails = [rails]
    elif not isinstance(rails, list):
        return config
    defaults = {
        key: config[key] for key in regulator.REGULATOR_DEFAULT_KEYS if key in config
    }
    if defaults:
        config[CONF_RAILS] = [
            {**defaults, **rail} if isinstance(rail, dict) else rail for rail in rails
        ]
    return config


CONFIG_SCHEMA = cv.All(
    _apply_entry_defaults,
    cv.Schema(
        {
            cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
            cv.Required(CONF_RAILS): cv.ensure_list(RAIL_ENTRY_SCHEMA),
            **regulator.REGULATOR_ENTRY_OPTIONS,
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_AXP_ID])
    for rail in config[CONF_RAILS]:
        var = cg.new_Pvariable(rail[CONF_ID])
        await regulator.register_regulator(var, rail)
        await cg.register_parented(var, parent)
        cg.add(var.set_rail_name(rail[CONF_RAIL]))
