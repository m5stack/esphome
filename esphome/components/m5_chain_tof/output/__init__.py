import esphome.codegen as cg
from esphome.components import output
from esphome.components.m5_chain_base.output import CHANNEL_MAP, M5ChainLedOutput
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from .. import M5ChainToF

CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["m5_chain_tof"]

CONF_M5_CHAIN_TOF_ID = "m5_chain_tof_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(M5ChainLedOutput),
        cv.GenerateID(CONF_M5_CHAIN_TOF_ID): cv.use_id(M5ChainToF),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_MAP),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5_CHAIN_TOF_ID])
    await output.register_output(var, config)
    cg.add(var.set_channel(CHANNEL_MAP[config[CONF_CHANNEL]]))
