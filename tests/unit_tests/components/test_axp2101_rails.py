"""Tests for the AXP2101 rail selection shared by the switch and output platforms."""

from pathlib import Path
import re

import pytest

from esphome.components.axp2101 import (
    RAIL_CHANNELS,
    output as axp_output,
    switch as axp_switch,
)
from esphome.components.axp_base.rails import final_validate_rail_claims
import esphome.config_validation as cv
import esphome.final_validate as fv

COMPONENT_DIR = Path(__file__).parents[3] / "esphome" / "components" / "axp2101"


def test_power_channels_match_the_cpp_model_table():
    """The Python rail list must name exactly the rails the C++ table defines.

    The names are the only thing kept on both sides, so a rename in one place
    would otherwise only fail on a device.
    """
    source = (COMPONENT_DIR / "axp2101.cpp").read_text()
    rails_block = source.split("constexpr AxpRail RAILS[] = {", 1)[1].split("\n};", 1)[
        0
    ]
    cpp_names = re.findall(r'\.name = "([^"]+)"', rails_block)

    assert cpp_names == RAIL_CHANNELS


def _claim(config: dict) -> None:
    token = fv.full_config.set(config)
    try:
        final_validate_rail_claims("axp2101")({})
    finally:
        fv.full_config.reset(token)


def _switch(**kwargs) -> dict:
    return {"platform": "axp2101", "axp_id": "pmu", "name": "Rail Switch", **kwargs}


def _output(**kwargs) -> dict:
    return {"platform": "axp2101", "axp_id": "pmu", "id": "rail_output", **kwargs}


def test_one_rail_may_not_be_driven_by_two_entities():
    with pytest.raises(cv.Invalid, match="already driven by switch.axp2101"):
        _claim({"switch": [_switch(rail="DCDC3")], "output": [_output(rail="DCDC3")]})


def test_two_switches_may_not_share_a_rail():
    with pytest.raises(cv.Invalid, match="already driven by switch.axp2101"):
        _claim({"switch": [_switch(rail="ALDO2"), _switch(rail="ALDO2")]})


def test_each_rail_may_have_its_own_entity():
    _claim({"switch": [_switch(rail="DCDC3")], "output": [_output(rail="ALDO1")]})


def test_the_same_rail_name_on_two_chips_does_not_collide():
    _claim(
        {
            "switch": [_switch(rail="DCDC3", axp_id="pmu")],
            "output": [_output(rail="DCDC3", axp_id="other")],
        }
    )


def test_entries_without_a_rail_are_ignored():
    # Not every axp2101 platform entry drives a rail (sensors, for example).
    _claim({"sensor": [{"platform": "axp2101", "axp_id": "pmu", "id": "batt"}]})


@pytest.mark.parametrize("key, value", [("min_power", "20%"), ("max_power", "80%")])
def test_output_rejects_the_generic_power_scaling(key, value):
    with pytest.raises(cv.Invalid, match=f"'{key}' is not supported"):
        axp_output.CONFIG_SCHEMA({"rail": "DCDC3", "axp_id": "pmu", key: value})


@pytest.mark.parametrize("key, value", [("inverted", True), ("power_supply", "psu")])
def test_output_rejects_keys_that_make_no_sense_for_a_voltage(key, value):
    with pytest.raises(cv.Invalid, match=f"'{key}' is not supported"):
        axp_output.CONFIG_SCHEMA({"rail": "DCDC3", "axp_id": "pmu", key: value})


def test_output_rejects_an_unknown_option():
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        axp_output.CONFIG_SCHEMA({"rail": "DCDC3", "axp_id": "pmu", "nonsense": True})


def test_output_defaults_cover_the_whole_rail():
    config = axp_output.CONFIG_SCHEMA({"rail": "DCDC3", "axp_id": "pmu"})

    assert config[axp_output.CONF_MIN_VOLTAGE] == 1800
    assert config[axp_output.CONF_MAX_VOLTAGE] == 3300
    assert config[axp_output.CONF_DVM] is False


def test_output_rejects_an_inverted_voltage_range():
    with pytest.raises(cv.Invalid, match="must be below"):
        axp_output.CONFIG_SCHEMA(
            {"rail": "DCDC3", "axp_id": "pmu", "min_voltage": 3300, "max_voltage": 2800}
        )


def test_output_rejects_a_voltage_no_rail_can_reach():
    with pytest.raises(cv.Invalid, match="must be at least 500"):
        axp_output.CONFIG_SCHEMA({"rail": "DCDC3", "axp_id": "pmu", "min_voltage": 100})


@pytest.mark.parametrize("platform", [axp_switch, axp_output])
def test_rail_names_are_checked_against_the_chip(platform):
    entity = {"id": "o"} if platform is axp_output else {"name": "s"}
    with pytest.raises(cv.Invalid, match="Unknown value 'DCDC9'"):
        platform.CONFIG_SCHEMA({"rail": "DCDC9", "axp_id": "pmu", **entity})


def test_switch_defaults_to_off_after_boot():
    config = axp_switch.CONFIG_SCHEMA({"rail": "ALDO3", "axp_id": "pmu", "name": "Sw"})

    assert config["restore_mode"] == "ALWAYS_OFF"
