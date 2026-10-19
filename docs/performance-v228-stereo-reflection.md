# v228 — stereo-pass selection and conservative mirror LAND rejection

Parent: v227 `bf27468fe64d1a7c2dd04c77cac84db898f57859` on main.
One commit; no branch. Quest 3S target 72 Hz = 13.889 ms.

## Evidence and baseline
The uploaded v226 capture (session 22401, named megaton-v224(7).csv) contains
869 active unblocked gameplay frames; median app work 22.66 ms, P90 39.98 ms.
The 641 blocked frames and 2,586 loading frames must not be mixed into a
gameplay A/B comparison. Whole-eye GPU timing was unavailable in normal gameplay
and MUST NOT be treated as 0. The v227 HUD/NPC changes landed after this capture,
so that capture is a reference, not an immediately comparable v227 baseline.

## Changes
* Build publication-ordered alpha and additive-environment lists alongside the
  already-sorted opaque submission order. Use one list for both stereo eyes and
  reflection while preserving original unsorted alpha/additive render ordering.
  Invalidation is tied to existing visual revision, pointer, and count guards.
* In planar reflection only, bypass LAND base/painted batches whose precomputed
  scene-space max vertex Y is strictly below the original fragment clip plane
  (with a 0.001-m safety margin and finite-value guards). The authored shader
  passes position directly and discards any fragment below that plane. Do not
  reactivate the invalid Q20.18 general frustum terrain cull.
* Emit `mirrorUnderwaterSkipped` in the existing LAND diagnostic.

## Verification
Run `python3 -m unittest discover -s tests/render -p 'test_stereo_reflection_v228.py'`.
The debug APK still requires a headset comparison of the same Megaton save,
head pose/view, movement, time/weather and scene warm-up:
1. Capture 20+ seconds of active gameplay with the performance HUD and CSV.
2. Compare median and P90 workUs, eye0CpuUs/eye1CpuUs, draws/states, and WATER
   REFLECTION PHASES/WATER OUTSIDE PHASES logs; segregate loading/blocked frames.
3. Check from above-water and near-water vantage points for LAND seams, alpha
   paint, water reflections, culling or additive-environment regressions.
4. Confirm unchanged 1680x1760 per eye, production material/LOD policy, and
   original light/fog/water optics. There is no measured gain until headset run.

No distance cuts, time-slicing, visual approximations, stereo multiview, or
changes to NPC/quest/gameplay scheduling were introduced.
