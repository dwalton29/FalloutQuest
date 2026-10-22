#!/usr/bin/env python3
"""Fail CI if either Quest comparison APK was built with unexpected C++ flags.

Uses the CMake compile_commands.json emitted by Android Gradle Plugin rather
than guessing defaults from the Gradle build type or Android Studio labels.
"""
import argparse
import json
from pathlib import Path
import shlex


def native_commands():
    results = {}
    for path in Path("app/.cxx").rglob("compile_commands.json"):
        parts = [part.lower() for part in path.parts]
        variant = next((kind for kind in ("debug", "profile") if kind in parts), None)
        if not variant:
            continue
        records = json.loads(path.read_text(encoding="utf-8"))
        for record in records:
            if Path(record["file"]).name != "fo3-runtime.cpp":
                continue
            flags = record.get("arguments") or shlex.split(record["command"])
            results[variant] = (path, flags)
    return results


def verify(require_debug=True):
    records = native_commands()
    required = {"profile", "debug"} if require_debug else {"profile"}
    if not required.issubset(records):
        raise RuntimeError(
            f"Expected arm64-v8a {sorted(required)} compile commands; "
            f"found {list(records)}"
        )
    for variant in ("debug", "profile") if require_debug else ("profile",):
        source, flags = records[variant]
        optimisations = [arg for arg in flags if arg in
                         ("-O0", "-O1", "-O2", "-O3", "-Os", "-Oz", "-Og")]
        last = optimisations[-1] if optimisations else "(none)"
        defines = [arg for arg in flags if arg.startswith("-D")]
        has_marker = "-DFQ_OPTIMIZED_PROFILE=1" in defines
        ndebug = any(arg == "-DNDEBUG" or arg.startswith("-DNDEBUG=")
                     for arg in defines)
        if variant == "profile":
            assert last == "-O2", f"Profile last optimisation must be -O2, got {optimisations}"
            assert has_marker, "Profile source is missing FQ_OPTIMIZED_PROFILE=1"
            assert not ndebug, "NDEBUG strips performance capture and diagnostics"
        else:
            assert not has_marker, "Baseline Debug must not have the O2 profile marker"
            assert last != "-O2", f"Debug baseline is already -O2: {optimisations}"
        print(
            f"{variant}: {source}; optimisation flags={optimisations}; "
            f"effective={last}; profile_marker={has_marker}; NDEBUG={ndebug}"
        )
    for name in ("debug", "profile") if require_debug else ("profile",):
        apk = Path(f"app/build/outputs/apk/{name}/app-{name}.apk")
        assert apk.is_file(), f"Missing {name} APK: {apk}"
        print(f"{name} apk bytes={apk.stat().st_size}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile-only", action="store_true",
                        help="Fast push builds produce only the O2 APK.")
    args = parser.parse_args()
    verify(require_debug=not args.profile_only)
