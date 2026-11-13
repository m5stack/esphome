import esphome.codegen as cg
from esphome.components import sensor
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_VOLT,
)
from esphome.types import ConfigType

from .. import (
    CONF_AW88298_ID,
    EntityDef,
    SensorType,
    aw88298_ns,
    request_sensor_slot,
    schema,
)

CONF_AMPLIFIER_VOLTAGE = "amplifier_voltage"
CONF_BOOST_VOLTAGE = "boost_voltage"
CONF_DIE_TEMPERATURE = "die_temperature"
CONF_SUPPLY_VOLTAGE = "supply_voltage"

AW88298Sensor = aw88298_ns.class_("AW88298Sensor", sensor.Sensor)

#: The measurements of the register map that are worth reporting.
SENSORS = {
    CONF_DIE_TEMPERATURE: EntityDef(
        AW88298Sensor,
        SensorType.SENSOR_TYPE_DIE_TEMPERATURE,
        {
            "unit_of_measurement": UNIT_CELSIUS,
            "device_class": DEVICE_CLASS_TEMPERATURE,
            "state_class": STATE_CLASS_MEASUREMENT,
            "accuracy_decimals": 0,
            "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
        },
    ),
    CONF_SUPPLY_VOLTAGE: EntityDef(
        AW88298Sensor,
        SensorType.SENSOR_TYPE_SUPPLY_VOLTAGE,
        {
            "unit_of_measurement": UNIT_VOLT,
            "device_class": DEVICE_CLASS_VOLTAGE,
            "state_class": STATE_CLASS_MEASUREMENT,
            "accuracy_decimals": 3,
            "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
        },
    ),
    CONF_AMPLIFIER_VOLTAGE: EntityDef(
        AW88298Sensor,
        SensorType.SENSOR_TYPE_AMPLIFIER_VOLTAGE,
        {
            "unit_of_measurement": UNIT_VOLT,
            "device_class": DEVICE_CLASS_VOLTAGE,
            "state_class": STATE_CLASS_MEASUREMENT,
            "accuracy_decimals": 2,
            "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
        },
    ),
    CONF_BOOST_VOLTAGE: EntityDef(
        AW88298Sensor,
        SensorType.SENSOR_TYPE_BOOST_VOLTAGE,
        {
            "unit_of_measurement": UNIT_VOLT,
            "device_class": DEVICE_CLASS_VOLTAGE,
            "state_class": STATE_CLASS_MEASUREMENT,
            "accuracy_decimals": 3,
            "entity_category": ENTITY_CATEGORY_DIAGNOSTIC,
        },
    ),
}

CONFIG_SCHEMA = schema(
    SENSORS,
    lambda definition: sensor.sensor_schema(definition.class_, **definition.options),
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_AW88298_ID])
    for key, definition in SENSORS.items():
        if (conf := config.get(key)) is None:
            continue
        var = await sensor.new_sensor(conf)
        request_sensor_slot()
        cg.add(parent.add_sensor(definition.source, var))
