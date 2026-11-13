import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHANNEL,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_STEP,
    CONF_TYPE,
    UNIT_DEGREES,
    UNIT_PERCENT,
)
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

M5Unit8Servos2AngleNumber = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2AngleNumber",
    number.Number,
    cg.Component,
    cg.Parented.template(M5Unit8Servos2Chain),
)
M5Unit8Servos2DutyNumber = m5_unit_8servos2_chain_ns.class_(
    "M5Unit8Servos2DutyNumber",
    number.Number,
    cg.Component,
    cg.Parented.template(M5Unit8Servos2Chain),
)

_CHANNEL = cv.int_range(min=0, max=SERVOS2_CHANNEL_COUNT - 1)
_HUB = {cv.GenerateID(CONF_M5_UNIT_8SERVOS2_CHAIN_ID): cv.use_id(M5Unit8Servos2Chain)}

ANGLE_SCHEMA = number.number_schema(
    M5Unit8Servos2AngleNumber,
    icon="mdi:angle-acute",
    unit_of_measurement=UNIT_DEGREES,
).extend(
    {
        cv.Required(CONF_CHANNEL): _CHANNEL,
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=180.0): cv.float_,
        cv.Optional(CONF_STEP, default=1.0): cv.float_,
        **_HUB,
    }
)

DUTY_SCHEMA = number.number_schema(
    M5Unit8Servos2DutyNumber,
    icon="mdi:percent",
    unit_of_measurement=UNIT_PERCENT,
).extend(
    {
        cv.Required(CONF_CHANNEL): _CHANNEL,
        cv.Optional(CONF_MIN_VALUE, default=0.0): cv.float_,
        cv.Optional(CONF_MAX_VALUE, default=100.0): cv.float_,
        cv.Optional(CONF_STEP, default=1.0): cv.float_,
        **_HUB,
    }
)

_FUNCTIONS = {"angle": "servo", "duty": "pwm"}


def _validate(config: ConfigType) -> ConfigType:
    claim_channel(
        config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID],
        config[CONF_CHANNEL],
        _FUNCTIONS[config[CONF_TYPE]],
        allow_duplicate=True,
    )
    return config


CONFIG_SCHEMA = cv.All(
    cv.typed_schema(
        {"angle": ANGLE_SCHEMA, "duty": DUTY_SCHEMA},
        key=CONF_TYPE,
        lower=True,
    ),
    _validate,
)


async def to_code(config: ConfigType) -> None:
    var = await number.new_number(
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_8SERVOS2_CHAIN_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
