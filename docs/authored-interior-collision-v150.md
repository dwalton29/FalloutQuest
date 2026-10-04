# Authored interior collision — Quest v150

Baseline: latest `main` fetched twice before publication, `4962440d5f9f505ec521ebb383b60cb80790efea` (v149). No experimental branch code was used.

## Root cause

The generic CELL loader already resolves active REFRs, MODL paths, authored transforms and scales. `InitializeFo3CollisionOverlay` then discarded non-exterior models unless the path contained `architecture\megaton`. This is the primary failure.

Super-Duper Mart (`SuperDuperMart`, CELL `00017F37`, resolved by EDID in the supplied Fallout3.esm) contains **1,756 REFRs** examined by the production loader, **1,755 enabled REFRs**, and **1,690 active model placements**. Only 46 placements pass the old Megaton path check: five ShackPaperDebris models, all without BHK blocks. All supermarket structural architecture was rejected. Thus the old selection produces no authored player collision for this CELL.

Secondary issues:

- The Q7.10 dynamic-only model policy was configured for worldspaces, but cleared for interior transitions.
- CPU collision candidates came from successful visible mesh parts, losing unsupported visual NIFs and expanding one REFR into repeated candidates.
- The synchronous interior path applied packed non-solid Havok layer filtering only in exterior mode.
- Cached model results could bypass classification, and policy exclusions could be cached as permanently missing collision assets.
- A historical limit of 256 successful interior collision placements could truncate valid CELL geometry.
- Transformed REFR cache invalidation used only the render origin, insufficient for replacing a CELL with the same origin.

## Implementation

Interior collision candidates now come from original active CELL placements, before visual filtering or mesh expansion. The shared `SelectFo3AuthoredCollisionPlacements` deduplicates nonzero `refFormId` identity and preserves the original NIF and placement transform. Exterior visual selection and rolling streaming are unchanged.

The existing Q7.10 classifier is configured for interiors and exteriors. Eligibility is checked before positive/negative cache lookup and before assembly. Dynamic-only models are neither welded into the static world nor recorded as missing assets. The conservative model-level rule remains: a model also used by a static record remains eligible.

The Megaton path restriction is removed. Geometry still comes from `LoadFo3NifCollisionShapesQ6F`, with original NIF BHK shapes, packed subshape metadata, welding identity and `Fo3HavokLayerBlocksPlayerQ714`. Packed layer metadata takes precedence; non-packed shapes retain their rigid-body layer. No render-mesh collision, room boxes or gameplay walls were introduced. The standalone packed parser in `fo3-nif-collision-parser.cpp` was inspected; the active player collision shape parser is the inline implementation in `fo3-nif-collision.h`.

Interiors reuse the existing scene collision preparation worker and immutable snapshot publication. Explicit CELL replacements clear transformed and negative REFR chunks after background work drains; immutable NIF geometry caches remain valid. The old world remains queryable during preparation. The pending snapshot publishes in the same render-thread turn as the visual scene swap, before transition completion. Failed/discarded background results leave the old collision world intact. Snapshot mode sets `gExteriorAllBhksQ78A` correctly; interior snapshots retain the legacy interior resolver rather than exterior manifold/step rules. Collected exclusions and loose-object contact grids are reapplied on publication.

No player response functions, exterior draw distance, lighting, NPC visuals, fonts, door or pickup implementation were rewritten.

## Authored-data verification

Production CELL placement loader + active parser/transform/assembly/publication runtime on host, using original user ESM/NIF data. These are host measurements, not headset logs.

| Metric | Super-Duper Mart `00017F37` | Megaton player house `000151E3` |
|---|---:|---:|
| REFRs examined | 1,756 | 928 |
| Enabled REFRs | 1,755 | 159 |
| Active model placements | 1,690 | 147 |
| Static eligible placements | 1,217 | 133 |
| Dynamic-only placements excluded | 473 | 14 |
| Authored BHK placements decoded | 951 | 73 |
| Static candidates without BHK | 266 | 60 |
| Published collision triangles | 33,074 | 2,515 |
| Non-solid triangles filtered | 0 | 0 |
| Capped | 0 | 0 |
| Collision ready | 1 | 1 |

All 166 distinct Mart static candidate models were retrieved and inspected. 114 decode authored BHK; the other 52 have no BHK blocks. No important Mart authored collision format was unsupported. Missing-placement totals include repeated placements of those non-colliding models, rather than 266 distinct parser failures.

| Structural model | Shapes | Triangles/model | Shape kind | Havok layer | Filtered triangles |
|---|---:|---:|---|---|---:|
| `Dungeons\Office\Pit\OffPitMid01.NIF` | 1 | 4 | TriangleMesh | 1: static | 0 |
| `Dungeons\Office\Room\OffRmWallONLY01a.NIF` | 1 | 12 | TriangleMesh | 1: static | 0 |
| `Clutter\Grocery\GroceryShelves01.NIF` | 1 | 68 | TriangleMesh | 1: static | 0 |
| `Clutter\Grocery\GroceryShelvesRamp01.NIF` | 1 | 204 | TriangleMesh | 1: static | 0 |

An optimized host cold Mart collision build/publish took approximately 68–87 ms; the unoptimized build took about 1.4–1.5 s. Host timing is not a Quest performance estimate. Interior work therefore uses the existing background loader to preserve loading presentation. No exterior streaming redesign was added.

## Placement ceiling

The supplied ESM contains 421 interior CELLs. 223 have more than 256 active model-bearing placements. Representative larger CELLs:

| CELL EDID | FormID | Active model placements |
|---|---|---:|
| RedRacer01 | `00017F40` | 2,415 |
| ArlingtonLibrary01 | `00017F5B` | 2,386 |
| NtlGuardDepot01 | `0001BC93` | 2,340 |
| FranklinMetro01 | `0001A280` | 2,234 |
| SuperDuperMart | `00017F37` | 1,690 |

Mart alone has 951 successful authored collision placements, directly proving the old successful-placement ceiling unsafe. Interiors now assemble the full finite active CELL placement set, without a first-N placement ceiling. Existing NIF parser bounds and per-placement triangle safeguards remain; safeguard failure is logged and background scene preparation fails while retaining the old scene. Exterior limits remain 1,024 successful placements / 250,000 triangles, with explicit cap diagnostics.

## Tests and build

- 18 CTest cases across assets, world, NIF/NPC, player/inventory, audio and collision passed.
- New production-runtime collision regressions cover non-Megaton/Megaton eligibility, dynamic-only exclusion, mixed static/dynamic model policy, non-blocking layers, collision-only NIFs, original REFR deduplication, >256 placements, ready state, floor grounding, wall contact, interaction occlusion, loose-object floor contact, collected exclusions, interior/exterior publication, interior replacement and discarded-token retention.
- Source integration checks verify collision selection before visual support checks, classifier/cache setup, and publication before visual swap/loading completion.
- Existing render policy, GL-state and asynchronous GPU timer tests passed.
- Shader compilation/linking, interior lighting, font consumer, actor scope, render dispatch, opaque-path and LOD diagnostic checks passed; production GLES skinning transform-feedback parity and CPU actor-skinning tests passed.
- Java install discovery tests passed.
- Local Android arm64 Quest `assembleDebug` succeeded **before main publication**. Manifest verified: version code **150**, version name `0.36.2-authored-interior-collision`. APK signature verified against the existing CI development certificate. No Bethesda assets are committed.

On-device entry/exit, stairs/ramps, ceilings, doors, pickups, lighting and NPC appearance still require the user's Quest check. Wasteland/Megaton streaming code, player response and v149 font implementation are retained; host coverage is not a claim of hardware verification.

## Modified files

- `app/src/main/cpp/physics/fo3-collision-runtime.cpp`
- `app/src/main/cpp/physics/fo3-collision-overlay.h`
- `app/src/main/cpp/physics/fo3-nif-collision.h`
- `app/src/main/cpp/world/fo3-transition.h`
- `app/src/main/cpp/world/fo3-cell-world.cpp`
- `app/src/main/cpp/world/fo3-cell-interior.inc`
- `app/src/main/cpp/core/fo3-runtime.cpp`
- `app/build.gradle`
- `.github/workflows/build-apk.yml`
- `tests/physics/CMakeLists.txt`
- `tests/physics/collision_tests.cpp`
- `tests/physics/test_scene_collision_source.py`
- `tests/physics/mock/android/log.h`
- `tests/physics/mock/GLES3/gl3.h`
- This report.

## Quest log capture (Windows)

```bat
adb logcat -v time FalloutQuest:I "*:S" | findstr /C:"INTERIOR COLLISION" /C:"SCENE COLLISION" /C:"Q7.14 CONTACT" /C:"Q7.4 PLAYER RESET" /C:"Q22.5 CONTACT" /C:"COLLISION SAFETY CAP"
```

Enter the Mart, check floor/walls/counters and loose-item interaction, then leave. Look for `INTERIOR COLLISION READY: cell=00017F37 ... triangles=33074 ... capped=0 ready=1` (collected exclusions can reduce counts), bounded structural model samples, player contact lines and a subsequent exterior scene collision publication. Interior sample logs are limited to eight successful placements per build; no new per-frame logging was added.
