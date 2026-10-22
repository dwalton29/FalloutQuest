# v242 — terrain shader compilation and fallback

The user reports that enabling terrain fusion removes the terrain. The supplied
v241 capture uses the working legacy path (`terrain_legacy=1`, `terrainFusion=0`)
and therefore does not contain the fused program's device compilation failure.

Strict glslang compilation of the v241 production fused shader reproduces a
syntax error: `unexpected SHARED`. Its Q239 lighting cache used `shared`, a GLSL
reserved word, for locals and a function parameter. Mesa's GLES compiler accepted
the same source, so the existing pixel parity test missed this portability bug.
Rename these identifiers to `lighting`. The shader calculations, original LAND
assets, fusion admission, distance and render settings are unchanged.

If the device still rejects the fused program at compilation or linking, retry
the original v237 program and disable CPU layer fusion for later cold/streamed
admissions in that process. This happens before original triangles are removed.
The existing `terrain_legacy=1` manual override remains available. Logs identify
the active fusion policy, device renderer and fallback event. If even the
original program fails, retain the existing explicit failure rather than
publishing incompatible fused geometry. Fallback does not detect every possible
post-link device rendering defect.

Validation: strict glslang compilation/linking; production GLES pixel parity for
FP16 and RGBA8; actual program-factory tests with injected vertex/fragment/link
failures, both-program failure and subsequent admissions; existing terrain
geometry and sliced-upload tests. No headset FPS improvement is asserted.

After installing v242 O2, set `debug.falloutquest.terrain_legacy` to `0`, force-stop
and relaunch to try corrected fusion. Verify terrain before comparing speed.
If terrain is still absent, restore `1`, restart, and capture startup logcat
including `Q7.20 terrain shader`, `Q242 TERRAIN` and `Q10.7 TERRAIN FAILED`.
