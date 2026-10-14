# M1B3j — private candidate collision resources and policy exchange

Continues canonical main b57842aeb7e39c34f182f6debc3e44efd6146196 (v209).
M1B3i Android CI succeeded (run 38629731659). This adds collision resources to
the reserved preparation packet; the outer transaction remains unfinished and
no production script starts this lane or admits world commands.

## Private preparation

PrepareFo3ResidentCollisionSnapshot takes candidate collision placements, an
explicit transform origin, a captured collision revision/collected-reference set/
door-pose map/model policy, and a cancellation flag. It constructs a candidate
model policy, deduplicates decoded models, and builds transformed chunks locally.
It never primes, clears or consults ordinary transformed/negative/model caches,
and never temporarily changes the live collision policy. The existing triangle
transform/filter body is shared with ordinary priming without changing ordinary
callers' permissive defaults.

The candidate decoder rejects partial supported collision-object/shape chains,
malformed transforms or metadata, missing named static assets, invalid placements,
duplicate reference IDs, safety caps and cancellation. A successfully parsed NIF
with no authored collision objects is a valid empty resource; a missing NIF is
not. Mixed supported/unsupported list children reject the complete candidate even
when the ordinary decoder can retain a supported child. Complete-mode output is
atomic. This validates the currently supported collision formats, not arbitrary
Havok objects, constraints, convex semantics or every NIF variant. Ambiguous
packed subshape coverage and invalid solid triangles reject rather than inventing
collision. Original non-solid layers and dynamic-only model exclusions remain
intentional filters.

The existing mutation snapshot builder accepts private chunks/policy, bypassing
its ordinary prewarmed-cache requirement. It prepares triangles, source identity,
collected/door filtering and the existing exterior weld/shape/query structures
without exchanging the live world. Empty candidates remain valid. Preparation
may allocate, decode and take substantial time; no new latency or memory bound
is claimed. It requires the reserved serialized asset lane.

## Token and policy lifetime

Fo3CollisionSnapshotOwner is a move-only owner for the registry token. Cancellation,
failed worker results and abandoned CPU/GPU packets discard their prepared token.
Ownership follows the packet into GPU staging and back to the outer caller when
the visual ownership plan is ready. Destruction never publishes a collision world.
Completed ordinary collision tokens may remain parked independently in the
registry; private preparation does not have to discard them to obtain a token.
The final coordinator must still invalidate/reconcile those old queued results.

Candidate tokens retain both policy identities and a policy payload. The existing
allocation-free collision exchange also swaps that payload with the live policy.
Rollback restores the exact old policy and world buffers. Read-only
CanExchangeFo3CollisionMutationSnapshot checks collision revision, transform and
exterior context, collected references, door poses and the appropriate expected
policy. Any mismatch rejects before the first swap. The same check guards initial
GPU admission and every GPU step, so door/collection/collision changes during a
long preparation make its GPU packet stale. Legacy publication cannot consume a
mutation token.

StartFo3ResidentCpuPreparation now captures collision inputs on the render thread
and prepares the owned collision token after strict visual/texture decode in the
same worker reservation. Exterior visual and collision selection scopes are
explicitly separate: a visual residency radius must not silently expand or shrink
the current collision window. Both selections use the immutable candidate
reference projection. Interiors use the full candidate CELL collision placements.
The future outer caller remains responsible for capturing the actual live scope.

## Validation

- Ten world, 29 player and one physics CTest cases pass (40 core host cases).
- The real physics harness covers policy changes, forward/rollback/reapply with
  allocations forbidden, exact old buffer restoration, poisoned ordinary caches,
  missing/partial models, authored collision absence, empty inputs, normalized
  model deduplication, invalid/duplicate transforms, cancellation before/during
  asset reads, allocation failure, exterior caps, door poses, collected changes,
  read-only stale checks and move-only token cleanup. A reserved worker finishing
  after cancellation releases its token without publishing anything.
- Original Common House manifest: 335 placements; the supplied mesh archive has
  96 matching model sources and lacks 239. The full named request rejects with
  no token. Preparing the available authored subset retains all 1,715 original
  collision triangles and passes allocation-free exchange/rollback. This does
  not claim a complete 335-reference asset set or authorize omitting missing
  models in production. Proprietary source files are not modified or committed.
- Synthetic and original physics cases pass ASan/UBSan with LeakSanitizer disabled
  in the traced runtime. Source guards check private cache/policy isolation,
  render-thread capture, explicit collision scope, packet ownership and pre-GPU
  collision validation. Existing body/Pip-Boy/inventory/skin/opaque/dispatch
  checks pass. Android compilation is checked separately in CI.

The outer logical/resource publication still needs lighting, interactions and door
prompts, actor residency, held/loose objects, navigation and baked-LOD admission.
It must revalidate script/Player/resource stamps, exclude simulation/saves/effects,
exchange resources and canonical state in one render turn, and roll back every
exchange on failure. Post-publication CELL/NPC/terrain/LOD queue reconciliation
also remains. No source Enable/Disable, MoveTo, spawning or deletion is admitted.
Save schema and app version remain v209; no Quest performance claim is made.
