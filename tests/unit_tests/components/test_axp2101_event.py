"""Tests for the AXP2101 event platform and the hub's event polling option."""

import logging
from pathlib import Path
import re
from unittest.mock import patch

import pytest

from esphome import config, yaml_util
from esphome.components.axp2101 import CONFIG_SCHEMA, EVENT_NAMES, event as axp_event
from esphome.components.axp_base import event_poll_interval
import esphome.config_validation as cv
from esphome.core import CORE

COMPONENT_DIR = Path(__file__).parents[3] / "esphome" / "components" / "axp2101"


def test_event_names_match_the_cpp_model_table():
    """The Python list must name exactly the interrupt sources of the model table.

    The names are the only thing kept on both sides: Python validates `events:`
    with them and the entity resolves them against the C++ table at setup.
    """
    source = (COMPONENT_DIR / "axp2101.cpp").read_text()
    events_block = source.split("constexpr AxpEvent EVENTS[] = {", 1)[1].split(
        "\n};", 1
    )[0]
    cpp_names = re.findall(r'\.name = "([^"]+)"', events_block)

    assert cpp_names == EVENT_NAMES


def test_event_entity_covers_every_source_by_default():
    config = axp_event.CONFIG_SCHEMA({"axp_id": "pmu", "name": "PMU"})

    assert config[axp_event.CONF_EVENTS] == EVENT_NAMES


def test_events_can_be_narrowed_to_a_few_sources():
    config = axp_event.CONFIG_SCHEMA(
        {
            "axp_id": "pmu",
            "name": "PMU",
            "events": ["power_key_short_press", "vbus_insert"],
        }
    )

    assert config[axp_event.CONF_EVENTS] == ["power_key_short_press", "vbus_insert"]


def test_unknown_event_is_rejected():
    with pytest.raises(cv.Invalid, match="Unknown value 'power_key_triple_press'"):
        axp_event.CONFIG_SCHEMA(
            {"axp_id": "pmu", "name": "PMU", "events": ["power_key_triple_press"]}
        )


def test_an_empty_event_list_is_rejected():
    with pytest.raises(cv.Invalid, match="length"):
        axp_event.CONFIG_SCHEMA({"axp_id": "pmu", "name": "PMU", "events": []})


def test_event_poll_interval_defaults_to_a_hundred_milliseconds():
    config = CONFIG_SCHEMA({"id": "pmu"})

    assert config["event_poll_interval"].total_milliseconds == 100


def test_event_poll_interval_accepts_never():
    # Zero tells the base class to leave the status registers alone, which is
    # what an installation with an IRQ pin wants.
    assert event_poll_interval("never").total_milliseconds == 0


@pytest.mark.parametrize("value", ["10ms", "19ms"])
def test_event_poll_interval_rejects_a_rate_the_main_loop_cannot_keep(value):
    with pytest.raises(cv.Invalid, match="must be at least"):
        event_poll_interval(value)


def test_event_poll_interval_accepts_anything_slower_than_the_loop():
    assert event_poll_interval("20ms").total_milliseconds == 20
    assert event_poll_interval("2s").total_milliseconds == 2000


def _validate(tmp_path, hub: str) -> "config.Config | None":
    """Validate a whole configuration, which is where the hub is looked up."""
    yaml_text = f"""
esphome:
  name: test
esp32:
  board: esp32-s3-devkitc-1
i2c:
  sda: GPIO12
  scl: GPIO11
axp2101:
  id: pmu
{hub}
event:
  - platform: axp2101
    name: PMU
"""
    config_file = tmp_path / "test.yaml"
    config_file.write_text(yaml_text)
    parsed = yaml_util.load_yaml(config_file)
    with (
        patch.object(yaml_util, "load_yaml", return_value=parsed),
        patch.object(CORE, "config_path", config_file),
    ):
        return config.read_config({})


def test_an_event_entity_that_cannot_get_events_warns(tmp_path, caplog):
    # No IRQ pin and no polling leaves the chip with no way to report anything.
    with caplog.at_level(logging.WARNING):
        assert _validate(tmp_path, "  event_poll_interval: never") is not None

    assert "will never fire" in caplog.text


def test_an_irq_pin_makes_never_polling_fine(tmp_path, caplog):
    with caplog.at_level(logging.WARNING):
        assert (
            _validate(tmp_path, "  event_poll_interval: never\n  irq_pin: GPIO14")
            is not None
        )

    assert "will never fire" not in caplog.text
