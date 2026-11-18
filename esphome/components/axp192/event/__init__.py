import logging

import esphome.codegen as cg
from esphome.components import event
from esphome.components.axp_base import (
    CONF_AXP_ID,
    CONF_EVENT_POLL_INTERVAL,
    AxpComponent,
)
import esphome.config_validation as cv
from esphome.const import CONF_IRQ_PIN
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import EVENT_NAMES, axp192_ns

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["axp192"]

CONF_EVENTS = "events"

AXP192Event = axp192_ns.class_("AXP192Event", event.Event, cg.Component)

#: Every interrupt source the chip has, unless the configuration narrows it.
DEFAULT_EVENTS = EVENT_NAMES

CONFIG_SCHEMA = (
    event.event_schema(AXP192Event)
    .extend(
        {
            cv.GenerateID(CONF_AXP_ID): cv.use_id(AxpComponent),
            # The event types Home Assistant sees are exactly these names, so
            # subscribing to a subset is the same as triggering only for it.
            cv.Optional(CONF_EVENTS, default=DEFAULT_EVENTS): cv.All(
                cv.ensure_list(cv.one_of(*EVENT_NAMES, upper=False)),
                cv.Length(min=1),
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _final_validate(config: ConfigType) -> None:
    # `fv.get_declaring_config` only exists from ESPHome 2026.11. On 2026.10 the
    # hub cannot be looked up from the entity config, so the advisory warning
    # below is skipped instead of failing the whole configuration.
    try:
        hub = fv.get_declaring_config(config[CONF_AXP_ID])
    except AttributeError:
        return
    if (
        hub[CONF_EVENT_POLL_INTERVAL].total_milliseconds == 0
        and CONF_IRQ_PIN not in hub
    ):
        _LOGGER.warning(
            "'%s: never' and no 'irq_pin' means the chip cannot report interrupts, "
            "so this entity will never fire",
            CONF_EVENT_POLL_INTERVAL,
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AXP_ID])

    var = await event.new_event(config, event_types=config[CONF_EVENTS])
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
