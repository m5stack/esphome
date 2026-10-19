import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
)
from . import UnitScrollComponent, CONF_UNIT_SCROLL_ID, unit_scroll_ns

DEPENDENCIES = ["unit_scroll"]

UnitScrollSensor = unit_scroll_ns.class_("UnitScrollSensor", sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UnitScrollSensor,
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_UNIT_SCROLL_ID): cv.use_id(UnitScrollComponent),
        }
    )
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_UNIT_SCROLL_ID])
    cg.add(var.set_parent(parent))