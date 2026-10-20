# v238 — Terrain layer fusion and an isolated HDR/MSAA experiment

## Capture basis

Quest 3S, v237 O2, original assets, 1680x1760 per eye, FP16 HDR and 2x MSAA.
These captures use different views and populations and are not controlled A/B.

| Scene | v236 mean GPU | v237 mean GPU | v237 GPU median / p95 | v237 mean cadence FPS |
| --- | ---: | ---: | ---: | ---: |
| Megaton | 21.14 ms | 20.80 ms | 23.73 / 27.75 ms | 45.8 |
| Wasteland | 50.68 ms | 42.26 ms | 42.97 / 49.30 ms | 23.0 |

The 72 Hz budget is 13.889 ms. GPU utilisation remains 98–99%. Native LOD is
now actually indexed; unique vertices are about 43% of submitted corners.
LAND still submitted approximately 193–268 visible paint draws and 31–47 base
draws per terrain invocation. The next default change targets that repeated
terrain submission, rasterisation and framebuffer blending. No v238 device
performance or sustained 72 Hz result is claimed yet.

## Default terrain change

Complete stacks containing one base triangle and one to three paint triangles
are combined into one opaque draw with up to four original materials. The
fragment shader evaluates each original material's diffuse, normal, specular,
shadow, local-light and fog calculations, then folds RGB and alpha in original
paint order. It retains FP16 intermediate rounding and fixed-point source
clamping/rounding for the RGBA8 reflection fallback. There is no material bake,
texture atlas, reduced view distance or replacement asset.

Admission requires bit-identical ordered positions, normals, UVs and vertex
colours, opaque base alpha, unique base topology, and GPU-ready paint textures.
Sparse subsets share a material group with zero alpha for absent paints.
Different attributes, ambiguous/repeated topology, and stacks above three paints retain
the original path. The cell/window also retains its original geometry if
material combinations would increase its nonempty draw count. The 15-float
fused vertex includes three paint alphas and uses the existing exact indexing,
sliced upload and geometry retirement paths. Base geometry remains available
to shadows and depth consumers. Cold and streamed paths emit `Q238 TERRAIN
FUSION` counters for folded triangles, material groups and fallback stacks.

The fusion preparation cost is paid when geometry becomes GPU-ready, not on
every draw. Streaming logs must be checked for a new preparation spike. Fusion
is conservative and its real draw savings depend on the authored stacks.

## HDR/MSAA prototype — off by default

`debug.falloutquest.hdr_rtt=1`, set before process launch in Debug/O2 profile,
requests `GL_EXT_multisampled_render_to_texture` for **main views without
potentially visible water**. The target keeps FP16 colour and exactly 2x MSAA.
Colour resolves implicitly into the existing HDR texture; depth remains an EXT
multisample renderbuffer and is explicitly copied to the existing post depth
texture before exposure, bloom, tone mapping and compositor depth consumption.
World drawing never resumes after that copy. Water views use the established
explicit colour/depth snapshot and resume path.

Exact extension/procedure availability, framebuffer completeness, actual colour
and depth sample counts, and the depth blit are checked before live use. Failure
keeps the existing target and is cached until target dimensions/format change.
The extension can discard multisample data after a resolve, so general water
support is deliberately outside this prototype. Depth textures through EXT2
are not used. This experiment may be unsupported or slower on the headset;
`Q238 HDR RTT` states whether admission succeeded. It does not establish a
performance gain simply by removing an explicit colour blit.

CSV format 233 gains appended `hdrRttEyes` and `implicitColorResolvePixels`.
`colorResolvePixels` continues to count only explicit blits. Capture metadata
records `terrainFusion` and `hdrRttRequested`. Existing parsers remain compatible.

## Verification and headset comparison

Host checks cover indexed and expanded complete stacks, ordered alphas, seam
attributes, missing textures, ambiguity, excessive layers, index compaction and
the draw-count guard. The real streamed uploader is exercised with both 12- and
15-float layouts and 17-byte upload budgets. A Mesa GLES pixel test compares the
production fused program with a retained v237 shader fixture across one to
three paints, varying interpolated alpha, normal/specular/fog/shadow/local-light
inputs and FP16/RGBA8 targets. Allowed differences are bounded to 0.002 in FP16
and two 8-bit channel steps for implementation-dependent blend rounding. Driver
mocks check EXT failure paths, capability admission, resource lifetime, resize,
probe caching and depth/renderbuffer state restoration. Android CI compiles
both Debug and native O2 builds and retains the existing gameplay regressions.

1. Install v238 O2 and use its updated `tools/performance/capture-quest.ps1`.
2. First capture the default in the same settled Megaton and Wasteland views.
   Match save, time, view, resolution and population. Check terrain logs, GPU
   median/tails, frame cadence, streaming preparation, seams, normal/specular
   transitions, both eyes, water, interior round trips and lockpick depth.
3. For a same-build terrain comparison set
   `adb shell setprop debug.falloutquest.terrain_legacy 1`, then force-stop and
   relaunch. This disables fusion only; v237 indexing/coverage stays active.
   Restore the property to `0` and relaunch afterwards.
4. Test HDR separately with fusion unchanged. Set
   `adb shell setprop debug.falloutquest.hdr_rtt 1`, force-stop/relaunch, confirm
   `ready=1` and nonzero `hdrRttEyes`, and compare a water-free settled view.
   Pan towards water and verify fallback plus refraction/reflection/depth.
   Restore the property to `0` and relaunch afterwards.
5. Keep the default prototype off until same-view headset timing and visual
   checks show a repeatable benefit. Both debug properties are read once per
   process. Changing them while running is insufficient.

This pass changes renderer implementation only. Original asset selection,
render resolution, authored LOD/view distance, water behaviour, lighting,
post-processing and NPC/gameplay policy remain at their working defaults.
