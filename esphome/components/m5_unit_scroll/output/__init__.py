import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_CHANNEL
from .. import unit_scroll_ns, CONF_UNIT_SCROLL_ID

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["unit_scroll"]

UnitScrollOutput = unit_scroll_ns.class_("UnitScrollOutput", output.FloatOutput)
OutputChannel = unit_scroll_ns.enum("OutputChannel", is_class=True)

CHANNEL_MAP = {
    "rgb_red": OutputChannel.RGB_RED,
    "rgb_green": OutputChannel.RGB_GREEN,
    "rgb_blue": OutputChannel.RGB_BLUE,
}

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(UnitScrollOutput),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_MAP),
        cv.GenerateID(CONF_UNIT_SCROLL_ID): cv.use_id(cg.Component),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    
    parent = await cg.get_variable(config[CONF_UNIT_SCROLL_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
