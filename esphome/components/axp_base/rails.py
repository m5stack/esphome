"""Ownership checks for the things that can drive an AXP rail.

A power rail has exactly one owner: letting a switch, an output and a regulator
all write the same rail would make them fight over it, with the last writer
winning and none of them reflecting the real state. Chip settings exposed as
child entities (`charging:`, ...) work the same way. The check runs during
validation, so it shows up for `esphome config` as well and not only when code
is generated.
"""

from collections.abc import Iterator

import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PLATFORM
import esphome.final_validate as fv
from esphome.types import ConfigType

CONF_RAIL = "rail"
CONF_RAILS = "rails"
CONF_AXP_ID = "axp_id"

#: Domain whose platform entries declare several rails under one `rails:` key.
REGULATOR_DOMAIN = "regulator"


def rail_schema(channels: list[str]) -> cv.Schema:
    """The `rail` option, restricted to the channels a chip provides."""
    return cv.Schema({cv.Required(CONF_RAIL): cv.one_of(*channels, upper=True)})


def final_validate_rail_claims(domain: str, child_keys: tuple[str, ...] = ()):
    """Build a FINAL_VALIDATE_SCHEMA rejecting two owners driving one thing.

    Final validation sees the whole configuration, so the check can compare
    entries across platform domains. `rail:` entries claim their rail name; each
    key listed in `child_keys` claims that name too. A `regulator:` entry claims
    every rail in its `rails:` list.
    """

    def validate(config: ConfigType) -> None:
        claims: dict[tuple[str, str], str] = {}
        for entity_domain, entries in fv.full_config.get().items():
            if not isinstance(entries, list):
                continue
            for entry in entries:
                if not isinstance(entry, dict):
                    continue
                if entity_domain == REGULATOR_DOMAIN:
                    claimed = _regulator_claims(entry, domain)
                elif entry.get(CONF_PLATFORM) == domain:
                    claimed = _claimed(entity_domain, entry, child_keys)
                else:
                    continue
                for name, label, described in claimed:
                    # The hub id only matters once a chip allows several
                    # instances, but keying on it now keeps the check correct.
                    key = (str(entry.get(CONF_AXP_ID, "")), name)
                    if (owner := claims.get(key)) is not None:
                        raise cv.Invalid(
                            f"{described} is already driven by {owner}, so {label} "
                            f"cannot drive it as well. Each one can only be driven "
                            f"by a single owner."
                        )
                    claims[key] = label

    return validate


def _regulator_claims(entry: ConfigType, domain: str) -> Iterator[tuple[str, str, str]]:
    """Yield every rail a `regulator:` entry of this chip declares."""
    if entry.get(CONF_PLATFORM) != domain:
        return
    for rail in entry.get(CONF_RAILS, []):
        if not isinstance(rail, dict) or (name := rail.get(CONF_RAIL)) is None:
            continue
        label = rail.get(CONF_ID) or name
        yield (
            name,
            f"{REGULATOR_DOMAIN}.{domain} '{label}'",
            f"Regulator '{label}'",
        )


def _claimed(
    entity_domain: str, entry: ConfigType, child_keys: tuple[str, ...]
) -> Iterator[tuple[str, str, str]]:
    """Yield (claim name, entity label, display name) of everything an entry drives."""
    prefix = f"{entity_domain}.{entry[CONF_PLATFORM]}"
    if (rail := entry.get(CONF_RAIL)) is not None:
        label = entry.get(CONF_NAME) or entry.get(CONF_ID)
        yield rail, f"{prefix} '{label}'", f"Rail {rail}"
    for key in child_keys:
        if (child := entry.get(key)) is None:
            continue
        yield (
            key,
            f"{prefix} '{child.get(CONF_NAME) or child.get(CONF_ID)}'",
            f"'{key}'",
        )
