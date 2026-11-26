import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_BUZZER, CONF_M5_UNIT_NCIR2_ID, M5UnitNCIR2, m5_unit_ncir2_ns

DEPENDENCIES = ["m5_unit_ncir2"]

M5UnitNCIR2BuzzerSwitch = m5_unit_ncir2_ns.class_(
    "M5UnitNCIR2BuzzerSwitch", switch.Switch, cg.Parented.template(M5UnitNCIR2)
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_NCIR2_ID): cv.use_id(M5UnitNCIR2),
    # Sounds the tone the buzzer_frequency and buzzer_duty numbers set; it boots with the unit's state
    cv.Optional(CONF_BUZZER): switch.switch_schema(
        M5UnitNCIR2BuzzerSwitch, icon="mdi:bullhorn"
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_UNIT_NCIR2_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_BUZZER, hub.set_buzzer_switch)
