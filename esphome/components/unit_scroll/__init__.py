import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@m5stack"]

unit_scroll_ns = cg.esphome_ns.namespace("unit_scroll")
UnitScrollComponent = unit_scroll_ns.class_("UnitScrollComponent", cg.Component, i2c.I2CDevice)

CONF_UNIT_SCROLL_ID = "unit_scroll_id"
CONF_DIRECTION = "direction"
CONF_RGB_RED = "rgb_red"
CONF_RGB_GREEN = "rgb_green"
CONF_RGB_BLUE = "rgb_blue"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UnitScrollComponent),
            cv.Optional(CONF_DIRECTION, default=False): cv.boolean,
            cv.Optional(CONF_RGB_RED, default=0): cv.int_range(min=0, max=255),
            cv.Optional(CONF_RGB_GREEN, default=255): cv.int_range(min=0, max=255),
            cv.Optional(CONF_RGB_BLUE, default=0): cv.int_range(min=0, max=255),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x40))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    
    # Set initial values
    cg.add(var.set_initial_direction(config[CONF_DIRECTION]))
    cg.add(var.set_initial_rgb_color(config[CONF_RGB_RED], config[CONF_RGB_GREEN], config[CONF_RGB_BLUE]))

