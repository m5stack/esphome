import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_ILLUMINANCE, STATE_CLASS_MEASUREMENT, UNIT_LUX
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_DLIGHT_ID, M5ChainDLight, m5_chain_dlight_ns

DEPENDENCIES = ["m5_chain_dlight"]

M5ChainDLightSensor = m5_chain_dlight_ns.class_(
    "M5ChainDLightSensor", sensor.Sensor, cg.Parented.template(M5ChainDLight)
)

CONFIG_SCHEMA = sensor.sensor_schema(
    M5ChainDLightSensor,
    unit_of_measurement=UNIT_LUX,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_ILLUMINANCE,
    state_class=STATE_CLASS_MEASUREMENT,
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_DLIGHT_ID): cv.use_id(M5ChainDLight),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_DLIGHT_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_illuminance_sensor(var))
