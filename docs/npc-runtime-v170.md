# NPC runtime candidate v170

Baseline main: f999b4d7e8e71cbd0be4263f64cbed230fd55d65.
Version: 170 / 0.45.1-nav-voice-runtime. This is a candidate with established code fixes, not a claim that the reported Quest symptoms are resolved.

## Established defects and changes

* Voice lookup formerly stopped at the first nonempty loose folder or archive directory. A partial extracted voice folder therefore hid dialogue files present only in the installed Voices BSA. The production resolver now merges both discovered audio roots and all discovered sound archives, normalizes case/separators and deduplicates identical asset paths. Distinct matching filenames remain ambiguous and fail explicitly. Windows separators now work for loose-file lookup as well.
* Native completion used a single unfiltered atomic value. A late callback could overwrite a newer completion before the render thread polled it. The mailbox now registers the current token and rejects old/duplicate callbacks. Failure remains distinguishable from success; v168's continue-without-audio policy is preserved.
* Nearest-triangle selection used centroid distance. A point lying on a large triangle could be rejected by the 350-unit limit. Distance now comes from the closest point on the triangle's actual surface. Existing distance limits remain.
* Routes stopped at the goal triangle centroid even when the requested Travel destination was elsewhere inside that triangle. Routes now retain the target projected onto the navigable surface and use actual shared-edge portal midpoints for transitions. A link to a nonmatching boundary is rejected rather than allowing a line through geometry. Portal midpoint routing is conservative; a funnel pass and real Megaton route quality still need original-data/device evaluation.
* An interaction-bound query can sample a skin before package movement in the same stereo frame. Package changes now invalidate that earlier sample. Dialogue facing uses the live runtime position rather than the immutable source placement.
* NVTR's last four bytes were exposed as one flags word. They are now separate 16-bit triangle/cover flags, following xEdit's Fallout 3 schema. NVEX remains the existing 10-byte unknown-u32 / NAVM-FormID / triangle-u16 format; the low three external-edge bits were already correct.

The package/navigation bridge is separately included by production and a host test, so the test executes the actual selection, conversion, A*, movement, completion and repath logic. Existing actor upload/render code remains intact.

## Original voice evidence

The original extracted Dropbox file was downloaded and fully decoded with FFmpeg:

`sound/voice/fallout3.esm/maleuniquesimms/ms11_greeting_0003da20_3.ogg`

52,063 bytes; mono Vorbis; 44,100 Hz; 5.256485 seconds. The repository's prior original-data dialogue regression identifies INFO 0003DA20, response 3, voice 00061EA1 / MaleUniqueSimms, request `@voice:MaleUniqueSimms:_0003da20_3`, and NAM1 beginning “Name's Lucas Simms, town sheriff.” Those ESM expectations were not reverified against a complete ESM in this session.

The new resolver test successfully looks up and extracts those exact original bytes from a generated temporary BSA with a partial loose folder present. This tests the shared reader/resolver, not the user's actual Fallout - Voices.bsa. No Bethesda files are committed or packaged. No unusual codec was found; Android MediaPlayer playback and Quest output are still unverified.

## Verification and limits

Host coverage includes partial-folder/archive lookup, normalization/deduplication, ambiguity, extraction, stale/duplicate completion, failure versus success, NAVTR flag separation, external references, two-NAVM traversal, invalid target/geometry rejection, scene/game conversion, large-triangle containment, exact same-triangle destination, unsupported priority fall-through, overnight schedules, Travel once, Wander/Sandbox repeated routes, dialogue suspension and skin invalidation. Existing dialogue tests cover Speaking until callback and token rejection.

The six CMake host suites pass (28 passed, one optional external VR-body asset test skipped). Render/source checks and the workflow's Android build are verified separately in the delivery. The APK continues to discover the installed Data root through GameInstall; original audio remains in the install.

The ESM attachment failed to become available. The installed Voices BSA and headset logcat were not supplied. Consequently this work cannot identify the user's exact failing stage, validate live Megaton external boundaries/PKID selection, or confirm audible full-duration playback and normal walking. Targeted logs now include voice request/prefix/resolved path/archive/byte size/container header, platform preparation/gates/duration/completion/token, and actor/package/NAVM/triangle/endpoints/waypoints/path failure. These are needed to complete the runtime investigation.

Schedules still use the existing game clock and limited supported calendar conditions. Movement speed remains the existing explicit VR bridge at 1.05 m/s; it is not a recovered Bethesda actor-value rule. Eat, Sleep, Patrol, Escort, scripted packages, sandbox furniture use, and NPC doors/cell traversal remain unsupported. Existing lower-priority supported-package selection is retained, not presented as full vanilla simulation. Scene rebuild persistence and nonresident actor simulation are still incomplete.

Source for binary schema: https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsFO3.pas

## Device reproduction

Install v170, launch through game setup, enter Megaton, speak to Lucas and allow his first response to finish; leave dialogue and observe several NPCs. Capture startup plus the reproduction with:

```powershell
adb logcat -c
adb logcat -v threadtime > falloutquest-v170-logcat.txt
```

Stop with Ctrl+C after testing. Supply that log, a complete Fallout3.esm and Fallout - Voices.bsa (or the installed archive's directory listing and extraction result) to finish distinguishing installed-data failures, lifecycle/decoder failures and unsupported packages from navigation failures.
