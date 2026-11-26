import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR,
    CONF_FREQUENCY,
    CONF_GREEN,
    CONF_INTERVAL,
    CONF_RED,
    CONF_TEMPERATURE,
)
from esphome.types import ConfigType

CODEOWNERS = ["@m5stack"]
DEPENDENCIES = ["i2c"]
DOMAIN = "m5_unit_ncir2"
MULTI_CONF = True

CONF_M5_UNIT_NCIR2_ID = "m5_unit_ncir2_id"
CONF_EMISSIVITY = "emissivity"
CONF_LOW_ALARM = "low_alarm"
CONF_HIGH_ALARM = "high_alarm"
CONF_BUZZER = "buzzer"
CONF_DUTY = "duty"

m5_unit_ncir2_ns = cg.esphome_ns.namespace("m5_unit_ncir2")
M5UnitNCIR2 = m5_unit_ncir2_ns.class_("M5UnitNCIR2", i2c.I2CDevice, cg.PollingComponent)

# The order matches the alarm blocks in the unit's register map
ALARMS = (CONF_LOW_ALARM, CONF_HIGH_ALARM)

COLOR_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_RED): cv.percentage,
        cv.Required(CONF_GREEN): cv.percentage,
        cv.Required(CONF_BLUE): cv.percentage,
    }
)

BUZZER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FREQUENCY, default="4000Hz"): cv.All(
            cv.frequency, cv.Range(min=20, max=20000)
        ),
        cv.Optional(CONF_INTERVAL, default="100ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(
                min=cv.TimePeriod(milliseconds=1), max=cv.TimePeriod(milliseconds=5000)
            ),
        ),
        cv.Optional(CONF_DUTY, default="50%"): cv.percentage,
    }
)

# The unit drives its LED and buzzer on its own while the temperature is past a threshold
ALARM_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TEMPERATURE): cv.All(
            cv.temperature, cv.Range(min=-70.0, max=380.0)
        ),
        cv.Optional(CONF_COLOR): COLOR_SCHEMA,
        cv.Optional(CONF_BUZZER): BUZZER_SCHEMA,
    }
)


def _validate_alarms(config: ConfigType) -> ConfigType:
    low = config.get(CONF_LOW_ALARM)
    high = config.get(CONF_HIGH_ALARM)
    if low and high and low[CONF_TEMPERATURE] >= high[CONF_TEMPERATURE]:
        raise cv.Invalid(
            f"The '{CONF_LOW_ALARM}' temperature must be below the '{CONF_HIGH_ALARM}' temperature"
        )
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5UnitNCIR2),
            cv.Optional(CONF_EMISSIVITY): cv.float_range(min=0.1, max=1.0),
            cv.Optional(CONF_LOW_ALARM): ALARM_SCHEMA,
            cv.Optional(CONF_HIGH_ALARM): ALARM_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x5A)),
    _validate_alarms,
)


def _byte(fraction: float) -> int:
    return round(fraction * 255)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    if (emissivity := config.get(CONF_EMISSIVITY)) is not None:
        # The unit stores the emissivity as a fraction of 65535
        cg.add(var.set_emissivity(round(emissivity * 65535)))
    for index, key in enumerate(ALARMS):
        if (alarm := config.get(key)) is None:
            continue
        # Temperatures are in hundredths of a degree Celsius
        cg.add(var.set_alarm_temperature(index, round(alarm[CONF_TEMPERATURE] * 100)))
        if (color := alarm.get(CONF_COLOR)) is not None:
            cg.add(
                var.set_alarm_color(
                    index,
                    _byte(color[CONF_RED]),
                    _byte(color[CONF_GREEN]),
                    _byte(color[CONF_BLUE]),
                )
            )
        if (buzzer := alarm.get(CONF_BUZZER)) is not None:
            cg.add(
                var.set_alarm_buzzer(
                    index,
                    round(buzzer[CONF_FREQUENCY]),
                    buzzer[CONF_INTERVAL].total_milliseconds,
                    _byte(buzzer[CONF_DUTY]),
                )
            )
