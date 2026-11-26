import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_M5_UNIT_ACSSR_ID, M5UnitACSSR, m5_unit_acssr_ns

DEPENDENCIES = ["m5_unit_acssr"]

CONF_RELAY = "relay"

M5UnitACSSRRelaySwitch = m5_unit_acssr_ns.class_(
    "M5UnitACSSRRelaySwitch", switch.Switch, cg.Parented.template(M5UnitACSSR)
)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_M5_UNIT_ACSSR_ID): cv.use_id(M5UnitACSSR),
    # With restore_mode DISABLED the switch boots with the unit's own state
    cv.Optional(CONF_RELAY): switch.switch_schema(
        M5UnitACSSRRelaySwitch, icon="mdi:electric-switch"
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_M5_UNIT_ACSSR_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_RELAY, hub.set_relay_switch)
