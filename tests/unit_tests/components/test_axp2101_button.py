"""Tests for the AXP2101 button platform and the chip's automation actions."""

from unittest.mock import patch

import pytest

from esphome import config, yaml_util
from esphome.automation import ACTION_REGISTRY
from esphome.components.axp2101 import (
    CHIP_OPERATIONS,
    CONF_RESTART,
    CONF_SHUTDOWN,
    button as axp_button,
)
import esphome.config_validation as cv
from esphome.core import CORE

DOMAIN = "axp2101"


def test_every_operation_has_a_button_and_a_matching_setting():
    """The button keys come from CHIP_OPERATIONS, so the two must not drift."""
    assert set(axp_button.BUTTON_SETTINGS) == set(CHIP_OPERATIONS)


def test_every_operation_is_available_as_an_action():
    for operation in CHIP_OPERATIONS:
        assert ACTION_REGISTRY.get(f"{DOMAIN}.{operation}") is not None


def test_buttons_are_all_optional():
    config = axp_button.CONFIG_SCHEMA({"axp_id": "pmu"})

    assert CONF_SHUTDOWN not in config


@pytest.mark.parametrize("operation", list(CHIP_OPERATIONS))
def test_each_operation_can_be_configured_as_a_button(operation):
    config = axp_button.CONFIG_SCHEMA({"axp_id": "pmu", operation: {"name": "Do It"}})

    assert config[operation]["name"] == "Do It"


def test_an_unknown_operation_is_rejected():
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        axp_button.CONFIG_SCHEMA({"axp_id": "pmu", "power_cycle": {"name": "Nope"}})


def test_restart_button_carries_the_restart_device_class():
    config = axp_button.CONFIG_SCHEMA(
        {"axp_id": "pmu", CONF_RESTART: {"name": "Restart"}}
    )

    assert config[CONF_RESTART]["device_class"] == "restart"


def _validate_config(tmp_path, body: str):
    yaml_text = f"""
esphome:
  name: test
  on_boot:
    then:
{body}
esp32:
  board: esp32-s3-devkitc-1
i2c:
  sda: GPIO12
  scl: GPIO11
axp2101:
  id: pmu
button:
  - platform: axp2101
    shutdown:
      name: Power Off
"""
    config_file = tmp_path / "test.yaml"
    config_file.write_text(yaml_text)
    parsed = yaml_util.load_yaml(config_file)
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed),
        patch.object(CORE, "config_path", config_file),
    ):
        return config.read_config({})


@pytest.mark.parametrize("operation", list(CHIP_OPERATIONS))
def test_actions_are_usable_with_a_bare_id(tmp_path, operation):
    assert _validate_config(tmp_path, f"      - axp2101.{operation}: pmu") is not None


@pytest.mark.parametrize("operation", list(CHIP_OPERATIONS))
def test_actions_are_usable_with_an_id_mapping(tmp_path, operation):
    body = f"      - axp2101.{operation}:\n          id: pmu"
    assert _validate_config(tmp_path, body) is not None


def test_an_action_pointing_at_a_missing_hub_fails(tmp_path):
    assert _validate_config(tmp_path, "      - axp2101.shutdown: other") is None
