import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_FREQUENCY, CONF_ID
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_NCIR2_ID, M5UnitNCIR2, m5_unit_ncir2_ns

DEPENDENCIES = ["m5_unit_ncir2"]

M5UnitNCIR2Buzzer = m5_unit_ncir2_ns.class_(
    "M5UnitNCIR2Buzzer", output.FloatOutput, cg.Parented.template(M5UnitNCIR2)
)

# The level sets the buzzer's duty cycle; rtttl changes the frequency for each note
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(M5UnitNCIR2Buzzer),
        cv.GenerateID(CONF_M5_UNIT_NCIR2_ID): cv.use_id(M5UnitNCIR2),
        cv.Optional(CONF_FREQUENCY, default="4000Hz"): cv.All(
            cv.frequency, cv.Range(min=20, max=20000)
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID], round(config[CONF_FREQUENCY]))
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_M5_UNIT_NCIR2_ID])
