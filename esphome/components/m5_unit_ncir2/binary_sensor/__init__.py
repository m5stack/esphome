import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_BUTTON
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_NCIR2_ID, M5UnitNCIR2

DEPENDENCIES = ["m5_unit_ncir2"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_NCIR2_ID): cv.use_id(M5UnitNCIR2),
    cv.Optional(CONF_BUTTON): binary_sensor.binary_sensor_schema(),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_UNIT_NCIR2_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_BUTTON, hub.set_button_binary_sensor)
