"""Tests for the I2S frame the AW88298 is configured with."""

import pytest

from esphome.components.aw88298 import audio_dac as aw
from esphome.components.speaker import DOMAIN as SPEAKER_DOMAIN
from esphome.const import CONF_AUDIO_DAC, CONF_ID, CONF_SAMPLE_RATE
from esphome.core import CORE

DAC_ID = "amp"


@pytest.fixture(autouse=True)
def aw88298_config(monkeypatch):
    """Start every test with no speaker and a bare amplifier configuration."""
    monkeypatch.setattr(CORE, "config", {})
    return {CONF_ID: DAC_ID, aw.CONF_SLOT_COUNT: 2}


@pytest.fixture
def speaker(monkeypatch):
    def add(**kwargs):
        monkeypatch.setattr(
            CORE,
            "config",
            {SPEAKER_DOMAIN: [{CONF_AUDIO_DAC: DAC_ID, **kwargs}]},
        )

    return add


def test_frame_follows_the_speaker(aw88298_config, speaker):
    speaker(
        **{
            CONF_SAMPLE_RATE: 48000,
            aw.CONF_BITS_PER_SAMPLE: 24,
            aw.CONF_I2S_COMM_FMT: "stand_msb",
        }
    )
    assert aw._resolve_i2s(aw88298_config) == (48000, 24, 24, "left_justified", "slot")


def test_right_justified_comes_from_a_pcm_short_frame(aw88298_config, speaker):
    speaker(
        **{
            CONF_SAMPLE_RATE: 16000,
            aw.CONF_BITS_PER_SAMPLE: 16,
            aw.CONF_I2S_COMM_FMT: "pcm_short",
        }
    )
    assert aw._resolve_i2s(aw88298_config) == (16000, 16, 16, "left_justified", "bck")


def test_explicit_values_win_over_the_speaker(aw88298_config, speaker):
    speaker(**{CONF_SAMPLE_RATE: 16000, aw.CONF_BITS_PER_SAMPLE: 16})
    aw88298_config[CONF_SAMPLE_RATE] = 22050
    assert aw._resolve_i2s(aw88298_config) == (22050, 16, 16, "i2s", "slot")


def test_20_bit_samples_use_24_bit_slots(aw88298_config, speaker):
    speaker(**{CONF_SAMPLE_RATE: 32000, aw.CONF_BITS_PER_SAMPLE: 32})
    assert aw._resolve_i2s(aw88298_config)[1:3] == (32, 32)
    aw88298_config[aw.CONF_BITS_PER_SAMPLE] = 20
    assert aw._resolve_i2s(aw88298_config)[1:3] == (20, 24)


def test_no_speaker_falls_back_with_a_warning(aw88298_config, caplog):
    assert aw._resolve_i2s(aw88298_config) == (16000, 16, 16, "i2s", "slot")
    assert "sample rate falls back" in caplog.text


def test_a_rate_the_device_cannot_lock_to_is_rejected(aw88298_config, speaker):
    speaker(**{CONF_SAMPLE_RATE: 88200})
    with pytest.raises(aw.cv.Invalid, match="cannot lock to 88200 Hz"):
        aw._resolve_i2s(aw88298_config)


def test_four_slots_need_sixteen_bit_slots(aw88298_config, speaker):
    speaker(**{CONF_SAMPLE_RATE: 96000, aw.CONF_BITS_PER_SAMPLE: 16})
    aw88298_config[aw.CONF_SLOT_COUNT] = 4
    assert aw._resolve_i2s(aw88298_config)[2] == 16
    aw88298_config[aw.CONF_BITS_PER_SAMPLE] = 24
    with pytest.raises(aw.cv.Invalid, match="Four slot TDM needs 16 bit slots"):
        aw._resolve_i2s(aw88298_config)


def test_samples_must_fit_their_slot(aw88298_config):
    aw88298_config[aw.CONF_BITS_PER_SAMPLE] = 24
    aw88298_config[aw.CONF_SLOT_BITS] = 16
    with pytest.raises(aw.cv.Invalid, match="need at least 24 bit slots"):
        aw._resolve_i2s(aw88298_config)
