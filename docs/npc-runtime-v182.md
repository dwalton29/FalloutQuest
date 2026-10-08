# v182 — Original furniture, Sleep and verified seated Eat

Version 182 / `0.48.0-npc-furniture-sleep-eat`.
Base: v181 `24fdb42f10fb5f7b74e507146c187a687979d355`.
Furniture foundations checkpoint: `3aa6ee339785f79527028e9b637ef741f4bdc22f`.
Sleep checkpoint: `f5574979d2ef3d3bc4d003c2584a007367dde986`.
Implementation history is in [npc-furniture-checkpoints.md](npc-furniture-checkpoints.md).

## Verified data and runtime

FURN MODL/MNAM, REFR DATA/XSCL/XOWN/enable-parent fields and original NIF
BSFurnitureMarker FRN are decoded. FRN contains a name index, count and
16-byte XYZ/ushort clockwise-milliradian orientation/two position-reference
bytes. Original BedTwin01, BedQueen01, NavalCot01, Chair01/02/03, Stool01 and
OfficeChairRuined01 were available to the audit; only verified marker-reference
programs are enabled. MNAM low bits address marker slots. BedTwin01L's enabled
second slot has position reference 2 and selects Bedright through original
GetFurnitureMarkerID, regardless of the misleading base-record name.

World alignment composes the original marker with Bethesda's inverse
Rz*Ry*Rx placement convention, matching existing geometry conversion. Current
runtime admits level, unscaled furniture only. NPC ownership or original
faction membership is checked; unresolved enable-parent references are excluded.
Alternative entrances to one furniture reference share one lease. Separate
furniture objects remain independent. Multi-seat objects are conservatively
limited to one actor per reference until original occupancy grouping is decoded.

Sleep type 4 and seated Eat type 3 use the normal schedule/condition selector.
They resolve resident direct-reference targets, supported local CELL searches
or furniture within the authored radius. They reserve, approach on NAVM,
verify the projected arrival, orient and play original entry. Loop ownership
lasts until schedule/condition change or interruption. Matching exit returns
the actor to ordinary navigation; the authored exit translation and yaw are
baked once. Fresh cycles reanchor to FRN.

Furniture Sample has a separate root policy, retaining Bip01 accumulation
through entry, loop and exit. DynamicIdle_Sleep has no accumulated translation,
so the entry result is carried into it. Ordinary locomotion still strips
accumulation. Furniture poses bypass locomotion blending/look rotation. Original
clip envelopes extend actor bounds for furniture poses without changing world
rendering. Exit invalidates the ordinary animation binding, preventing the
furniture track mapping/root from leaking into walking.

Moira: Sleep PACK 00004156 → REFR 00003D75 → FURN 00015838.
Eat PACK 00004155 → REFR 00015882 → FURN 00015840.
Billy: Sleep 00003FF9 → REFR 00003CBF; Eat 00003FEE → REFR 00003FE5.
Both residents pass their normal original schedule selection, NAVM approach,
entry/loop/exit and three repeat cycles without root drift.

Dialogue requests exit and may speak during it. Combat movement/fire waits
for exit; an interruption during entry completes entry before exiting. Death,
loading/save-blocked suspension and cell destruction release reservations.
Occupancy is not persisted; saved roots remain marker anchors and restored
actors must reacquire. Existing save schema remains unchanged. Missing targets,
contention and route failures yield to the existing per-package retry policy.
Eight game units of arrival tolerance and a 60-second approach timeout are
Quest navigation adaptations; no positional animation offsets are added.

## Eat boundary

Implemented Eat behavior is **verified seated furniture execution**, including
original chair entry, DynamicIdle_ChairSit, schedule ownership and exit. It is
not complete food consumption. Original SitChairEatA's IDLE condition 246 tests
used-item membership in original MeatFood FLST 00029D9C. The engine's choice of
used item, inventory consumption timing, animation-object props and drink/food
variant selection have not been established. The runtime does not invent those
operations or force a gesture whose original condition is unresolved. Standing
Eat locations and unsupported furniture/activity families remain rejected.

## Original assignment coverage

Before: 0/36 Sleep, 0/33 Eat executable by this furniture runtime.
After: **26/36 Sleep** and **8/33 seated Eat** assignments complete original-data
entry/loop/exit tests in the actor's currently resident scene. The eight Eat
entries are not claims of food-consumption support. Coverage counts assigned
entries, including repeated assignments; it does not count one example as
support for all packages.

Every relevant assigned entry is evaluated independently using its original
schedule and condition state, then routed and exercised when eligible. All
37 original placements (3 exterior, 34 interior) additionally run concurrently
within their authored CELL populations at 12:00, 20:00 and 01:00 for 80 seconds
per schedule. Ten Common House actors contend for available beds; four can
occupy distinct supported bed references without sharing them.

The 35 remaining unverified assignments in that test state comprise:

* 27 without a supported resident target (including cross-cell destinations,
  child programs, floor-bed/activity marker families and ownership restrictions).
* 7 with false or unsupported conditions in the tested quest/local-variable
  state. Mister Burke Sleep reports unsupported CTDA function 0. These counts
  are not proof that a condition can never become true in another game state.
* 1 unsupported weekday/date schedule (Mister Burke's Tenpenny package).

Explicit unsupported Sleep entries include Lucas's home bed while exterior,
Stockholm's floor-bed family, Harden and Maggie's child programs, Mother Maya's
unsupported furniture target, Gob/Moriarty/Nova's tested local condition state,
and Mister Burke's condition/weekday entries. Cross-cell scheduling/travel,
child/floor-bed IDLE branches, tilted/scaled furniture and enable-parent
resolution were not substituted or expanded in this milestone.

## Validation and acceptance

Host assertions cover decoding/malformed input, raw placement transforms,
reservation conflict through alternative entrances, independent objects,
entry accumulation/loop retention/exit continuity, repeated cycles, normal
package schedule selection, package changes, dialogue/combat/death/loading
cleanup, and independent residents. Existing Travel/Wander/Sandbox/Patrol/
Follow/Escort/Flee, grounding, dialogue reentry, combat, weapons, player saves
and facial tests remain part of regression validation.

Run original data explicitly (assets are not committed):

```
build/host-nif/furniture_asset_tests /path/to/original/meshes
build/host-player/npc_furniture_runtime_tests /path/to/Fallout3.esm /path/to/original/meshes
```

Android build status and the APK are reported with delivery. No Quest headset
test was performed. Headset acceptance still needs visual entry/exit alignment,
root/bounds behavior, sleep pose/body attachment, physical door approach,
dialogue during exit, combat interruption, death pose transitions and resumed
walking. Full Eat consumption/props remain an explicit incomplete part of the
original Eat procedure; this is not a claim of full Fallout 3 daily-life AI.
