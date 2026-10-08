# v188 — Automatic original ACHR actor registration

Baseline: v187 `301ac9f89d299559293aefd920c07a84ee8527f1`.
No separate branch, no generated NPC assets or fictional behaviours.

## What is registered

During the **existing** original Fallout3.esm catalogue scan, the player
catalogue preserves a lightweight, immutable source census of placed ACHR
records: original reference/base FormIDs, authored CELL and WRLD group
ownership, reference flags, XESP enable parent, raw DATA position/orientation
and scale. This is *not* a second full ESM scan.

The Pip-Boy dialogue/target finalizer originally dropped unreferenced ACHR
placements. v188 retains eligible placed actor identities in `pipboy.targets`
so original records are resolvable by ActorHealth, UpdateActor, scripts,
save/restore and v187 unloaded package selection even before any CELL loads.

At initial player-session creation, `Player::RegisterOriginalActors()`
iterates the authored census in sorted FormID order. It creates
**canonical ActorState** entries for supported, live, placed NPC_ bases using
only their real CELL/worldspace, XYZ and authored rotation; package selection
remains deferred to the existing generic runtime. Registration is idempotent:
existing saved/moved actors, dead actors, package state and their pose are
never overwritten. The current v9 save schema is unchanged. Maximum tracked
actors remains the existing 10,000; overflow is counted and not silently
invented. The initial registration happens on the scene preparation worker,
not during rendering or each game frame.

**Fail-closed exclusions**: initially disabled or deleted references,
unresolved XESP enable parents, missing/malformed/nonfinite original DATA,
invalid scale, missing CELL+WRLD ownership, unresolved NPC_ statistics,
leveled/noninvariant actor bases and actors with nonpositive or unavailable
calculated health. The code does not invent leveled list choices or quest
enable-parent semantics.

The previously implemented v187 scheduler now sees registered actors before
the player has visited their original cells. It may advance supported
location-only package intent through authored accessible XTEL links; the
limitations on scripts, target types, time/calendar and off-screen NAVM from
v187 remain in force.

## Scene-residency safety

Registration is **not visual instantiation**. The ESM actor assembly and
GPU preparation still run only for the requested scene. The v186 relocation
handoff now gates exterior as well as interior saved actors by their exact
resident CELL, not by the entire Capital Wasteland WRLD. Actors moved elsewhere
cannot respawn at their original base placement. Unvisited actors registered
in their current CELL can appear only when the resident loader selects that
CELL; no global outdoor actor mesh population is created.

The local ESM actor assembler now also excludes original XESP-controlled
placements until a genuine enabling system exists, instead of ignoring their
authored initial condition.

## Validation

`player_state_tests` includes an eight-placement census
fixture exercising deterministic original positions, interior/exterior,
disabled and enable-parent exclusion, unknown base, nonfinite/missing
location, preexisting transferred state, idempotence and existing v9
save/restore. Optional original-master validation:

```sh
cmake -S tests/player -B build/host-player
cmake --build build/host-player --target player_state_tests
./build/host-player/player_state_tests /path/to/Fallout3.esm
```

Original-data run reports authored/registered/conditional/unsupported counts
rather than claiming all Fallout 3 actor variants are supported. CI host tests
and Android APK build do not constitute Quest headset verification.

## Remaining to full NPC registration parity

- Quest/script-controlled XESP dynamic enable/disable.
- Non-invariant LVLN/NPC_ actor choice and respawning actor populations.
- Full all-world outdoor scene streaming/activation across exterior cells.
- Alternate plugin/modded master references and DLC.
- In-game headset Megaton day/night demonstration, ensuring NPCs do not
  duplicate after player cell transitions.

No unrelated rendering, lighting, weapon, Pip-Boy, body or optimisation
changes are part of this checkpoint.
