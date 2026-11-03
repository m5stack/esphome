import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_SWITCH_ID, M5ChainSwitch, m5_chain_switch_ns

DEPENDENCIES = ["m5_chain_switch"]

# The writable switch controls the device's 0xE1/0xE2 event-report setting. The
# slider's actual state is reported asynchronously by the device as 0xE0.
M5ChainSwitchEventTriggerSwitch = m5_chain_switch_ns.class_(
    "M5ChainSwitchEventTriggerSwitch",
    switch.Switch,
    cg.Parented.template(M5ChainSwitch),
)

CONFIG_SCHEMA = switch.switch_schema(M5ChainSwitchEventTriggerSwitch).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_SWITCH_ID): cv.use_id(M5ChainSwitch),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await switch.new_switch(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_SWITCH_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_event_trigger_switch(var))
