# M1B3m — read-only resident interaction preflight

Continues latest canonical main 5a4ad709d43d36ec756aaa48bae441e45f400db7,
preserving v212 (0.74.1-moira-barter), barter input handling, original Moira
dialogue conditions and MS11 completion/rewards. M1B3l and v212 Android CI passed
(runs 38635385905 and 38647849567). No gameplay caller is added for the general
resident lane, and the separate v211 nonresident actor-removal path is unchanged.

## Admission, not forced cleanup

The resident worker previously had resource-epoch checks but no concrete check
for active UI or affected physical/actor state. A candidate must not become
publishable simply because its meshes, collision, lights and XTEL caches decode.
This checkpoint adds read-only render-thread admission at the real lane/GPU/
resource-commit boundaries.

Admission requires a ready interior scene and available, save-unblocked Player.
It rejects active dialogue, barter, Pip-Boy, script messages, lockpick focus or
its release latch, loot panels, physical weapon handling/reloading and pending
shoulder gestures. Barter is checked independently of dialogue. No probe polls
queued effects, changes container interaction state, closes a menu, stops audio,
releases a hand or creates a lazy physics/door/actor entry.

Every affected reference must have a valid, non-deleted catalog identity. The
current bounded policy rejects affected authored or live actors, held references,
loose/pickup objects, collected references, cached/live local swing doors and
furniture. Original actor placements and base inventories catch incoming or
unloaded actors before they have resident visuals. Item/furniture catalogs catch
incoming supported types before lazy runtime state exists. Private staged GPU
parts are checked too, preventing newly enabled local swing doors or loose
objects from bypassing a live-registry check. Unrelated physical registry entries
alone do not reject an otherwise idle static-reference scope.

These are conservative exclusions, not new support for those resource types.
The probes are not a complete authored-type/LOD/navigation capability classifier.
Exterior interaction scope is intentionally not admitted by this preflight yet.
An empty or invalid affected list rejects; a future no-op transaction should
bypass resource preparation instead of reserving and rebuilding a scene.

## Owned scope and late checks

The CPU packet owns the complete affected-reference list. The worker only copies
that immutable list; it never probes menus, Player or live registries. Admission
is checked before worker capture and before initial GPU admission/every GPU step.
After private GPU staging, a ticketed admission object owns a separate copy of
the list before door/light/visual plans are prepared.

Visual, light and door commit wrappers recheck that ticket, current UI and every
affected runtime identity. A late menu or body/actor/door hazard blocks commit
without moving live resources. Checks and commit gates allocate nothing. A menu
that appears during an already partly applied outer transaction must not prevent
recovery: rollback wrappers deliberately do not require interaction-idle status.
Their existing resource/ticket checks remain intact.

Failed GPU handoff destroys prepared admission with its other private plans.
Cancellation clears it only for the owning ticket and cannot discard applied
resources. Release clears it only after the lane accepts release and all resource
plans are gone. Scene/context teardown joins the worker and clears admission
before surrounding caches change. A pending admission also prevents another
candidate GPU stage from starting.

This object owns reference IDs only. It is not a UI rollback snapshot, a Player
revision/lifetime certificate, a simulation freeze or a replacement for final
canonical/script/resource stamp checks. The outer coordinator still has to
exclude simulation, saves and effects across its exchange/rollback turn.

## Validation and next boundary

- Thirteen world, 31 player and one physics CTest cases pass (45 core host cases).
- Pure tests cover all busy flags, malformed/empty scopes, every unsupported
  reference category, wrong tickets, owned-scope independence and late hazards.
  Both owner-allocation and affected-list-copy failures preserve inputs. Admission
  checks run with allocations disabled. A late modal menu prevents a real generic
  resource exchange; rollback restores exact old storage even while busy.
- The real runtime probe is included in interaction host tests with real Player
  state. Tests exercise incoming actors/items/furniture, held/collected/body/door
  registries, UI/weapon/shoulder focus, late barter/body changes and unchanged
  Player revision and transient ownership.
- Pure admission/resource/lane/GPU/door tests pass ASan/UBSan with LeakSanitizer
  disabled in the traced runtime. Original Brass Lantern door regression remains
  valid; original barter and MS11 completion/reward/save-reload regressions pass.
  Source guards cover capture isolation, actual lane/GPU/commit ordering,
  staged-part admission, release/cancel ownership and rollback availability.
  Existing body/Pip-Boy/inventory/skin/opaque/render-dispatch checks pass.

Native Android validation runs in CI; no physical Quest or performance claim is
made. Actor/held-object/local-door/UI ownership, navigation and unsupported baked
LOD effects, exterior resources, final canonical/resource exchange and parked
queue reconciliation remain unfinished. General resident Enable/Disable is still
not admitted. The newer v211 nonresident actor Disable policy is preserved.
