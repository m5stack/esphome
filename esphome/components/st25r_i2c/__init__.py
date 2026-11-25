import esphome.codegen as cg
from esphome.components import i2c, st25r
import esphome.config_validation as cv
from esphome.types import ConfigType

AUTO_LOAD = ["st25r"]
DEPENDENCIES = ["i2c"]
DOMAIN = "st25r_i2c"
MULTI_CONF = True

st25r_i2c_ns = cg.esphome_ns.namespace("st25r_i2c")
ST25RI2c = st25r_i2c_ns.class_("ST25RI2c", st25r.ST25R, i2c.I2CDevice)

CONFIG_SCHEMA = st25r.ST25R_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ST25RI2c),
    }
).extend(i2c.i2c_device_schema(0x50))


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await st25r.setup_st25r(var, config)
