import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5_unit_acssr"
MULTI_CONF = True

CONF_M5_UNIT_ACSSR_ID = "m5_unit_acssr_id"

m5_unit_acssr_ns = cg.esphome_ns.namespace("m5_unit_acssr")
M5UnitACSSR = m5_unit_acssr_ns.class_("M5UnitACSSR", i2c.I2CDevice, cg.PollingComponent)

# The Unit DCSSR runs the same firmware, so it is configured the same way
CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(M5UnitACSSR)})
    .extend(cv.polling_component_schema("10s"))
    .extend(i2c.i2c_device_schema(0x50))
)


async def to_code(config: ConfigType) -> None:
    await i2c.new_i2c_component(config)
