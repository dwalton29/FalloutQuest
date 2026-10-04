# Authored interior lighting (Quest version 148)

## Audit baseline

Audited canonical main `2478743a6982f0ef0383bcbdc3d320d2e2dd6d04`.
All five reported failure paths were present: WRLD-only placed lights, the NIF
placement loader not publishing non-model LIGH references, transition completion
resetting interiors, the 0.34/0.66 invalid-environment fallback, and unconditional
exterior CELL/environment finalisation with WRLD zero. The interior geometry
loader already understood XESP enable parents, but only within its collected
cell references; the new lighting loader also resolves parents outside the cell.
Additional findings: local selection allocated/sorted per eye; the static SP17
constant override could overwrite a newly supplied environment; post processing
used one captured exterior's film/target/bright-pass constants; the IMGS parser
rejected the supplied rooms' pre-v10 132-byte DNAM.

## Source and binary layouts

Originals used: supplied Fallout3.esm, Fallout3.exe, shaderpackage017.sdp from
Shaders(1).zip, FALLOUT.INI, FalloutPrefs, VeryHigh and RendererInfo. No Bethesda
asset bytes are committed. FO3-specific record layouts additionally cross-checked
against [xEdit's FO3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsFO3.pas).

CELL DATA distinguishes interiors (bit 0), public cells (bit 5), and behave-like-
exterior (bit 7). XCLL is 40 bytes: RGB+unused ambient/directional/fog at 0/4/8;
fog near/far floats at 12/16; signed degree rotations at 20/24; directional fade,
fog clip distance, fog power floats at 28/32/36. LTMP resolves LGTM DATA with the
same layout. LNAM inheritance bits 1/2/4/8/16/32/64/128/256 select ambient,
directional, fog colour, near, far, rotation pair, fade, clip distance, power.
CELL XCIM resolves IMGS directly; no interior WRLD/REGN/WTHR or weather IMAD.

REFR children are scoped to their enclosing type-6 CELL group, including type-8,
9 and 10 subgroups. The parser handles shared compressed/extended-subrecord
conventions. NAME identifies LIGH; DATA supplies position and rotation; XESP
supplies parent and opposite bit. Deleted/initially-disabled refs are inactive
without an enable parent. With a parent, inherited initial state replaces that
local state, matching the existing placement policy. Parents outside the cell
are resolved from the same immutable preparation-time record index. Missing or
cyclic parents fail closed (including opposite parents), with diagnostics.
Script-driven subsequent enable changes are not simulated.

LIGH DATA (32 bytes): time at 0, unsigned radius at 4, RGB at 8, flags at 12,
falloff exponent at 16, FOV at 20, value at 24, weight at 28. FNAM fade follows
colour selection. Off-by-default (0x20) sources are inactive. Negative (0x4)
changes the colour sign; signed light terms enter the sum before the existing
nonnegative output clamp. Radius/fade are not hand-clamped to visual values.
Coordinates use precisely `(x-originX, z-originZ, -(y-originY))/70`, with Y floor
`-1.55`; these are the existing geometry origin/unit conventions, not new light
placement constants. Rotation, flags, falloff and FOV remain in the snapshot.

IMGS has multiple actual FO3 formats: supplied master contains 11 DNAM records
of 132 bytes, 12 of 148, and 25 of 152. Pre-v10 (132) omits HDR Skin Dimmer at 56;
all later fields shift -4. Modern film offsets are saturation 100, contrast
average/value 104/108, brightness 112, tint RGB 116/120/124, tint amount 128.
The v13 152-byte record has cinematic flags at 148; older layouts have no v13
flags. The shared CPU parser handles all three, with identity skin dimmer for
pre-v10. Existing 152-byte exterior correction remains compatible.

## Shader and attenuation evidence

SP17 `SLS2034.pso` has two PSLightPosition registers and three PSLightColor
registers (directional + two points). It takes `(position - fragment)/radius`,
DP3-saturates its squared length and subtracts from 1. `SLS2096.pso` has three
point-position registers and four colours and performs the same arithmetic.
Thus this implemented permutation uses `saturate(1 - distance²/radius²)`, not
`pow(1-distance/radius, LIGH falloff)` and not an invented reciprocal curve.
SLS2034's normal-map specular includes normal alpha, gloss exponent, and the
low-NdotL gate (0.2 threshold, saturate(NdotL+0.5)). Interior points contribute
both diffuse and specular using that structure. Editor colours remain normalized
byte-domain values, consistent with the current recovered raw SP17 colour and
texture staging. Exterior SP17 arithmetic and exterior local attenuation are
unchanged.

The INI's actual section is `[bLightAttenuation]`. Supplied defaults:
quadratic enabled, linear/constant disabled, radius multipliers 1, constant 0,
quadratic value 16, linear value 3, quadratic method 2, linear method 1,
`bOutQuadInLin=0`, flicker movement 8. These are real engine settings, not aliases
for LIGH's falloff exponent. In the supplied executable, settings initialization
at VA 0x541F20 builds the constant/linear/quadratic mask (1/2/4) and validates
method enumerations. The NiPointLight update at 0x72D580 writes constant/linear/
quadratic coefficients at light offsets F0/F4/F8. Method 2 computes
`value/(radius*radiusMultiplier)^2`; method 1 computes `value/radius`; method 0
uses the value directly. bOutQuadInLin switches interior/exterior coefficient
selection. This does **not** establish that reciprocal coefficients should be
multiplied onto SP17's explicitly different attenuation arithmetic. They are
therefore not double-applied. Texture-based AttenuationMap permutations also
exist and are not claimed to be identical to the selected analytical SP17 path.
Executable 0xB67F50..0xB68060 additionally builds a 128² squared-distance
attenuation texture. Exact permutation dispatch/radius constant feeding still
needs an interior PC constant capture; that is an explicit compatibility limit.

PC light counts vary by permutation (including 1/2/3 points and other arrays of
8); PC pass scheduling is not reproduced. Quest keeps eight affecting sources
per object in one pass. Fixed insertion ranks absolute authored contribution at
the nearest AABB point and rejects nonintersecting spheres; no heap allocation,
global vector sort, or per-light geometry pass. More than eight overlapping
sources is a budget deviation, not lossless emulation of PC multipass lighting.
Static selection is cached for the scene. Moving/skinned bounds and selections
are prepared once per stereo frame. Actor bounds conservatively transform bind
AABB corners through the actual palette, then the player root; no mesh reskinning
or pose work was added to per-eye draws. Both eyes share selections and exposure.

## Original room verification

Resolved EDIDs through the master, not guessed runtime IDs.

| Field | MegatonTheBrassLantern | MegatonPlayerHouse |
|---|---|---|
| CELL | 00003A2F | 000151E3 |
| DATA | 0x21 (interior, public) | 0x01 (interior) |
| Ambient RGB | 47,70,69 | 47,70,69 |
| Directional RGB | 2,2,2 | 0,0,0 |
| Fog RGB | 77,62,32 | 77,62,32 |
| Fog near/far/clip (game units) | 100/1500/1500 | 100/1500/1500 |
| Direction rotations XY/Z | 0/0 | 0/0 |
| Directional fade / fog power | 1/1 | 1/1 |
| LTMP / LNAM | 0 / 0x9F | 0 / 0x9F |
| XCIM | 0001507A ShackInterior01 | same |
| LIGH refs / active | 11 / 11 | 11 / 11 |
| Radius range (game units) | 150–256 | 150–384 |
| Fade / flags / falloff / FOV | 1 / 0 / 1 / 90 | same |

Brass Lantern lights use RGB 243,226,156. House lights use that RGB plus two
radius-384 sources with RGB 222,236,179. No spotlight, negative, disabled or
parent-driven lights occur in these two original sets; synthetic tests cover
those conditions.

Shared ShackInterior01 DNAM is 132 bytes: HDR eye speed .3, blur radius 6,
blur passes 4, emissive 1, target/upper luminance 1/1, bright scale/clamp 2.4/.9;
bloom radius 3, interior/exterior alpha .8/.2; saturation .9, contrast average
.14, contrast 1.2, brightness 1.1; tint RGB (176,143,77)/255, tint amount .5.
Interior film, target, upper clamp, adaptation speed, bright clamp and scale are
now fed from this direct XCIM. The .8/.2 values describe the LDR Bloom block,
not an arbitrary multiplier on HDR bloom. Existing HDR final blend's literal
0.5 is from ISHDRBLENDINSHADER(CIN), independent of the LDR alpha.

## Publication, fallback and diagnostics

Interior preparation builds its private CELL, LIGH and IMGS data alongside
Fo3SceneCpuPreparation. It does not mutate live globals. While LSCR covers the
swap, the render thread publishes geometry/actors and the entire lighting state.
Outdoor time-of-day, raw weather staging, sky and SP17 overrides are disabled in
interior mode. Interior-to-interior replaces all cell data; exit clears the
interior snapshot and restores the existing WRLD/exterior CELL loaders. Exposure
history starts fresh at every destination commit. A missing CELL/XCLL is explicit
interior mode with zero ambient/directional, not the outdoor fallback. A valid
CELL with no active sources retains its authored ambient/directional and fog.
Unresolved IMGS is neutral cinematic processing with no added bloom.

`INTERIOR LIGHT READY` reports CELL, EDID, record/layout success, CELL lighting,
template/inheritance, XCIM/IMGS, LIGH totals/active counts, unsupported semantics
and unresolved parents. `INTERIOR LIGHT SHADER` reports actual maximum object
selection count, budget and objects without affecting local lights. No new
per-frame lighting log. On Windows:

```bat
adb logcat -v time FalloutQuest:I *:S | findstr /C:"INTERIOR LIGHT"
```

## Explicit remaining compatibility limits

- Spotlight sources retain flags/rotation/FOV/falloff but are excluded from the
  point permutation: exact FO3 spot constant/cone mapping is not established.
  No invented cone and no silent point-light substitution.
- Flicker/pulse, slow variants, movable/carryable lifetime and scripted light
  state changes are not animated. Their initial authored contribution and flags
  are retained and counted as unsupported when applicable. Dynamic flag by itself
  does not imply an invented animation.
- CELL directional rotation uses the conventional spherical degree-to-vector
  conversion plus the proven scene-axis conversion. Its zero-angle convention
  and Directional Fade engine use still need an interior PC capture. Fade is
  preserved but not guessed as an intensity multiplier. Fog Clip is retained;
  projection clipping remains the existing renderer policy pending exact engine
  usage. Behave-like-exterior cells are diagnosed rather than given weather.
- HDR keeps the recovered 15-tap/256² blur chain. Authored blur radius/pass count
  are parsed/reported but their CPU-to-BlurOffsets/BlurScale dispatch is not yet
  reproduced. Interior threshold/gain use IMGS HDRParam semantics; the exact
  engine CPU feeding should be compared against an interior capture.
- Stereo adaptation uses the existing shared eye-0 RGB history and per-frame
  retention, not independently adapting eyes. Exact TimingData.z feeding and
  original luminance reduction resolution remain a VR compatibility layer.
- Original INI says static/architecture shadows off, actor shadows on; supplied
  VeryHigh/Prefs request six interior actor shadows. Existing runtime does not
  reproduce those local actor shadows. No cube shadows or fake architecture
  shadows have been added; spot-shadow flag is retained and diagnosed.
- The existing 8-bit gamma ramp (fGamma=.76) and exterior presentation are kept.
  Full interior PC permutation/material/negative-light clamp parity needs a
  running-PC constant capture, not additional asset files.

No original asset is missing for the implemented parser/path. For the unresolved
engine dispatch, use the repository's tools/pc-d3d9-logger against the same PC
installation and capture Brass Lantern/Player House; an executable and compiled
shaders alone do not supply the runtime-selected constants/pass sequence.

## Verification

Portable synthetic tests cover XCLL fields and malformed layout, all three IMGS
sizes, enable parents inside/outside the cell, opposite and missing parents,
initially-disabled sources, negative RGB/fade, coordinate conversion, multiple
lights, empty/failed replacement and bounded affecting-light selection. Optional
`interior_lighting_tests /path/to/Fallout3.esm` resolves both original room EDIDs
and asserts their authored snapshots. Production world/skin/post/adaptation/bloom
GLSL compile/link checks run in CI, including GPU skin transform-feedback parity.
Lifecycle checks verify private CPU preparation, scene publication order, exit
replacement, weather/sky exclusion and selection outside per-eye draw functions.
Host world, NIF/actor, asset, player/interaction, audio and renderer suites are run.
A real Quest visual capture is still required to assert fixture illumination and
PC visual parity; host/CI verification is not a headset observation.
