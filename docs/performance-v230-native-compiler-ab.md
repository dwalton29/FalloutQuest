# Same-code native compiler A/B: v230 Debug vs Profile O2

This change intentionally **does not** bump versionCode 230 or modify Fallout 3
world systems. Both APK variants compile identical game source at the same Git
commit and retain the same `com.falloutquest.app` application ID and debug
signing key. Installing one over the other preserves game files/saves.

## Build variants

| APK artifact | Gradle target | Native flags | Diagnostics |
| --- | --- | --- | --- |
| FalloutQuest-debug | assembleDebug | Android Debug CMake defaults | yes |
| FalloutQuest-O2-profile | assembleProfile | -O2 added last by target_compile_options | yes |

Profile extends the Debug variant, preserving Android debuggability, game
permissions, package name, signing, `#ifndef NDEBUG` performance capture,
and symbols. `-O2` is native only; shader draw order, quality, original
scripts, content and render configuration are unchanged.

`tools/check_native_profile.py` inspects **effective** compile_commands.json
for each variant and fails CI unless profile's final optimisation flag is
`-O2`, `FQ_OPTIMIZED_PROFILE=1` is present, `NDEBUG` is absent, and the
baseline isn't already `-O2`. The runtime logs a build marker at EGL init.

## Headset A/B protocol

1. Download both variants from the *same GitHub Actions run*. Use the same
   save, Megaton NPC population, viewpoint, headset refresh and thermal state.
2. `adb install -r app-debug.apk`. Start FalloutQuest. Verify
   `FQ BUILD CONFIG: variant=debug` in logcat; capture a representative
   30–60 s stable gameplay session using the existing FQPerf property.
3. Install the Profile APK over the Debug variant with `adb install -r
   app-profile.apk`. Verify `variant=profile nativeOptimisation=O2` in
   logcat. Repeat the same scene and recording.
4. Compare active-frame CPU workUs median/p90, eye0/eye1 CPU work,
   NPC/script spikes, cadence, and available GPU/VrApi stats.
5. If profile is measurably faster without gameplay regressions, consider
   making an optimised *development* build the default. If it isn't, don't
   assume the compiler was the bottleneck.

Caution: Both APKs share a package name and versionCode. They can't be
installed simultaneously, but `adb install -r` swaps them without wiping
saved application data. GitHub `versionName` for Profile ends in `-O2`.
Native optimiser can expose undefined behaviour, so explicitly test loading,
Megaton dialogue/AI, weapon use, and quest scripts.
