"""Load the audio_dac base class the amplifier builds on in the C++ test build."""

from tests.testing_helpers import ComponentManifestOverride


def override_manifest(manifest: ComponentManifestOverride) -> None:
    manifest.dependencies = manifest.dependencies + ["audio_dac"]
