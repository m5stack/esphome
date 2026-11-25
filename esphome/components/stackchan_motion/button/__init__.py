import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_STACKCHAN_MOTION_ID

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["stackchan_motion"]

CONF_HOME = "home"
CONF_LEFT = "left"
CONF_RIGHT = "right"
CONF_UP = "up"
CONF_DOWN = "down"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_HOME): button.button_schema(
            button.ApplyButton, icon="mdi:home-import-outline"
        ),
        cv.Optional(CONF_LEFT): button.button_schema(
            button.ApplyButton, icon="mdi:arrow-left-bold"
        ),
        cv.Optional(CONF_RIGHT): button.button_schema(
            button.ApplyButton, icon="mdi:arrow-right-bold"
        ),
        cv.Optional(CONF_UP): button.button_schema(
            button.ApplyButton, icon="mdi:arrow-up-bold"
        ),
        cv.Optional(CONF_DOWN): button.button_schema(
            button.ApplyButton, icon="mdi:arrow-down-bold"
        ),
    }
).extend(BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    motion = await cg.get_variable(config[CONF_STACKCHAN_MOTION_ID])
    buttons = button.sub_buttons(config, parent=motion)
    await buttons(CONF_HOME, motion.set_home_button, apply="home()")
    await buttons(CONF_LEFT, motion.set_left_button, apply="nudge_left()")
    await buttons(CONF_RIGHT, motion.set_right_button, apply="nudge_right()")
    await buttons(CONF_UP, motion.set_up_button, apply="nudge_up()")
    await buttons(CONF_DOWN, motion.set_down_button, apply="nudge_down()")
