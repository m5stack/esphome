import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID

from .. import BASE_SCHEMA, CONF_M5STACK_IOE1_ID, m5stack_ioe1_ns

M5StackIOE1AddressableLight = m5stack_ioe1_ns.class_(
    "M5StackIOE1AddressableLight", light.AddressableLight
)

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(M5StackIOE1AddressableLight),
        cv.Required(CONF_NUM_LEDS): cv.int_range(min=1, max=32),
    }
).extend(BASE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, config[CONF_M5STACK_IOE1_ID])
    await cg.register_component(var, config)
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
