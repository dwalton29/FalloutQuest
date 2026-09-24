#!/usr/bin/env python3
"""One-time Q17 consolidation of the mature generated FalloutQuest runtime.

This script is intentionally mechanical: it copies the exact generated sources
produced by the Q16.27 build, rewrites only build-directory include paths, fixes
one declaration-order defect exposed by native compilation, and replaces the
CMake source-rewrite chain with a normal source list.
"""

from pathlib import Path
import re
import shutil
import textwrap

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "app" / "src" / "main" / "cpp"
BUILD_ROOT = ROOT / "app" / ".cxx"


def find_generated_dir() -> Path:
    matches = list(BUILD_ROOT.rglob("q6h-native-generated.cpp"))
    if not matches:
        raise SystemExit("Q17: q6h-native-generated.cpp was not produced")
    if len(matches) > 1:
        matches.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return matches[0].parent


def copy_generated(gen: Path) -> list[Path]:
    mapping = {
        "q6h-native-generated.cpp": "fo3-runtime.cpp",
        "q1800-q4-generated.cpp": "fo3-runtime-loop.inc",
        "fo3-interaction-hud-q1830.h": "fo3-interaction-hud-runtime.h",
        "fo3-cell-spawn-q1980.cpp": "fo3-cell-world.cpp",
        "fo3-worldspace-q1980.cpp": "fo3-worldspace-runtime.inc",
        "fo3-terrain-data-q720.cpp": "fo3-terrain-data-runtime.inc",
        "fo3-terrain-render-q720.cpp": "fo3-terrain-render-runtime.inc",
        "fo3-terrain-ground-q720.cpp": "fo3-terrain-ground-runtime.inc",
        "fo3-static-nif-q6h.cpp": "fo3-static-nif-runtime.cpp",
        "fo3-collision-overlay-q74.cpp": "fo3-collision-runtime.cpp",
        "fo3-collision-controller-q960.inc": "fo3-player-controller-runtime.inc",
    }
    written = []
    for source, target in mapping.items():
        source_path = gen / source
        if not source_path.exists():
            raise SystemExit(f"Q17: generated source missing: {source}")
        target_path = SRC / target
        shutil.copyfile(source_path, target_path)
        written.append(target_path)
    return written


def replace_once(path: Path, pattern: str, replacement: str) -> None:
    text = path.read_text()
    text, count = re.subn(pattern, replacement, text, count=1)
    if count != 1:
        raise SystemExit(
            f"Q17: {path.name}: expected one replacement for {pattern!r}, got {count}"
        )
    path.write_text(text)


def rewrite_generated_includes() -> None:
    replace_once(
        SRC / "fo3-runtime.cpp",
        r'#include "/home/runner/.*/q1800-q4-generated\.cpp"',
        '#include "fo3-runtime-loop.inc"',
    )
    replace_once(
        SRC / "fo3-runtime-loop.inc",
        r'#include "/home/runner/.*/fo3-interaction-hud-q1830\.h"',
        '#include "fo3-interaction-hud-runtime.h"',
    )

    cell = SRC / "fo3-cell-world.cpp"
    for generated, canonical in [
        ("fo3-worldspace-q1980.cpp", "fo3-worldspace-runtime.inc"),
        ("fo3-terrain-data-q720.cpp", "fo3-terrain-data-runtime.inc"),
        ("fo3-terrain-render-q720.cpp", "fo3-terrain-render-runtime.inc"),
        ("fo3-terrain-ground-q720.cpp", "fo3-terrain-ground-runtime.inc"),
    ]:
        replace_once(
            cell,
            rf'#include "/home/runner/.*/{re.escape(generated)}"',
            f'#include "{canonical}"',
        )

    replace_once(
        SRC / "fo3-collision-runtime.cpp",
        r'#include "/home/runner/.*/fo3-collision-controller-q960\.inc"',
        '#include "fo3-player-controller-runtime.inc"',
    )


def repair_collision_declaration_order() -> None:
    path = SRC / "fo3-collision-runtime.cpp"
    text = path.read_text()

    init_anchor = "bool InitializeFo3CollisionOverlay("
    if init_anchor not in text:
        raise SystemExit("Q17: collision initializer anchor missing")
    text = text.replace(
        init_anchor,
        "void InvalidateDerivedCollisionCachesQ17();\n\n" + init_anchor,
        1,
    )

    old = (
        "    gHkWeldAdjacencyReadyQ801 = false;\n"
        "    gHkShapesReadyQ900 = false;\n"
        "    gQ950Ready = false;\n"
        "    gPlayerCollisionReady = true;"
    )
    new = (
        "    InvalidateDerivedCollisionCachesQ17();\n"
        "    gPlayerCollisionReady = true;"
    )
    if old not in text:
        raise SystemExit("Q17: Q16.27 derived-cache invalidation block missing")
    text = text.replace(old, new, 1)

    include_anchor = '#include "fo3-player-controller-runtime.inc"\n'
    definition = textwrap.dedent(
        """\
        #include "fo3-player-controller-runtime.inc"

        void InvalidateDerivedCollisionCachesQ17() {
            gHkWeldAdjacencyReadyQ801 = false;
            gHkShapesReadyQ900 = false;
            gQ950Ready = false;
        }
        """
    )
    if include_anchor not in text:
        raise SystemExit("Q17: player-controller include missing")
    text = text.replace(include_anchor, definition, 1)
    path.write_text(text)


def write_cmake() -> None:
    (SRC / "CMakeLists.txt").write_text(
        textwrap.dedent(
            """\
            cmake_minimum_required(VERSION 3.22.1)
            project(falloutquest LANGUAGES C CXX)

            find_package(OpenXR REQUIRED CONFIG)

            # Q17 consolidated baseline: normal sources, no generated-source patch chain.
            add_library(falloutquest SHARED
                fo3-runtime.cpp
                fo3-megaton-scene.cpp
                fo3-cell-world.cpp
                fo3-static-nif-runtime.cpp
                fo3-nif-collision.cpp
                fo3-collision-runtime.cpp
                fo3-bsa-reader.cpp
                fo3-texture-bsa.cpp
                fo3-data-probe.cpp
                fo3-esm-scan.cpp
                fo3-megaton-contents.cpp
                fo3-bsa-extract.cpp
                fo3-fog-ab-q1450.cpp
                ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
            )

            target_include_directories(falloutquest PRIVATE
                ${ANDROID_NDK}/sources/android/native_app_glue
                ${CMAKE_CURRENT_SOURCE_DIR}
            )

            target_compile_options(falloutquest PRIVATE
                -include ${CMAKE_CURRENT_SOURCE_DIR}/openxr-platform-prelude.h
            )

            find_library(android-lib android)
            find_library(log-lib log)
            find_library(egl-lib EGL)
            find_library(gles-lib GLESv3)
            find_library(z-lib z)

            target_link_libraries(falloutquest
                OpenXR::openxr_loader
                ${android-lib}
                ${log-lib}
                ${egl-lib}
                ${gles-lib}
                ${z-lib}
            )
            """
        )
    )


def retire_patch_chain() -> None:
    for path in SRC.glob("q*.cmake"):
        path.unlink()
    for name in (
        "q4-native.cpp",
        "q5g-native.cpp",
        "q5h-native.cpp",
        "q5i-native.cpp",
        "q6a-native.cpp",
    ):
        path = SRC / name
        if path.exists():
            path.unlink()


def write_docs() -> None:
    (ROOT / "README.md").write_text(
        textwrap.dedent(
            """\
            # FalloutQuest

            Experimental standalone Meta Quest runtime for user-supplied Fallout 3 game data.

            ## Current milestone: Q17 — consolidated native runtime

            FalloutQuest is a native ARM64/OpenXR reimplementation of the runtime needed to
            interpret Fallout 3's original data on Quest. It is not a port of the original
            Windows executable.

            The Q2–Q16 prototype phase proved the major data/rendering path: BSA and ESM
            access, NIF geometry/material loading, authored CELL/REFR placement, interiors,
            doors/XTEL transitions, LAND terrain, collision/player movement, exterior
            streaming, Level4 LOD, weather/imagespace/lighting work, loading/UI
            infrastructure, and standalone OpenXR rendering.

            Q17 freezes the mature generated Q16.27 runtime into ordinary C++ source files.
            The historical Q-series CMake text-rewrite chain is no longer part of the build.

            See docs/ARCHITECTURE.md for the current source layout and development rules.

            ## Build

            GitHub Actions builds an ARM64 debug APK for Quest. The native target uses
            Android NDK 27, C++17, OpenXR, EGL and GLES3.

            ## Asset policy

            This repository does not contain Bethesda game assets, executables, ESM files,
            BSA archives, textures, meshes, sounds, or other copyrighted Fallout 3 data.
            Users supply files from their own legitimate Fallout 3 installation.

            This project is not affiliated with or endorsed by Bethesda Softworks or Meta.
            """
        )
    )

    docs = ROOT / "docs"
    docs.mkdir(exist_ok=True)
    (docs / "ARCHITECTURE.md").write_text(
        textwrap.dedent(
            """\
            # FalloutQuest architecture

            ## Q17 baseline

            Q17 is the consolidation boundary between the exploratory Q-series prototype
            and the maintained engine.

            Before Q17, CMake progressively read older C++ files as text and applied more
            than one hundred Q-numbered string replacements to generate the source that was
            actually compiled. That was effective for rapid reverse-engineering, but later
            changes became dependent on brittle textual anchors.

            Q17 captures the exact mature generated runtime and makes it canonical source.
            Future engine changes should modify normal C++ directly. Do not reintroduce
            generated-source patching for engine behaviour.

            ## Canonical native translation units

            - fo3-runtime.cpp: OpenXR lifecycle, renderer, scene/runtime streaming and frame loop.
            - fo3-runtime-loop.inc: mature runtime/frame implementation included by fo3-runtime.cpp.
            - fo3-cell-world.cpp: CELL/WRLD/LAND scene construction and terrain integration.
            - fo3-static-nif-runtime.cpp: static NIF render-mesh/material loading.
            - fo3-collision-runtime.cpp: authored collision world and collision entry points.
            - fo3-player-controller-runtime.inc: consolidated exterior player controller.
            - fo3-worldspace-runtime.inc and fo3-terrain-*-runtime.inc: consolidated world/terrain implementation.
            - Existing BSA, ESM, NIF and texture helpers remain normal source files.

            ## Source-of-truth rule

            Fallout 3's supplied data and observable PC runtime behaviour are authoritative
            wherever practical. Do not invent replacement world geometry, materials,
            lighting values, placements, or game-authored content when those can reasonably
            be recovered from the user's game files.

            Quest/OpenXR-specific behaviour may be implemented where Fallout 3 has no native
            equivalent, including stereo presentation, VR input, comfort behaviour and
            mobile performance adaptations. Keep those adaptations separate from authored
            Fallout data.

            ## Renderer verification

            For renderer discrepancies, prefer measured comparison with the PC reference path
            over screenshot tuning. tools/pc-d3d9-logger captures Fallout 3 D3D9 shader,
            state and constants and maps them to Shader Package 17.

            ## Development rule

            main is the working branch unless explicitly requested otherwise. Changes should
            leave the cloud APK build passing before additional systems are layered on top.
            """
        )
    )


def verify_no_build_paths(paths: list[Path]) -> None:
    for path in paths:
        text = path.read_text()
        if "/home/runner/" in text or "app/.cxx/" in text:
            raise SystemExit(f"Q17: build-directory reference survived in {path.name}")


def main() -> None:
    if (SRC / "fo3-runtime.cpp").exists():
        print("Q17: canonical runtime already present; nothing to migrate")
        return

    gen = find_generated_dir()
    written = copy_generated(gen)
    rewrite_generated_includes()
    repair_collision_declaration_order()
    write_cmake()
    retire_patch_chain()
    write_docs()
    verify_no_build_paths(written)
    print(f"Q17: consolidated runtime from {gen}")


if __name__ == "__main__":
    main()
