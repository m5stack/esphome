import esphome.codegen as cg
from esphome.components import ftservo
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID
from esphome.types import ConfigType

from .. import BASE_SCHEMA, CONF_SCS9009_ID, scs9009_ns

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["scs9009"]

SCS9009Servo = scs9009_ns.class_("SCS9009Servo", ftservo.FTServo)

# SCS9009 reports these fields; the current register is not part of the documented memory table.
SCS9009_CAPABILITIES = (
    ftservo.CAPABILITY_POSITION
    | ftservo.CAPABILITY_SPEED
    | ftservo.CAPABILITY_LOAD
    | ftservo.CAPABILITY_VOLTAGE
    | ftservo.CAPABILITY_TEMPERATURE
    | ftservo.CAPABILITY_MOVING
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SCS9009Servo),
            cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=253),
        }
    )
    .extend(BASE_SCHEMA)
    .extend(ftservo.SERVO_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SCS9009_ID])
    cg.add(var.set_address(config[CONF_ADDRESS]))
    await ftservo.register_ftservo(var, config, SCS9009_CAPABILITIES)
