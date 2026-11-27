import esphome.codegen as cg
from esphome.components.lvgl.defines import add_lv_use
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["display"]
AUTO_LOAD = ["lvgl"]
DOMAIN = "m5_sticks3_ui"

add_lv_use("label")

m5_sticks3_ui_ns = cg.esphome_ns.namespace("m5_sticks3_ui")
M5Sticks3UI = m5_sticks3_ui_ns.class_("M5Sticks3UI", cg.Component)


CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(M5Sticks3UI)}).extend(
    cv.COMPONENT_SCHEMA
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
