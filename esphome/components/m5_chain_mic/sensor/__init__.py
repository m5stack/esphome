import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE
from esphome.types import ConfigType

from .. import CONF_M5_CHAIN_MIC_ID, M5ChainMic, m5_chain_mic_ns

DEPENDENCIES = ["m5_chain_mic"]

M5ChainMicSensor = m5_chain_mic_ns.class_(
    "M5ChainMicSensor", sensor.Sensor, cg.Parented.template(M5ChainMic)
)
MicSensorType = m5_chain_mic_ns.enum("MicSensorType", is_class=True)
SENSOR_TYPES = {
    "adc_12": MicSensorType.MIC_SENSOR_ADC_12,
    "adc_8": MicSensorType.MIC_SENSOR_ADC_8,
}

CONFIG_SCHEMA = sensor.sensor_schema(
    M5ChainMicSensor,
    icon="mdi:microphone",
    accuracy_decimals=0,
).extend(
    {
        cv.GenerateID(CONF_M5_CHAIN_MIC_ID): cv.use_id(M5ChainMic),
        cv.Optional(CONF_TYPE, default="adc_12"): cv.enum(SENSOR_TYPES),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    hub = await cg.get_variable(config[CONF_M5_CHAIN_MIC_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.set_adc_sensor(SENSOR_TYPES[config[CONF_TYPE]], var))
