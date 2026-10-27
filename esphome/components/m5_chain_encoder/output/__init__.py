import esphome.codegen as cg
from esphome.components import output
from esphome.components.m5_chain_base.output import CHANNEL_MAP, M5ChainLedOutput
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from .. import M5ChainEncoder

CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["m5_chain_encoder"]

CONF_M5_CHAIN_ENCODER_ID = "m5_chain_encoder_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(M5ChainLedOutput),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_MAP),
        cv.GenerateID(CONF_M5_CHAIN_ENCODER_ID): cv.use_id(M5ChainEncoder),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5_CHAIN_ENCODER_ID])
    await output.register_output(var, config)
    cg.add(var.set_channel(CHANNEL_MAP[config[CONF_CHANNEL]]))
