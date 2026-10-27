import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import M5ChainSwitch, m5_chain_switch_ns

DEPENDENCIES = ["m5_chain_switch"]

CONF_M5_CHAIN_SWITCH_ID = "m5_chain_switch_id"

M5ChainSwitchSensor = m5_chain_switch_ns.class_(
    "M5ChainSwitchSensor", sensor.Sensor, cg.Parented.template(M5ChainSwitch)
)
SwitchSensorType = m5_chain_switch_ns.enum("SwitchSensorType", is_class=True)
SENSOR_TYPES = {
    "adc_12": SwitchSensorType.SWITCH_SENSOR_ADC_12,
    "adc_8": SwitchSensorType.SWITCH_SENSOR_ADC_8,
    "change_mode": SwitchSensorType.SWITCH_SENSOR_CHANGE_MODE,
    "open_threshold": SwitchSensorType.SWITCH_SENSOR_OPEN_THRESHOLD,
    "close_threshold": SwitchSensorType.SWITCH_SENSOR_CLOSE_THRESHOLD,
}

CONFIG_SCHEMA = sensor.sensor_schema(M5ChainSwitchSensor).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_SWITCH_ID): cv.use_id(M5ChainSwitch),
        cv.Optional(CONF_TYPE, default="adc_12"): cv.enum(SENSOR_TYPES),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_SWITCH_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_switch_sensor(SENSOR_TYPES[config[CONF_TYPE]], var))
