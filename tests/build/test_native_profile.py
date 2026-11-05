"""Exercise AGP layout differences without building or substituting an APK."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    "check_native_profile", Path(__file__).resolve().parents[2] / "tools/check_native_profile.py")
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


class NativeProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.previous = Path.cwd()
        os.chdir(self.temp.name)

    def tearDown(self):
        os.chdir(self.previous)
        self.temp.cleanup()

    def config(self, variant, *, folder="Debug", flags=None):
        path = Path("app/.cxx") / folder / variant / "arm64-v8a"
        path.mkdir(parents=True)
        (path / "CMakeCache.txt").write_text(
            f"FALLOUTQUEST_OPTIMIZED_PROFILE:BOOL={'ON' if variant == 'profile' else 'OFF'}\n")
        if flags is None:
            flags = ["clang++", "-O0"]
            if variant == "profile":
                flags += ["-O2", "-DFQ_OPTIMIZED_PROFILE=1"]
        (path / "compile_commands.json").write_text(json.dumps([
            {"file": "/source/core/fo3-runtime.cpp", "arguments": flags}]))
        output = Path(f"app/build/outputs/apk/{variant}/app-{variant}.apk")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.touch()  # Only presence is tested; this fixture is not an APK.
        return path

    def test_both_variants_under_native_debug(self):
        self.config("debug")
        self.config("profile")
        profile.verify()

    def test_distinct_gradle_directories(self):
        self.config("debug")
        self.config("profile", folder="Profile")
        profile.verify()

    def test_actual_marker_cannot_identify_the_requested_build(self):
        self.config("debug", flags=["clang++", "-O0", "-O2", "-DFQ_OPTIMIZED_PROFILE=1"])
        self.config("profile")
        with self.assertRaisesRegex(AssertionError, "Baseline Debug"):
            profile.verify()

    def test_last_optimisation_and_diagnostics_still_required(self):
        self.config("debug")
        path = self.config("profile", flags=["clang++", "-O2", "-DFQ_OPTIMIZED_PROFILE=1", "-O0"])
        with self.assertRaisesRegex(AssertionError, "last optimisation"):
            profile.verify()
        (path / "compile_commands.json").write_text(json.dumps([
            {"file": "fo3-runtime.cpp", "arguments": ["clang++", "-O2", "-DFQ_OPTIMIZED_PROFILE=1", "-DNDEBUG"]}]))
        with self.assertRaisesRegex(AssertionError, "NDEBUG"):
            profile.verify()

    def test_profile_only_does_not_prove_baseline(self):
        self.config("profile")
        profile.verify(require_debug=False)
        with self.assertRaisesRegex(RuntimeError, "Expected arm64"):
            profile.verify()

    def test_every_emitted_configuration_is_checked(self):
        self.config("debug")
        self.config("debug", folder="AnotherDebug", flags=["clang++", "-O2"])
        self.config("profile")
        with self.assertRaisesRegex(AssertionError, "already -O2"):
            profile.verify()


if __name__ == "__main__":
    unittest.main()
