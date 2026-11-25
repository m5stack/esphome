import esphome.codegen as cg
from esphome.components import ftservo
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["ftservo"]
DOMAIN = "ftservo_group"
MULTI_CONF = True

CONF_SERVOS = "servos"

ftservo_ns = cg.esphome_ns.namespace("ftservo")
FTServoGroup = ftservo_ns.class_("FTServoGroup", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(FTServoGroup),
        cv.Required(CONF_SERVOS): cv.All(
            cv.ensure_list(cv.use_id(ftservo.FTServo)), cv.Length(min=1, max=8)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_expected_servos(len(config[CONF_SERVOS])))
    for servo_id in config[CONF_SERVOS]:
        servo = await cg.get_variable(servo_id)
        cg.add(var.add_servo(servo))
