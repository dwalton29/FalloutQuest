# v281 — native Vulkan LAND fusion

## What was broken, and what is proven

The earlier GLES fused shader had a reserved-word compile error, repaired in
v242. The subsequent Adreno capture linked successfully and still had missing
ground with **zero folded groups**. That historical post-link failure has not
been reproduced here; neither a CPU merge error nor the fixes below can honestly
be claimed as its complete explanation. GLES retains the recovered v237 path.

The investigation found three concrete defects/limitations:

1. Vulkan had no fused material. Its terrain program was the frozen v237
   program, with attributes 0–4 and no paint descriptors. Enabling CPU fusion
   alone would delete paint triangles without drawing their replacement colours.
2. The fused shader used implicit texture LOD inside varying paint-alpha
   branches. Such derivatives are undefined in non-uniform control flow. The
   earlier 1×1 pixel tests did not exercise mip selection. Gradients are now
   computed before discard/paint branches and passed to `textureGrad` for both
   diffuse and normal sampling. This is a portability repair, not proof of the
   zero-group disappearance's cause. See the [Khronos GLSL rules](https://github.khronos.org/Vulkan-Site/glsl/latest/chapters/builtinfunctions.html).
3. A cell-wide draw guard threw away all fusion if material fragmentation
   increased the cell's draw count. Original LAND probes show this rejecting
   useful work. Admission now restores the least useful groups until the same
   no-increase draw budget is met, rather than discarding every group.

## Native implementation

The shader builder ports a separate fused material from the existing production
LAND shader. The retained v237 SPIR-V remains available as the working reference
and restart fallback. The new material carries the three paint alphas at vertex
location 5 (15-float layout), paint count, six paint samplers, normal/specular
flags and blend-rounding mode. Twelve-float fallback geometry is valid with zero
paint count. Native scene rounding follows the actual FP16 colour target, even
if compatibility RTT state supplies an inactive/incorrect legacy round mode.

Solid and spatially clipped variants, stereo camera transforms, fragment-density
passes and both SpaceWarp colour attachments have matching fused variants.
The private motion/composition layout moves to its own index; it cannot alias
the new material's larger descriptor interface. Existing dynamic motion tests
remain part of the same GPU suite.

CPU fusion still requires exact topology, UVs, normals and vertex colours, base
alpha one, and at most three complete ordered paint layers. It preserves the
original depth geometry. Unsupported stacks stay on the original multipass
path. Group pruning restores whole original triangles with their paint data;
it does not approximate opacity, reorder materials, reduce texture resolution
or change distance/fog. Cold and streamed admission use the same policy.

Fusion becomes available only after successful native program registration.
GLES and missing/rejected backend registration retain original geometry. It is
enabled by default for native Vulkan; no save or asset reset is needed.

## Validation and measured scope

- Real Vulkan software-GPU comparison against retained v237 material: indexed
  and non-indexed source, 0–3 fused paints, a fully unsupported four-paint stack,
  mixed retained/fused geometry, varying/zero alpha, real mipmapped checker
  textures, normals, specular/HDR values above one, fog, shadow sampling, local
  lights, reflection clipping and different cameras in both eyes.
- The same comparison runs in ordinary and SpaceWarp colour passes, plus density
  attachment emulation. Colour differences are bounded to 0.004 linear HDR;
  native D32 scene depth and retained SpaceWarp depth match exactly. Emulation
  does not establish real Adreno coarse-fragment behaviour.
- Production factory admission tests cover Vulkan/GLES, stale legacy properties,
  native off switch, missing/rejected registration and Debug/O2 policy. Sliced
  uploads cover both vertex layouts. CPU tests verify exact original contribution
  coverage after selective group restoration, as well as the draw-count bound.
- Original raw Wasteland LAND probe: 18 cells around Springvale and Rivet City,
  decoded through production height/normal/colour decoders and the streamed
  geometry builder. Old admission folded 6,293 base / 7,270 paint triangles;
  selective admission folds 12,952 base / 16,855 paint triangles. Nonempty draws
  are 211 before and 206 after in both policies. Several cells intentionally
  remain unfused. This assumes resolved texture paths are available, excludes
  child-world parent substitutions, and is not a DDS-upload or headset benchmark.

Run the original-data probe without adding game data to Git:

```
python tools/performance/check_terrain_fusion.py --esm /path/to/Fallout3.esm
```

No Quest GPU-time or FPS improvement is claimed from host timings. The first
headset check must verify terrain appearance, especially zero-group cells, then
compare the same stationary views. `Q281 TERRAIN NATIVE` identifies registration;
`Q238 TERRAIN FUSION` gives cold/streamed admissions; `FQVK MATERIAL` includes
`fusedLandDraws` alongside total terrain draws.

For a same-build comparison/recovery, restart after setting:

```
adb shell setprop debug.falloutquest.terrain_fusion 0
adb shell am force-stop com.falloutquest.app
```

Use `terrain_fusion 1` and restart to restore native fusion. The older
`terrain_legacy` property remains ignored so stale settings cannot select the
historically broken GLES shader.
