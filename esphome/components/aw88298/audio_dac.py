import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUDIO_DAC,
    CONF_BITS_PER_SAMPLE,
    CONF_ID,
    CONF_MODE,
    CONF_RESET_PIN,
    CONF_SAMPLE_RATE,
    CONF_VOLTAGE,
)
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import AW88298, Setting, aw88298_ns

try:  # ESPHome 2026.11 moved this into the i2s_audio package
    from esphome.components.i2s_audio import CONF_I2S_COMM_FMT
except ImportError:  # 2026.10 keeps it in the speaker platform
    from esphome.components.i2s_audio.speaker import CONF_I2S_COMM_FMT

try:  # ESPHome 2026.11 exports the domain
    from esphome.components.speaker import DOMAIN as SPEAKER_DOMAIN
except ImportError:  # 2026.10 does not
    SPEAKER_DOMAIN = "speaker"

_LOGGER = logging.getLogger(__name__)

CONF_AGC = "agc"
CONF_ATTACK_THRESHOLD = "attack_threshold"
CONF_ATTACK_TIME = "attack_time"
CONF_BOOST = "boost"
CONF_CURRENT_LIMIT = "current_limit"
CONF_DEGLITCH = "deglitch"
CONF_HDCC = "hdcc"
CONF_HOLD_TIME = "hold_time"
CONF_I2S_CHANNEL = "i2s_channel"
CONF_I2S_FORMAT = "i2s_format"
CONF_INITIAL_VOLUME = "initial_volume"
CONF_INPUT_ATTENUATION = "input_attenuation"
CONF_RELEASE_THRESHOLD = "release_threshold"
CONF_RELEASE_TIME = "release_time"
CONF_RMS = "rms"
CONF_RX_SLOTS = "rx_slots"
CONF_SLOT_BITS = "slot_bits"
CONF_SLOT_COUNT = "slot_count"
CONF_VOLUME_MAX_DB = "volume_max_db"
CONF_VOLUME_MIN_DB = "volume_min_db"
CONF_WS_PULSE = "ws_pulse"

#: Sample rates the device can lock to, I2SCTRL.I2SSR in the order of the register.
SAMPLE_RATES = [8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 96000]

I2SFormat = aw88298_ns.enum("I2SFormat", is_class=True)
ChannelSelect = aw88298_ns.enum("ChannelSelect", is_class=True)

#: I2SCTRL.I2SMD
I2S_FORMATS = {
    "i2s": I2SFormat.I2S_FORMAT_STANDARD,
    "left_justified": I2SFormat.I2S_FORMAT_LEFT_JUSTIFIED,
    "right_justified": I2SFormat.I2S_FORMAT_RIGHT_JUSTIFIED,
}

#: I2SCTRL.CHSEL
I2S_CHANNELS = {
    "left": ChannelSelect.CHANNEL_SELECT_LEFT,
    "right": ChannelSelect.CHANNEL_SELECT_RIGHT,
    "mono": ChannelSelect.CHANNEL_SELECT_MONO,
}

#: I2SCFG1.FSYNC_TYPE
WS_PULSES = {"slot": False, "bck": True}

#: BSTCTRL2.BST_MODE, as the value the boost mode setting uses.
BOOST_MODES = {
    "pass_through": 0.0,
    "force_boost": 1.0,
    "smart_boost_1": 2.0,
    "smart_boost_2": 3.0,
}

#: BSTCTRL2.BST_TDEG, the deglitch time of smart boost 1.
BOOST_DEGLITCH = {
    "0.33ms": 0.0,
    "1.4ms": 1.0,
    "5.6ms": 2.0,
    "21.3ms": 3.0,
    "44ms": 4.0,
    "88ms": 5.0,
    "352ms": 6.0,
    "1.4s": 7.0,
}

#: What the frame format of the speaker driving this amplifier says. ESPHome folds several
#: i2s_comm_fmt names into one driver format, so this table follows the driver, not the names.
COMM_FMT_I2S = {
    "stand_i2s": ("i2s", "slot"),
    "i2s_msb": ("i2s", "slot"),
    "stand_pcm_long": ("i2s", "slot"),
    "pcm_long": ("i2s", "slot"),
    "stand_max": ("i2s", "slot"),
    "stand_msb": ("left_justified", "slot"),
    "i2s_lsb": ("left_justified", "slot"),
    "stand_pcm_short": ("left_justified", "bck"),
    "pcm_short": ("left_justified", "bck"),
    "pcm": ("left_justified", "bck"),
}


def _validate_bits_per_sample(value: str | int) -> int:
    bits = cv.float_with_unit("bits", "bit")(value)
    if bits not in (16, 20, 24, 32):
        raise cv.Invalid("The AW88298 takes 16, 20, 24 or 32 bits per sample")
    return int(bits)


def _validate_config(config: ConfigType) -> ConfigType:
    if config[CONF_VOLUME_MIN_DB] >= config[CONF_VOLUME_MAX_DB]:
        raise cv.Invalid(f"{CONF_VOLUME_MIN_DB} must be less than {CONF_VOLUME_MAX_DB}")

    if (slots := config.get(CONF_RX_SLOTS)) is not None:
        if len(set(slots)) != 2:
            raise cv.Invalid(f"{CONF_RX_SLOTS} must name two different slots")
        if config[CONF_SLOT_COUNT] == 2:
            raise cv.Invalid(
                f"{CONF_RX_SLOTS} only applies to TDM; a two slot frame receives slots 0 and 1"
            )

    # Only 16 bit slots keep a four slot frame within the 32/48/64 fs the chip accepts. The
    # data width is checked where it is resolved, since it can also come from the speaker.
    slot_bits = config.get(CONF_SLOT_BITS)
    if config[CONF_SLOT_COUNT] == 4 and slot_bits not in (None, 16):
        raise cv.Invalid(
            "Four slot TDM needs 16 bit slots to keep BCK within 32/48/64 fs"
        )
    return config


AGC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ATTACK_THRESHOLD, default="64%"): cv.percentage,
        cv.Optional(CONF_RELEASE_THRESHOLD, default="57%"): cv.percentage,
        cv.Optional(
            CONF_ATTACK_TIME, default="1ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_RELEASE_TIME, default="10ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_HOLD_TIME, default="133ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RMS, default=False): cv.boolean,
    }
)

#: The values an absent agc block leaves in the registers.
AGC_DEFAULTS = AGC_SCHEMA({})

BOOST_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MODE, default="smart_boost_2"): cv.enum(
            BOOST_MODES, lower=True
        ),
        cv.Optional(CONF_VOLTAGE, default="9.5V"): cv.All(
            cv.voltage, cv.float_range(min=5.0, max=10.25)
        ),
        cv.Optional(CONF_CURRENT_LIMIT, default="3.5A"): cv.All(
            cv.current, cv.float_range(min=1.5, max=4.25)
        ),
        cv.Optional(CONF_DEGLITCH, default="21.3ms"): cv.enum(
            BOOST_DEGLITCH, lower=True
        ),
        cv.Optional(CONF_ATTACK_THRESHOLD, default="30%"): cv.percentage,
        cv.Optional(CONF_RELEASE_THRESHOLD, default="15%"): cv.percentage,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AW88298),
            # The I2S settings come from the speaker that drives this amplifier unless they are
            # given here, because the amplifier has to be told the frame it will receive.
            cv.Optional(CONF_SAMPLE_RATE): cv.one_of(*SAMPLE_RATES, int=True),
            cv.Optional(CONF_BITS_PER_SAMPLE): _validate_bits_per_sample,
            cv.Optional(CONF_I2S_FORMAT): cv.enum(I2S_FORMATS, lower=True),
            cv.Optional(CONF_I2S_CHANNEL, default="left"): cv.enum(
                I2S_CHANNELS, lower=True
            ),
            cv.Optional(CONF_SLOT_BITS): cv.one_of(16, 24, 32, int=True),
            cv.Optional(CONF_SLOT_COUNT, default=2): cv.one_of(2, 4, int=True),
            cv.Optional(CONF_RX_SLOTS): cv.All(
                [cv.int_range(min=0, max=3)], cv.Length(min=2, max=2)
            ),
            cv.Optional(CONF_WS_PULSE): cv.enum(WS_PULSES, lower=True),
            # RSTN is active low, so it is driven low and then high. Any output pin works,
            # including one on an IO expander such as the AW9523B.
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_INPUT_ATTENUATION, default=False): cv.boolean,
            cv.Optional(CONF_HDCC, default=True): cv.boolean,
            cv.Optional(CONF_AGC): AGC_SCHEMA,
            cv.Optional(CONF_BOOST, default={}): BOOST_SCHEMA,
            cv.Optional(CONF_VOLUME_MIN_DB, default="-96dB"): cv.All(
                cv.decibel, cv.float_range(min=-96.0, max=0.0)
            ),
            cv.Optional(CONF_VOLUME_MAX_DB, default="0dB"): cv.All(
                cv.decibel, cv.float_range(min=-96.0, max=0.0)
            ),
            cv.Optional(CONF_INITIAL_VOLUME, default="100%"): cv.percentage,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x36)),
    _validate_config,
)


def _speaker_config(config: ConfigType) -> ConfigType:
    """The speaker that plays through this amplifier, empty when no speaker uses it."""
    for speaker_config in (CORE.config or {}).get(SPEAKER_DOMAIN, []):
        if speaker_config.get(CONF_AUDIO_DAC) == config[CONF_ID]:
            return speaker_config
    return {}


def _resolve_i2s(config: ConfigType) -> tuple[int, int, int, str, str]:
    """Work out the frame the amplifier receives.

    Values given in the configuration win, the rest come from the speaker, which is where the
    I2S peripheral gets them, so the amplifier is configured for the frame it will see.
    Returns the sample rate, the data bits, the slot bits and the format pair.
    """
    speaker = _speaker_config(config)

    sample_rate = config.get(CONF_SAMPLE_RATE) or speaker.get(CONF_SAMPLE_RATE)
    if sample_rate is None:
        sample_rate = 16000
        _LOGGER.warning(
            "%s: no speaker plays through this amplifier, so the sample rate falls back to "
            "%d Hz. Set %s to match the audio you play",
            config[CONF_ID],
            sample_rate,
            CONF_SAMPLE_RATE,
        )
    elif sample_rate not in SAMPLE_RATES:
        raise cv.Invalid(
            f"The AW88298 cannot lock to {sample_rate} Hz, it takes one of {SAMPLE_RATES}"
        )

    data_bits = config.get(CONF_BITS_PER_SAMPLE) or speaker.get(CONF_BITS_PER_SAMPLE)
    if data_bits is None:
        data_bits = 16
    elif data_bits not in (16, 20, 24, 32):
        raise cv.Invalid(
            f"The speaker sends {data_bits} bits per sample, the AW88298 takes 16, 20, 24 or 32"
        )

    # A 20 bit sample travels in a 24 bit slot, every other width fills its slot exactly.
    slot_bits = config.get(CONF_SLOT_BITS) or (24 if data_bits == 20 else data_bits)
    if data_bits > slot_bits:
        raise cv.Invalid(f"{data_bits} bit samples need at least {data_bits} bit slots")
    if config[CONF_SLOT_COUNT] == 4 and slot_bits != 16:
        raise cv.Invalid(
            "Four slot TDM needs 16 bit slots to keep BCK within 32/48/64 fs"
        )

    format_, ws_pulse = COMM_FMT_I2S.get(
        speaker.get(CONF_I2S_COMM_FMT, "stand_i2s"), ("i2s", "slot")
    )
    return (
        sample_rate,
        data_bits,
        slot_bits,
        config.get(CONF_I2S_FORMAT) or format_,
        config.get(CONF_WS_PULSE) or ws_pulse,
    )


def _initial_settings(config: ConfigType) -> list[tuple[MockObj, float]]:
    """The settings the amplifier starts with, one entry per register field."""
    boost = config[CONF_BOOST]
    settings = [
        (Setting.SETTING_HDCC, float(config[CONF_HDCC])),
        (Setting.SETTING_INPUT_ATTENUATION, float(config[CONF_INPUT_ATTENUATION])),
        (Setting.SETTING_BOOST_MODE, boost[CONF_MODE]),
        (Setting.SETTING_BOOST_VOLTAGE, boost[CONF_VOLTAGE]),
        (Setting.SETTING_BOOST_CURRENT_LIMIT, boost[CONF_CURRENT_LIMIT]),
        (Setting.SETTING_BOOST_DEGLITCH, boost[CONF_DEGLITCH]),
        (
            Setting.SETTING_BOOST_ATTACK_THRESHOLD,
            round(boost[CONF_ATTACK_THRESHOLD] * 100),
        ),
        (
            Setting.SETTING_BOOST_RELEASE_THRESHOLD,
            round(boost[CONF_RELEASE_THRESHOLD] * 100),
        ),
    ]

    # The AGC registers are always written, so an absent block leaves the datasheet defaults in
    # place instead of zeroing them.
    agc = config.get(CONF_AGC, AGC_DEFAULTS)
    settings += [
        (Setting.SETTING_AGC, 1.0 if CONF_AGC in config else 0.0),
        (Setting.SETTING_AGC_RMS, float(agc[CONF_RMS])),
        (Setting.SETTING_AGC_ATTACK_THRESHOLD, round(agc[CONF_ATTACK_THRESHOLD] * 100)),
        (
            Setting.SETTING_AGC_RELEASE_THRESHOLD,
            round(agc[CONF_RELEASE_THRESHOLD] * 100),
        ),
        (Setting.SETTING_AGC_ATTACK_TIME, agc[CONF_ATTACK_TIME].total_milliseconds),
        (Setting.SETTING_AGC_RELEASE_TIME, agc[CONF_RELEASE_TIME].total_milliseconds),
        (Setting.SETTING_AGC_HOLD_TIME, agc[CONF_HOLD_TIME].total_milliseconds),
    ]
    return settings


async def to_code(config: ConfigType) -> None:
    # `i2c.new_i2c_component` only exists from ESPHome 2026.11; these three calls
    # are the same thing and also work on 2026.10.
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    sample_rate, data_bits, slot_bits, i2s_format, ws_pulse = _resolve_i2s(config)
    cg.add(var.set_sample_rate(sample_rate))
    cg.add(var.set_data_bits(data_bits))
    cg.add(var.set_slot_bits(slot_bits))
    cg.add(var.set_slot_count(config[CONF_SLOT_COUNT]))
    cg.add(
        var.set_rx_slot_mask(
            sum(1 << slot for slot in config.get(CONF_RX_SLOTS, [0, 1]))
        )
    )
    cg.add(var.set_i2s_format(I2S_FORMATS[i2s_format]))
    cg.add(var.set_i2s_channel(config[CONF_I2S_CHANNEL]))
    cg.add(var.set_ws_pulse_one_bck(WS_PULSES[ws_pulse]))
    cg.add(var.set_volume_min_db(config[CONF_VOLUME_MIN_DB]))
    cg.add(var.set_volume_max_db(config[CONF_VOLUME_MAX_DB]))
    cg.add(var.set_initial_volume(config[CONF_INITIAL_VOLUME]))
    # `cg.add_pin` only exists from ESPHome 2026.11.
    if (reset_pin := config.get(CONF_RESET_PIN)) is not None:
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))

    for setting, value in _initial_settings(config):
        cg.add(var.set_setting(setting, value))
