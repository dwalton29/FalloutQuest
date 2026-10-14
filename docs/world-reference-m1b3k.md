# M1B3k — reversible interior light resources

Continues canonical main 98ad9cbd3e4c0ece6188e2108f3e76a391772f65 (v209).
M1B3j Android CI succeeded (run 38631613450). This adds interior point-light
preparation and reversible ownership to the resident resource packet. No
production script starts the lane or admits Enable/Disable or other world commands.

## Preparation and supported boundary

The reserved worker loads published and candidate interior light snapshots from
the original ESM and immutable reference projections, using the fixed scene origin.
Changed light identities must be included in the complete affected-reference list.
Lights present in both projections must retain their original base, transform,
colour, radius, fade, flags and other metadata. CELL lighting/template/image-space
identity and payload must remain identical; root-enable changes do not rewrite
the room environment. A valid dark or empty interior remains a supported result.

Candidate loading uses an explicit strict mode. It checks record/subrecord framing,
recognized light and transform layouts, source/base/image-space availability,
finite renderer inputs, resolved parents, supported point-light semantics and
complete reference reads. Spotlight, animated/time-limited/carryable light modes,
Behave Like Exterior cells, malformed data and unsupported lighting templates
reject instead of silently substituting a light. Ordinary scene loading retains
its existing parser defaults. Strict mode indexes non-light record types too, so
an ordinary STAT reference is distinguishable from an unresolved base identity.
The wrapper builds into local snapshots and leaves its caller output unchanged
on failure or cancellation. This is not support for every Fallout lighting mode.

The worker also builds the renderer's placed-light representation from the same
candidate snapshot. It captures a render-thread light-publication revision before
starting. Initial GPU admission and every GPU step require that same revision.
At final GPU handoff, preparation checks the actual live snapshot against the
published projection and checks its coupled placed-light list before owning a
reversible light plan. A visual-plan failure discards that prepared light plan.

## Coupled exchange and cache lifetime

ResourceExchange is a tested RAII-only bundle owner. It allocates during prepare,
then swaps all prebuilt values during commit/rollback without allocation. Owner,
publication identity/revision, exhaustion, duplicate owners and incoming/live
aliases are guarded. A prepared plan owns candidate values; an applied plan owns
displaced values. Dropping the plan finalizes whichever state is already live.
It exposes read-only commit/rollback admission checks for the future coordinator.

The interior light plan couples the full light snapshot and its placed-light
vector. Commit and rollback require the reserved asset ticket and increment the
light publication revision. They clear transient selected-light uniforms without
reloading CELL fog, ambient/directional light, image space or exposure adaptation.
The existing placed-light reset/load and scene publication paths mark this epoch
before structural mutations, invalidating stale prepared plans.

Static objects cache light indices across frames. Their cache now also checks
the light revision, so a light exchange or rollback cannot reuse indices into an
old vector. Existing body, weapon and actor light selection observes the same
revision. Cached water reflections also include the light revision. These are
resource-coherence changes, not a new rendering or performance policy.

Scheduler release refuses an owned light plan. Cancellation refuses an already
applied light exchange until the outer transaction rolls back or finalizes it.
Scene/context teardown destroys light plans alongside the existing private GPU
and visual plans before their surrounding context changes.

## Validation

- Eleven world, 29 player and one physics CTest cases pass (41 core host cases).
- Coupled-resource tests cover allocation failure before transfer, no-allocation
  commit/rollback, exact old storage/node restoration, finalization/abandonment,
  stale/wrong publication or owners, duplicate/aliased resources and exhaustion.
- Interior tests cover changed parent/child lights, negative-light preservation,
  original transforms and placed-light parity, undeclared/metadata/environment
  deltas, unknown parents, unsupported spots, malformed DATA/FNAM/XCLL/subrecords,
  missing image-space/CELL sources, cancellation and intentionally dark/empty
  rooms. The real point-light selector follows committed and restored vectors.
- Original Fallout3.esm: Brass Lantern and Player House each prepare an authored
  11-to-10 light projection using the original light reference, with unchanged
  CELL/image-space data. Existing SuperDuperMart template/image-space checks pass.
- Interior/resource/lane/GPU tests pass ASan/UBSan with LeakSanitizer disabled in
  the traced runtime; original ESM interior checks pass under the sanitizers too.
  Source guards cover immutable worker capture, handoff/failure cleanup, lifecycle
  guards and light/reflection cache epochs. Existing body/Pip-Boy/inventory/skin/
  opaque/dispatch source regressions pass.
- The local post-shader compile subtest could not run because glslangValidator is
  absent from this runtime; its source check passes. CI installs that compiler
  and checks the shaders and native Android build. No shader source was changed.
  No physical Quest/performance claim is made. v209 is unchanged.

## Remaining coordinator

Exterior placed lights need their own candidate selection/publication semantics.
This checkpoint does not admit exterior light changes. Door prompts/interactions,
actor residency, held/loose objects, navigation and unsupported baked-LOD effects
still need preflight/resource ownership. The final coordinator must revalidate
script/Player/resource stamps, exclude simulation/saves/effects, exchange canonical
state plus every resource in one render turn, roll back on failure, and reconcile
parked CELL/NPC/terrain/LOD work after publication. No CELL reload substitutes for
the resident transaction, and no success-only world-command handler is introduced.
