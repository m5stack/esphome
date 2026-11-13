"""Awinic AW88298 I2S/TDM input smart amplifier.

The amplifier is created by the `audio_dac` platform, which takes the I2S settings and the
settings that describe how the speaker is used. The entity platforms in this directory expose
the rest of the register map: the measurements, the status bits and the settings that are
useful to change at runtime.
"""

from collections.abc import Callable
from dataclasses import dataclass, field
from typing import Any

import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType

DOMAIN = "aw88298"
CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]

#: Every entity platform uses this to point at the amplifier it belongs to.
CONF_AW88298_ID = "aw88298_id"

aw88298_ns = cg.esphome_ns.namespace("aw88298")
AW88298 = aw88298_ns.class_("AW88298", AudioDac, cg.PollingComponent, i2c.I2CDevice)
Setting = aw88298_ns.enum("Setting", is_class=True)
SensorType = aw88298_ns.enum("SensorType", is_class=True)
StatusFlag = aw88298_ns.enum("StatusFlag", is_class=True)

#: The hub keeps one slot per configured sensor and status binary sensor, and the defines are
#: emitted after every entity platform has registered. Nothing registered, nothing allocated.
request_sensor_slot = cg.slot_counter("AW88298_SENSOR_COUNT")
request_status_sensor_slot = cg.slot_counter("AW88298_STATUS_SENSOR_COUNT")

AW88298_SCHEMA = cv.Schema({cv.GenerateID(CONF_AW88298_ID): cv.use_id(AW88298)})


@dataclass(frozen=True)
class EntityDef:
    """One entity a platform can expose.

    :param class_: C++ entity class.
    :param source: the C++ enum value that names what the entity reads or writes.
    :param options: keyword arguments passed straight to the platform's `*_schema` helper.
    :param create_options: keyword arguments passed to the platform's `new_*` factory.
    """

    class_: MockObjClass
    source: MockObj
    options: dict = field(default_factory=dict)
    create_options: dict = field(default_factory=dict)


def schema(
    definitions: dict[str, EntityDef], build: Callable[[EntityDef], Any]
) -> cv.Schema:
    """Platform schema for one entity platform.

    :param definitions: the entity table of the platform.
    :param build: turns one definition into the platform's entity schema.
    """
    config = {
        cv.Optional(key): build(definition) for key, definition in definitions.items()
    }
    return cv.Schema(config).extend(AW88298_SCHEMA)


async def add_setting_entities(
    config: ConfigType,
    definitions: dict[str, EntityDef],
    create: Callable[[ConfigType, EntityDef], Any],
) -> None:
    """Create the configured entities of a platform that writes one setting each.

    :param create: coroutine taking the entity configuration and its definition, and returning
        the new entity, e.g. `number.new_number`.
    """
    parent = await cg.get_variable(config[CONF_AW88298_ID])
    for key, definition in definitions.items():
        if (conf := config.get(key)) is None:
            continue
        var = await create(conf, definition)
        await cg.register_component(var, conf)
        await cg.register_parented(var, parent)
        cg.add(var.set_setting(definition.source))
