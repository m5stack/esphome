import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_TYPE
from esphome.types import ConfigType

from .. import (
    CONF_M5_UNIT_8SERVOS2_CHAIN_ID,
    SERVOS2_CHANNEL_COUNT,
    M5Unit8Servos2Chain,
    claim_channel,
    m5_unit_8servos2_chain_ns,
)

CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["m5_unit_8servos2_chain"]

M5Unit8Servos2FloatOutput = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2FloatOutput",
    output.FloatOutput,
    cg.Component,
    cg.Parented.template(M5Unit8Servos2Chain),
)
M5Unit8Servos2BinaryOutput = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2BinaryOutput",
    output.BinaryOutput,
    cg.Component,
    cg.Parented.template(M5Unit8Servos2Chain),
)
Servos2OutputType = m5_unit_8servos2_chain_ns.enum("Servos2OutputType", is_class=True)

_CHANNEL = cv.int_range(min=0, max=SERVOS2_CHANNEL_COUNT - 1)
_HUB = {cv.GenerateID(CONF_M5_UNIT_8SERVOS2_CHAIN_ID): cv.use_id(M5Unit8Servos2Chain)}


def _schema(base: cv.Schema, class_) -> cv.Schema:
    return base.extend(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.Required(CONF_CHANNEL): _CHANNEL,
            **_HUB,
        }
    )


_FUNCTIONS = {"servo": "servo", "pwm": "pwm", "digital": "digital"}


def _validate(config: ConfigType) -> ConfigType:
    claim_channel(
        config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID],
        config[CONF_CHANNEL],
        _FUNCTIONS[config[CONF_TYPE]],
        allow_duplicate=config[CONF_TYPE] != "digital",
    )
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {
            "servo": _schema(output.FLOAT_OUTPUT_SCHEMA, M5Unit8Servos2FloatOutput),
            "pwm": _schema(output.FLOAT_OUTPUT_SCHEMA, M5Unit8Servos2FloatOutput),
            "digital": _schema(output.BINARY_OUTPUT_SCHEMA, M5Unit8Servos2BinaryOutput),
        },
        key=CONF_TYPE,
        lower=True,
    ),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID])
    await output.register_output(var, config)
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    if config[CONF_TYPE] == "servo":
        cg.add(var.set_output_type(Servos2OutputType.SERVOS2_OUTPUT_TYPE_SERVO))
    elif config[CONF_TYPE] == "pwm":
        cg.add(var.set_output_type(Servos2OutputType.SERVOS2_OUTPUT_TYPE_PWM))
