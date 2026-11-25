import esphome.codegen as cg
from esphome.components import switch
from esphome.types import ConfigType

from .. import CONF_FTSERVO_ID, FTSERVO_BASE_SCHEMA

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["ftservo"]

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    icon="mdi:lock",
    default_restore_mode="RESTORE_DEFAULT_OFF",
).extend(FTSERVO_BASE_SCHEMA)


async def to_code(config: ConfigType) -> None:
    servo = await cg.get_variable(config[CONF_FTSERVO_ID])
    switches = switch.sub_switches(config, parent=servo)
    await switches.create(
        config,
        servo.set_torque_switch,
        apply="set_torque(state)",
        publish="on_success",
    )
