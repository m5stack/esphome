import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from .. import aw87559_ns, BASE_SCHEMA, CONF_AW87559_ID

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["aw87559"]

ICON_SPEAKER = "mdi:speaker"

AW87559Switch = aw87559_ns.class_("AW87559Switch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(
        AW87559Switch, icon=ICON_SPEAKER, default_restore_mode="RESTORE_DEFAULT_ON"
    )
    .extend(BASE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_AW87559_ID])
