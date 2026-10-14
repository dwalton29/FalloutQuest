# M1B3l — reversible interior load-door resources

Continues canonical main 7f97e3f062609847e972cdf34f32133896410028 (v209).
M1B3k Android CI succeeded (run 38633472657). This checkpoint prepares and
exchanges authored interior XTEL targeting and prompts together. Production
scripts still cannot start the lane or execute Enable/Disable or other world
commands; the outer coordinator remains unfinished.

## Prepared resources

The authored anchor cache and mutable prompt metadata/text caches now reference
one resource bundle. The immutable ESM name index remains separate. Ordinary
first-use anchor builds, metadata writes and prompt insertion mark the bundle's
publication epoch before mutation; read-only cache hits do not. Prompt formatting
uses the existing English prefix and original DOOR/CELL FULL bytes, falling back
to the destination WRLD FULL when the CELL has no name. Both names missing reject.

After the reserved lane drains workers, the render thread captures an owned copy
and its epoch. The CPU worker reads only that copy, immutable before/after
reference projections and the original ESM. It indexes authored references,
DOOR bases and CELL/WRLD ownership, validates complete record/subrecord framing,
finite source/arrival transforms, XTEL layouts and destinations, and resolves
bounded enable-parent graphs including inversion and presentation flags. Missing
parents, cycles, unavailable identities and malformed inputs reject. This strict
candidate reader does not replace ordinary scene-load parsing.

Before/after active load doors must differ only for listed affected references.
Existing attempted anchors and any cached active-door metadata/prompt text must
match the published authored projection. The candidate replaces the current
interior's anchors and door-cache entries while preserving other cells. All work
is local until success; rejection or cancellation leaves caller output unchanged.
A door without a drawable model still gets its authored interaction anchor.
Local swing doors without XTEL do not acquire teleport resources.

The extracted targeting helper preserves source-to-scene conversion, 4 m range,
1.35 m focus radius and the original perpendicular-distance/range score. Original
rendered DOOR bounds retain precedence over this authored fallback. Candidate
anchors use deterministic reference order; legacy unordered-cache order was
unspecified for equal scores. Invalid rays reject before first-use cache loading.

## Ownership and lifecycle

Initial GPU admission and every upload step check the captured door epoch in
addition to existing scene/reference/visual/light/collision stamps. Private
candidate uploads continue to suppress ordinary prompt-cache publication. At GPU
completion, a door plan is prepared before light and visual plans; any failure
discards the prepared resource plans.

The generic RAII exchange owns the whole anchor/metadata/text bundle. Preparation
copies the candidate before allocating the plan so allocation failure preserves
both caller and live resources. Commit and rollback only swap owned values and
advance the epoch, without allocation or ESM access. Rollback restores exact
previous vector buffers and map nodes. Ordinary cache mutations invalidate both
prepared commit and applied rollback admission. Core commit/rollback wrappers
require the owning exclusive asset ticket and no active scene load.

Lane release refuses an owned door plan. Cancellation refuses an applied plan
until the caller rolls back or finalizes it. Scene/context teardown joins the
worker and discards door ownership alongside the visual/light plans. The final
transaction must exclude live interaction/cache writes across its exchange turn;
these resource checks alone do not freeze the game or grant activation permission.

## Validation and remaining work

- Twelve world, 29 player and one physics CTest cases pass (42 core host cases).
- Door tests cover original prompt bytes and WRLD fallback, model-free ray
  selection through exchange/rollback, enable-parent projection, invalid graphs,
  missing/non-door destinations, malformed/nonfinite fields and framing,
  undeclared deltas, cancellation, cache mismatch and stale epochs.
- Allocation failure preserves preparation inputs. Commit/rollback succeed with
  allocations disabled and restore exact original anchor storage/prompt nodes.
- Original Fallout3.esm: CELL resolved by EDID MegatonTheBrassLantern prepares
  one authored load door to zero and restores one, including its prompt.
- Synthetic and original door cases pass ASan/UBSan with LeakSanitizer disabled
  in the traced runtime. Source guards verify cache aliases/epochs, immutable
  worker capture, GPU handoff and lifecycle guards. Existing body/Pip-Boy/
  inventory/skin/opaque/render-dispatch checks pass. Native Android validation
  runs in CI; no physical Quest or performance claim is made.

This is interior XTEL-resource ownership. Local swing-door poses, active
interaction/loot/lock UI, actor residency, held/loose objects, navigation and
unsupported baked LOD need their own preflight/ownership. Exterior light and
interaction scope remain separate. The final coordinator must revalidate
script/Player/resource stamps, exclude simulation/saves/effects, publish canonical
state and every resource in one render turn, roll back failures and reconcile
parked CELL/NPC/terrain/LOD work. v209 and existing render-distance/body/Pip-Boy/
weapon/NPC behavior remain unchanged.
