import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL
from esphome.types import ConfigType

from .. import (
    CONF_M5_UNIT_8SERVOS2_CHAIN_ID,
    SERVOS2_CHANNEL_COUNT,
    M5Unit8Servos2Chain,
    claim_channel,
    m5_unit_8servos2_chain_ns,
)

CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["m5_unit_8servos2_chain"]

M5Unit8Servos2Switch = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2Switch",
    switch.Switch,
    cg.Component,
    cg.Parented.template(M5Unit8Servos2Chain),
)


def _validate(config: ConfigType) -> ConfigType:
    claim_channel(
        config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID], config[CONF_CHANNEL], "digital"
    )
    return config


CONFIG_SCHEMA = cv.All(
    switch.switch_schema(M5Unit8Servos2Switch).extend(
        {
            cv.Required(CONF_CHANNEL): cv.int_range(
                min=0, max=SERVOS2_CHANNEL_COUNT - 1
            ),
            cv.GenerateID(CONF_M5_UNIT_8SERVOS2_CHAIN_ID): cv.use_id(
                M5Unit8Servos2Chain
            ),
        }
    ),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
