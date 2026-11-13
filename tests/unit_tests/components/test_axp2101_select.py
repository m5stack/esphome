"""Tests for the AXP2101 select platform.

The option labels live in Python because the select entity needs them at config
time, while the values live in the model table. These tests keep the two in step.
"""

from pathlib import Path
import re
from unittest.mock import patch

import pytest

from esphome import config, yaml_util
from esphome.components.axp2101 import select as axp_select
import esphome.config_validation as cv
from esphome.core import CORE

COMPONENT_DIR = Path(__file__).parents[3] / "esphome" / "components" / "axp2101"

#: Label suffix -> the factor the model table stores values in.
UNITS = {"mA": 1, "V": 1000, "s": 1000}


def _selects_block() -> str:
    source = (COMPONENT_DIR / "axp2101.cpp").read_text()
    return source.split("constexpr AxpSelect SELECTS[] = {", 1)[1].split("\n};", 1)[0]


def _tables() -> dict[str, list[int]]:
    """Values of every value table, in the order the select presents them."""
    source = (COMPONENT_DIR / "axp2101.cpp").read_text()
    tables: dict[str, list[int]] = {}
    for name in re.findall(
        r"constexpr AxpLutEntry (\w+)\[\] = \{(.*?)\n\};", source, re.DOTALL
    ):
        tables[name[0]] = [int(value) for value in re.findall(r"\{(\d+),", name[1])]
    return tables


def _table_for(setting: str) -> list[int]:
    block = _selects_block()
    entry = block.split(f'"{setting}"', 1)[1]
    table = re.search(r"\.entries = (\w+)", entry).group(1)
    return _tables()[table]


def _parse_label(label: str) -> int:
    match = re.match(r"^([\d.]+)(mA|V|s)$", label)
    assert match, f"label {label!r} needs a unit"
    return round(float(match.group(1)) * UNITS[match.group(2)])


def test_settings_match_the_cpp_select_table():
    names = re.findall(r'\.name = "([^"]+)"', _selects_block())

    assert names == list(axp_select.SETTINGS)


@pytest.mark.parametrize("setting", list(axp_select.SETTINGS))
def test_options_match_the_value_table(setting):
    """Option i must be value i, otherwise a select applies the neighbour's value."""
    expected = [_parse_label(label) for label in axp_select.SETTINGS[setting]]

    assert expected == _table_for(setting)


def test_every_setting_can_be_configured():
    config = axp_select.CONFIG_SCHEMA(
        {"axp_id": "pmu", **{name: {"name": name} for name in axp_select.SETTINGS}}
    )

    assert set(config) >= set(axp_select.SETTINGS)


def test_an_unknown_setting_is_rejected():
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        axp_select.CONFIG_SCHEMA({"axp_id": "pmu", "vbus_current": {"name": "Nope"}})


def test_a_setting_is_optional():
    assert "charge_current" not in axp_select.CONFIG_SCHEMA({"axp_id": "pmu"})


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
select:
  - platform: axp2101
    charge_current:
      id: charge_current
      name: Charge Current
script:
  - id: slow_charge
    then:
      - select.set:
          id: charge_current
          option: "500mA"
      - select.set_index:
          id: charge_current
          index: 4
"""
    config_file = tmp_path / "test.yaml"
    config_file.write_text(yaml_text)
    parsed = yaml_util.load_yaml(config_file)
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed),
        patch.object(CORE, "config_path", config_file),
    ):
        return config.read_config({})


def test_a_select_can_be_driven_by_the_generic_actions(tmp_path):
    assert _validate(tmp_path) is not None
