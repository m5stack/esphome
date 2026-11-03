import esphome.codegen as cg
from esphome.components import output
from esphome.components.m5_chain_base.output import CHANNEL_MAP, M5ChainLedOutput
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_BUZZER_ID, M5ChainBuzzer

CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["m5_chain_buzzer"]

M5ChainBuzzerOutput = cg.esphome_ns.namespace("m5_chain_buzzer").class_(
    "M5ChainBuzzerOutput", output.FloatOutput, cg.Parented.template(M5ChainBuzzer)
)

_HUB_SCHEMA = {cv.GenerateID(CONF_M5_CHAIN_BUZZER_ID): cv.use_id(M5ChainBuzzer)}

# Without "channel" this is the speaker output, which rtttl drives with a frequency
# and a level. With "channel" it is one of the onboard RGB LED channels.
TONE_OUTPUT_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(M5ChainBuzzerOutput),
        **_HUB_SCHEMA,
    }
)

LED_OUTPUT_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(M5ChainLedOutput),
        cv.Required(CONF_CHANNEL): cv.enum(CHANNEL_MAP),
        **_HUB_SCHEMA,
    }
)

# The LED schema goes first so a bad "channel" value is reported as such.
CONFIG_SCHEMA = cv.Any(LED_OUTPUT_SCHEMA, TONE_OUTPUT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_parented(var, config[CONF_M5_CHAIN_BUZZER_ID])
    await output.register_output(var, config)
    if (channel := config.get(CONF_CHANNEL)) is not None:
        cg.add(var.set_channel(CHANNEL_MAP[channel]))
