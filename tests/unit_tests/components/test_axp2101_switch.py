"""Tests for the AXP2101 switch platform: rail switches and on/off settings."""

from pathlib import Path
import re

import pytest

from esphome.components.axp2101 import switch as axp_switch
import esphome.config_validation as cv
import esphome.final_validate as fv

COMPONENT_DIR = Path(__file__).parents[3] / "esphome" / "components" / "axp2101"


def _model_flags() -> list[str]:
    source = (COMPONENT_DIR / "axp2101.cpp").read_text()
    block = source.split("constexpr AxpFlag FLAGS[] = {", 1)[1].split("\n};", 1)[0]
    return re.findall(r'\.name = "([^"]+)"', block)


def test_flag_keys_match_the_cpp_flag_table():
    assert list(axp_switch.FLAGS) == _model_flags()


def test_a_rail_entry_keeps_its_shape():
    config = axp_switch.CONFIG_SCHEMA(
        {"rail": "ALDO3", "name": "Peripherals", "axp_id": "pmu"}
    )

    assert config["rail"] == "ALDO3"
    assert config["name"] == "Peripherals"


@pytest.mark.parametrize("key", ["charging", "gauge"])
def test_each_flag_can_be_a_switch(key):
    config = axp_switch.CONFIG_SCHEMA(
        {"axp_id": "pmu", key: {"name": "Setting", "id": "setting_switch"}}
    )

    assert config[key]["name"] == "Setting"
    # The chip keeps these across a reboot, so the default is to report what it
    # finds rather than to write a state nobody asked for.
    assert config[key]["restore_mode"] == "DISABLED"


def test_a_rail_and_a_flag_in_one_entry_are_rejected():
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        axp_switch.CONFIG_SCHEMA(
            {
                "axp_id": "pmu",
                "rail": "ALDO3",
                "name": "Mixed",
                "charging": {"name": "C"},
            }
        )


def test_an_entry_needs_either_a_rail_or_a_flag():
    with pytest.raises(
        cv.Invalid, match="'rail' or one of charging, gauge is required"
    ):
        axp_switch.CONFIG_SCHEMA({"axp_id": "pmu"})


def test_an_unknown_flag_is_rejected():
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        axp_switch.CONFIG_SCHEMA({"axp_id": "pmu", "discharging": {"name": "No"}})


def _claim(config: dict) -> None:
    token = fv.full_config.set(config)
    try:
        axp_switch.FINAL_VALIDATE_SCHEMA({})
    finally:
        fv.full_config.reset(token)


def test_a_setting_cannot_be_exposed_twice():
    entry = {"platform": "axp2101", "axp_id": "pmu"}
    with pytest.raises(cv.Invalid, match="'charging' is already driven by"):
        _claim(
            {
                "switch": [
                    {**entry, "charging": {"name": "First", "id": "first"}},
                    {**entry, "charging": {"name": "Second", "id": "second"}},
                ]
            }
        )


def test_two_flags_in_one_entry_are_fine():
    _claim(
        {
            "switch": [
                {
                    "platform": "axp2101",
                    "axp_id": "pmu",
                    "charging": {"name": "Charging", "id": "charging_switch"},
                    "gauge": {"name": "Gauge", "id": "gauge_switch"},
                }
            ]
        }
    )
