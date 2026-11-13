"""Shared scaffolding for the AXP entity platforms.

Each chip declares a table of `entity definitions <EntityDef>` for a platform and
the platform module builds its schema and code generation from it. Nothing here
knows about a specific ESPHome platform: the caller passes the platform's
`schema` helper, its `new_*` factory and the setter for the model table entry.

Entities are registered with the hub and never poll on their own, so a chip with
many entities still costs one set of bus transactions per interval and the I2C
timing stays under the hub's control. `AXP_POLLED_ENTITY_COUNT` sizes the hub's
list; with no such entity the list and its registration method compile out.
"""

from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_UPDATE_INTERVAL
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

from . import AXP_BASE_SCHEMA

# Constants shared by the AXP chips. They live here because more than one chip
# uses them; `esphome/const.py` is frozen.
CONF_DIE_TEMPERATURE = "die_temperature"
CONF_SYSTEM_VOLTAGE = "system_voltage"
CONF_TS_VOLTAGE = "ts_voltage"
CONF_VBUS_VOLTAGE = "vbus_voltage"

_request_entity_slot = cg.slot_counter("AXP_POLLED_ENTITY_COUNT")


@dataclass(frozen=True)
class EntityDef:
    """One entity a chip can expose.

    :param class_: C++ entity class.
    :param source: entry name in the chip's model table, or None when the entity
        reads something that has no table entry.
    :param source_from_key: the configuration key is the table entry name, which
        is how the status flags are addressed.
    :param options: keyword arguments passed straight to the platform's
        `*_schema` helper.
    """

    class_: MockObjClass
    source: str | None = None
    source_from_key: bool = False
    options: dict = field(default_factory=dict)


def adc(class_: MockObjClass, channel: str, **options) -> EntityDef:
    """A sensor backed by an ADC channel of the chip."""
    return EntityDef(class_, channel, False, options)


def state_of_charge(class_: MockObjClass, **options) -> EntityDef:
    """The state of charge reported by the fuel gauge."""
    return EntityDef(class_, None, False, options)


def status(class_: MockObjClass, **options) -> EntityDef:
    """A binary sensor backed by the status flag named by its configuration key."""
    return EntityDef(class_, None, True, options)


def schema(
    definitions: dict[str, EntityDef],
    build: Callable[[EntityDef], cv.Schema],
    extra: dict[str, Any] | None = None,
) -> cv.Schema:
    """Platform schema for one chip and one platform.

    :param definitions: the chip's entity table for this platform.
    :param build: turns one definition into the platform's entity schema.
    :param extra: additional top level options for this platform.
    """
    config = dict(extra or {})
    for key, definition in definitions.items():
        config[cv.Optional(key)] = build(definition)
    return cv.Schema(config).extend(AXP_BASE_SCHEMA)


async def add_entities(
    config: ConfigType,
    definitions: dict[str, EntityDef],
    parent: MockObj,
    create: Callable[..., Any],
    apply_source: Callable[[MockObj, str], None],
) -> None:
    """Create the configured entities and register them with the hub.

    :param create: the platform's `new_*` coroutine, e.g. `sensor.new_sensor`.
    :param apply_source: names the model table entry on the entity, e.g.
        `lambda var, channel: cg.add(var.set_channel_name(channel))`.
    """
    for key, definition in definitions.items():
        if (conf := config.get(key)) is None:
            continue

        var = await create(conf)
        await cg.register_component(var, conf)
        await cg.register_parented(var, parent)
        if definition.source_from_key:
            apply_source(var, key)
        elif definition.source is not None:
            apply_source(var, definition.source)
        _request_entity_slot()
        cg.add(parent.add_polled_entity(var))


def forward_update_interval(config: ConfigType, parent: MockObj) -> None:
    """Give the hub an interval configured on an entity platform.

    Older configurations set `update_interval` on the sensor platform; the hub is
    what polls, so that is where the value belongs.
    """
    if (interval := config.get(CONF_UPDATE_INTERVAL)) is not None:
        cg.add(parent.set_update_interval(interval))


__all__ = [
    "CONF_DIE_TEMPERATURE",
    "CONF_SYSTEM_VOLTAGE",
    "CONF_TS_VOLTAGE",
    "CONF_VBUS_VOLTAGE",
    "EntityDef",
    "add_entities",
    "adc",
    "forward_update_interval",
    "schema",
    "state_of_charge",
    "status",
]
