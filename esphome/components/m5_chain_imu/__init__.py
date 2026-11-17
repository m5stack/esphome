import esphome.codegen as cg
from esphome.components import m5_chain_base, uart
from esphome.components.const import (
    CONF_ACCELEROMETER_ODR,
    CONF_ACCELEROMETER_RANGE,
    CONF_GYROSCOPE_ODR,
    CONF_GYROSCOPE_RANGE,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UPDATE_INTERVAL
from esphome.types import ConfigType

AUTO_LOAD = ["binary_sensor", "m5_chain_base", "sensor"]
CODEOWNERS = ["@Jasionf"]
DEPENDENCIES = ["uart"]
DOMAIN = "m5_chain_imu"
MULTI_CONF = True

CONF_M5_CHAIN_IMU_ID = "m5_chain_imu_id"

m5_chain_imu_ns = cg.esphome_ns.namespace("m5_chain_imu")
M5ChainImu = m5_chain_imu_ns.class_("M5ChainImu", m5_chain_base.M5ChainBase)

# Wire codes of the Chain IMU firmware; they are not in range order.
ACCEL_RANGES = {"2G": 0x00, "4G": 0x02, "8G": 0x03, "16G": 0x01}
GYRO_RANGES = {
    "125DPS": 0x01,
    "250DPS": 0x00,
    "500DPS": 0x02,
    "1000DPS": 0x04,
    "2000DPS": 0x06,
}
GYRO_ODRS = {
    "OFF": 0x00,
    "12_5HZ": 0x01,
    "26HZ": 0x02,
    "52HZ": 0x03,
    "104HZ": 0x04,
    "208HZ": 0x05,
    "416HZ": 0x06,
    "833HZ": 0x07,
    "1666HZ": 0x08,
    "3332HZ": 0x09,
    "6664HZ": 0x0A,
}
ACCEL_ODRS = {**GYRO_ODRS, "1_6HZ": 0x0B}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5ChainImu),
            cv.Optional(CONF_ACCELEROMETER_RANGE): cv.enum(ACCEL_RANGES, upper=True),
            cv.Optional(CONF_GYROSCOPE_RANGE): cv.enum(GYRO_RANGES, upper=True),
            cv.Optional(CONF_ACCELEROMETER_ODR): cv.enum(ACCEL_ODRS, upper=True),
            cv.Optional(CONF_GYROSCOPE_ODR): cv.enum(GYRO_ODRS, upper=True),
        }
    )
    .extend(m5_chain_base.BASE_SCHEMA)
    .extend({cv.Optional(CONF_UPDATE_INTERVAL, default="1s"): cv.update_interval})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=115200,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_chain_index(config[m5_chain_base.CONF_CHAIN_INDEX]))
    cg.add(var.set_update_interval(config[CONF_UPDATE_INTERVAL]))
    cg.add_optional(var.set_accel_range, config.get(CONF_ACCELEROMETER_RANGE))
    cg.add_optional(var.set_gyro_range, config.get(CONF_GYROSCOPE_RANGE))
    cg.add_optional(var.set_accel_odr, config.get(CONF_ACCELEROMETER_ODR))
    cg.add_optional(var.set_gyro_odr, config.get(CONF_GYROSCOPE_ODR))
