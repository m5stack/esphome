import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_SI5351_ID, Si5351Component, si5351_ns

DEPENDENCIES = ["si5351"]

Si5351StatusSensor = si5351_ns.class_(
    "Si5351StatusSensor", binary_sensor.BinarySensor, cg.PollingComponent
)

#: Register 0 of the Si5351, see AN619.
STATUS_TYPES = {
    "sys_init": 7,
    "lol_b": 6,
    "lol_a": 5,
    "los_clkin": 4,
    "los_xtal": 3,
}

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(Si5351StatusSensor)
    .extend(
        {
            cv.GenerateID(CONF_SI5351_ID): cv.use_id(Si5351Component),
            cv.Required(CONF_TYPE): cv.one_of(*STATUS_TYPES, lower=True),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_SI5351_ID])
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_status_bit(STATUS_TYPES[config[CONF_TYPE]]))
