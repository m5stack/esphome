"""Tests for the AXP2101 number platform.

The bounds live in Python because the number entity needs them at config time,
while the scale lives in the model table. These tests keep the two in step.
"""

from pathlib import Path
import re
from unittest.mock import patch

import pytest

from esphome import config, yaml_util
from esphome.components.axp2101 import number as axp_number
import esphome.config_validation as cv
from esphome.core import CORE

COMPONENT_DIR = Path(__file__).parents[3] / "esphome" / "components" / "axp2101"


def _model_numbers() -> dict[str, dict[str, int]]:
    """One dict per table entry, with the scale the model table declares."""
    source = (COMPONENT_DIR / "axp2101.cpp").read_text()
    block = source.split("constexpr AxpNumber NUMBERS[] = {", 1)[1].split("\n};", 1)[0]

    entries = {}
    for chunk in block.split("{.name = ")[1:]:
        name = re.match(r'"([^"]+)"', chunk).group(1)
        entries[name] = {
            key: int(re.search(rf"\.{key} = (\d+)", chunk).group(1))
            for key in ("offset", "step", "min_code", "max_code")
        }
    return entries


def test_settings_match_the_cpp_number_table():
    assert list(axp_number.SETTINGS) == list(_model_numbers())


@pytest.mark.parametrize("setting", list(axp_number.SETTINGS))
def test_bounds_follow_the_model_table(setting):
    """min/max/step must be the scale the chip actually has."""
    scale = _model_numbers()[setting]
    expected_min = scale["offset"] + scale["step"] * scale["min_code"]
    expected_max = scale["offset"] + scale["step"] * scale["max_code"]

    declared = axp_number.SETTINGS[setting]
    assert declared.min_value == expected_min
    assert declared.max_value == expected_max
    assert declared.step == scale["step"]


def test_every_setting_can_be_configured():
    config = axp_number.CONFIG_SCHEMA(
        {"axp_id": "pmu", **{name: {"name": name} for name in axp_number.SETTINGS}}
    )

    assert set(config) >= set(axp_number.SETTINGS)


def test_an_unknown_setting_is_rejected():
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        axp_number.CONFIG_SCHEMA(
            {"axp_id": "pmu", "battery_capacity": {"name": "Nope"}}
        )


def test_a_setting_is_optional():
    assert "precharge_current" not in axp_number.CONFIG_SCHEMA({"axp_id": "pmu"})


def _validate(tmp_path):
    yaml_text = """
esphome:
  name: test
esp32:
  board: esp32-s3-devkitc-1
i2c:
  sda: GPIO12
  scl: GPIO11
axp2101:
  id: pmu
number:
  - platform: axp2101
    input_voltage_limit:
      id: vindpm
      name: Input Voltage Limit
script:
  - id: lower_limit
    then:
      - number.set:
          id: vindpm
          value: 4200
"""
    config_file = tmp_path / "test.yaml"
    config_file.write_text(yaml_text)
    parsed = yaml_util.load_yaml(config_file)
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed),
        patch.object(CORE, "config_path", config_file),
    ):
        return config.read_config({})


def test_a_number_can_be_driven_by_the_generic_action(tmp_path):
    assert _validate(tmp_path) is not None
