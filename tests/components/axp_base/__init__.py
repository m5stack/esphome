import esphome.codegen as cg
from esphome.types import ConfigType
from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    """Give the unit test binary the define its code paths need.

    `AXP_POLLED_ENTITY_COUNT` normally comes from the platform components' slot
    requests; the test registers entities by hand, so it supplies the count.
    """

    async def to_code_testing(config: ConfigType) -> None:
        cg.add_define("AXP_POLLED_ENTITY_COUNT", 4)

    manifest.to_code = to_code_testing
