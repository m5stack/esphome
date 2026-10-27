import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_DISTANCE, UNIT_MILLIMETER
from esphome.types import ConfigType

from .. import M5ChainToF, m5_chain_tof_ns

DEPENDENCIES = ["m5_chain_tof"]

CONF_M5_CHAIN_TOF_ID = "m5_chain_tof_id"

M5ChainToFSensor = m5_chain_tof_ns.class_(
    "M5ChainToFSensor", sensor.Sensor, cg.Parented.template(M5ChainToF)
)

CONFIG_SCHEMA = sensor.sensor_schema(
    M5ChainToFSensor,
    icon="mdi:ray-vertex",
    unit_of_measurement=UNIT_MILLIMETER,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_DISTANCE,
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_TOF_ID): cv.use_id(M5ChainToF),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_TOF_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_distance_sensor(var))
