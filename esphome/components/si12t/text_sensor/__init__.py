import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_SENSITIVITY
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import CONF_SI12T_ID, SI12TComponent, si12t_ns

DEPENDENCIES = ["si12t"]

CONF_HIGH_SENSITIVITY = "high_sensitivity"

ICON_GESTURE_TAP_BUTTON = "mdi:gesture-tap-button"

# Datasheet 12.2.1: the low nibble of a SENx register holds ChxM[2:0] and ChxHL in bit 3
CHANNEL_HIGH_SENSITIVITY = 1 << 3

SI12TTextSensor = si12t_ns.class_("SI12TTextSensor", text_sensor.TextSensor)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    SI12TTextSensor, icon=ICON_GESTURE_TAP_BUTTON
).extend(
    {
        cv.GenerateID(CONF_SI12T_ID): cv.use_id(SI12TComponent),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=12),
        cv.Optional(CONF_SENSITIVITY, default=3): cv.int_range(min=0, max=7),
        cv.Optional(CONF_HIGH_SENSITIVITY, default=True): cv.boolean,
    }
)


def _final_validate(config: ConfigType) -> None:
    """Reject a second text sensor on a channel an earlier one already reports."""
    for other in fv.full_config.get().get(text_sensor.DOMAIN, ()):
        # Stop at the entry being validated, so only the later one of a pair reports
        if other is config:
            return
        if other.get(CONF_SI12T_ID) != config[CONF_SI12T_ID]:
            continue
        if other.get(CONF_CHANNEL) == config[CONF_CHANNEL]:
            raise cv.Invalid(
                f"Channel {config[CONF_CHANNEL]} is already reported by another Si12T text sensor",
                path=[CONF_CHANNEL],
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SI12T_ID])
    var = await text_sensor.new_text_sensor(config)

    # Channels are numbered from 1 in the datasheet, the register bits from 0
    channel = config[CONF_CHANNEL] - 1
    sensitivity = config[CONF_SENSITIVITY] | (
        CHANNEL_HIGH_SENSITIVITY if config[CONF_HIGH_SENSITIVITY] else 0
    )
    cg.add(var.set_channel(channel))
    cg.add(var.set_sensitivity(sensitivity))
    cg.add(hub.set_channel_sensor(channel, var))
