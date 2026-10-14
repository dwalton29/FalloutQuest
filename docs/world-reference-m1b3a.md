# M1B3a — scene publication identity and stale-work admission

Continues canonical b782e6e7 on main (v209). This is the first M1B3
subcheckpoint; it does not complete live Enable/Disable publication.

## Implemented

A render-thread ReferencePublication owns the immutable projection of the
successfully published scene. Its stamp includes a monotonically changing
scene identity and the reference-state revision. Repeated loads of the same
CELL at identical origins and with the same revision receive distinct identities.
The cached live-player projection remains separate from the published projection.

Full-scene loading checks its prepared reference revision every frame before
GPU uploads and collision/visual publication. It aborts if live state changes.
Only the successful scene/collision swap installs a new published identity.

Exterior CELL tasks capture the published projection instead of freshly edited
player state. CELL metadata, resumable GPU staging and collision snapshot tasks
carry the same identity. Worker consumption, metadata reuse, collision-window
assembly, GPU slices, visual commits and collision publication check admission.
A new published scene invalidates the CELL context even at identical origins.
CELL reset explicitly retires staged GL resources before clearing metadata.

If reference state changes without coordinated scene publication, streaming
stops admitting work and keeps the old scene resources. Completed stale tasks,
partial texture/VBO uploads and collision tokens are reclaimed. CELLs are
requeued with upload cursors reset, so a transaction rollback cannot resume
half-discarded uploads or leave an orphaned Worker stage. This is an explicit
safety barrier, not a live mutation refresh. It never labels the old visible
window ready for a new reference revision.

NPC arrivals also capture the publication stamp. A stale arrival waits for its
CPU worker to finish without blocking the render frame, retires partial GPU
parts before registration, and requeues actor IDs for reconsideration. No actor
enable/disable lifecycle is claimed by this guard.

## Validation

- Six world tests pass after rebuilding, including publication tests for a slow
  worker retaining its old immutable projection, live-revision mismatch, rollback,
  same-CELL/same-revision reload and rejection after a revision returns to an
  earlier numeric value in a different scene.
- All 27 existing player cases pass. No player/save/source-compiler code changes.
- Physics, NIF, asset and audio suites: 16 passed, two original-asset-dependent
  NIF cases skipped (vr_body_asset_tests and npc_combat_asset_tests).
- Combined host result: 49 passed, two skipped, zero failed. Earlier reports
  counted all 51 CTest registrations as passing; the two skips are explicit here.
- Scene collision source checks verify reference guards precede uploads/swaps,
  resource retirement precedes metadata deletion, and NPC arrival guards precede
  part uploads/registration. Render-dispatch checks pass.
- Original master regressions pass: 5,502 enable-parent children, bomb activation/
  cancel/disarm/Karma/notices/save, Lucas/MS11 dialogue acceptance/save, original
  detonator model/pose/collision-input selection, and 1,118 XTEL links.
- Native runtime translation unit is not compiled by the host world suite.
  Android CI and physical Quest validation remain required.

## Remaining M1B3 work

This checkpoint deliberately admits no Enable/Disable source handlers. A direct
StageReferenceEnable on an already loaded scene now halts streaming until that
state is rolled back or a matching scene is published. A supported live mutation
must first prepare resources and coordinate one outer-transaction publication
across visuals, interactions, reversible collision/navigation, held/loose objects
and actors. The normal transition loader changes context/collision policy during
preparation; do not present it as a transaction-safe in-place refresh.

Next: implement the resident mutation coordinator and reversible collision state
with failure/preview/rollback tests, then actor and held-object coordination. Only
admit source commands after the complete effect can be published safely. Version
remains 209; no new APK capability or performance result is claimed.
