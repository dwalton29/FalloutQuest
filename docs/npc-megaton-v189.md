# v189 — Megaton 24-hour NPC integration audit

Baseline: v188 `854fb9c7442cf9681ba3f6e7e97121c4622dd395`.
The canonical original game input is the user's Fallout3.esm; no asset bytes are committed.

## Audit methodology and honesty boundary

An independent full-master audit of the original Fallout3.esm (276 MB) examined every eligible Megaton ACHR placement, template-resolved PKID package chains, authored PSDT schedules, PLDT placed-reference destinations and directed XTEL topology. It sampled **every hour from 00:00 to 23:00**, retaining original package priority and explicitly recording uncertain CTDA functions, package script/interaction dependencies, missing references and unreachable cell graph destinations.

**This is a static schedule/destination projection, not a runtime or headset simulation of every original NPC.** It cannot establish whether quest predicates are true in any particular save, whether resident NAVM reaches furniture, or whether Bethesda's visual animation completes. Neither the aggregate counts nor per-NPC table must be presented as success rates or full NPC parity.

- **37** original resident placements (3 exterior, 34 interior).
- **888 hourly actor-samples** (=37 × 24).
- **257 schedule-and-location candidates** with no preceding unresolved higher-priority blocker identified by the static audit.
- **459 conditional candidates** where higher-priority CTDA, script or other unsupported semantics might pre-empt a lower package. Their actual runtime selection is unknown.
- **172 blocked or unproven samples**, where no conservative supported destination candidate was established.
- **1,080 directed XTEL links** used in the source-to-target graph after excluding initially-disabled, deleted, enable-parent-controlled or non-DOOR endpoints. This is a static topology count, not physical NPC door usability (locks, scripts, live NAVM and actor permissions still matter).

The reproducible repository entrypoint is `python tools/npc/audit_megaton_day.py /path/to/Fallout3.esm --output megaton-24h.json`. Its JSON contains the original FormIDs, time windows and per-actor reason codes. It uses the original census helper `tools/npc/audit_residents.py` and deliberately treats unknown CTDA as a blocker, never a simulated successful quest state.

## Hour-by-hour coverage by original resident

Columns are counts of hourly samples (not number of successful behaviours). **Clear** means an unobstructed static schedule/destination candidate. **Conditional** means a static fallback candidate with unresolved higher-priority packages. **Unproven** means no supported static candidate.

| NPC | ACHR | Clear | Conditional | Unproven |
| --- | --- | ---: | ---: | ---: |
| Andy Stahl | `00003B55` | 21 | 0 | 3 |
| Billy Creel | `00003B5A` | 24 | 0 | 0 |
| Child of Atom | `0004171F` | 0 | 0 | 24 |
| Child of Atom | `00041720` | 0 | 0 | 24 |
| Colin Moriarty | `00003B3C` | 0 | 24 | 0 |
| Confessor Cromwell | `00003B48` | 4 | 0 | 20 |
| Doc Church | `00003B56` | 0 | 24 | 0 |
| Gob | `00003B3D` | 0 | 24 | 0 |
| Harden Simms | `00003B45` | 0 | 24 | 0 |
| Jenny Stahl | `00003B53` | 8 | 0 | 16 |
| Jericho | `00003B5D` | 0 | 24 | 0 |
| Leo Stahl | `00003B54` | 24 | 0 | 0 |
| Lucas Simms | `00003B46` | 0 | 17 | 7 |
| Lucy West | `00003B5B` | 0 | 8 | 16 |
| Maggie | `00003B5C` | 24 | 0 | 0 |
| Manya | `00003B58` | 2 | 22 | 0 |
| Megaton Settler | `000043C5` | 0 | 24 | 0 |
| Megaton Settler | `000043C7` | 0 | 24 | 0 |
| Megaton Settler | `000043C9` | 0 | 24 | 0 |
| Megaton Settler | `000043CB` | 0 | 24 | 0 |
| Megaton Settler | `00014F1A` | 0 | 24 | 0 |
| Megaton Settler | `000208F7` | 16 | 0 | 8 |
| Megaton Settler | `000208F8` | 0 | 24 | 0 |
| Megaton Settler | `000208F9` | 0 | 24 | 0 |
| Megaton Settler | `000208FA` | 0 | 24 | 0 |
| Megaton Settler | `000208FB` | 0 | 24 | 0 |
| Mercenary | `0001FF18` | 24 | 0 | 0 |
| Mister Burke | `00014F77` | 0 | 0 | 24 |
| Moira Brown | `0002D2BC` | 0 | 24 | 0 |
| Mother Maya | `00003B49` | 18 | 0 | 6 |
| Nathan | `00003B57` | 0 | 24 | 0 |
| Nova | `00003B3F` | 0 | 24 | 0 |
| Patient | `00042866` | 24 | 0 | 0 |
| Patient | `00042867` | 24 | 0 | 0 |
| Patient | `00042868` | 24 | 0 | 0 |
| Stockholm | `0001942F` | 0 | 0 | 24 |
| Walter | `00003B59` | 20 | 4 | 0 |

### Concrete authored schedule cases (not proof of animation execution)

- **Moira Brown `0002D2BC`**: original sleep `00004156` at 00:00–02:00, service Sandbox `00004153` at 08:00–20:00, and Eat `00004155` at 21:00–23:00 are identifiable in Craterside Supply. Higher quest-conditioned packages can pre-empt them; the audit marks these hours *conditional*, not confirmed.
- **Lucas Simms `00003B46`**: home Sleep from 02:00–07:00, home breakfast at 07:00, outdoor observation from 09:00, outside lunch at 13:00, and evening travel are identifiable original packages. His scripted MS11, Patrol, quest conditions and incomplete player proximity semantics remain material blockers.
- **Colin Moriarty `00003B3C`**: authored daytime balcony/computer and evening movements, plus bar Sandbox, are present; condition-dependent quest packages precede these routines.
- **Gob `00003B3D`**: original Tend Bar Sandbox and night sleep/patrol appear, but original quest/local-variable CTDAs and animation-object interactions prevent full verification.
- **Mother Maya `00003B49`**: original home Sleep, Children of Atom activities, dinner in the Brass Lantern and evening travel give a useful comparatively clear static candidate sequence, though real chair, NAVM and door behaviour still require testing.
- **Billy Creel `00003B5A`**: relatively clear daytime/evening package ordering provides a second useful integration test for cross-cell routine changes.
- **Stockholm `0001942F`**: original NPC statistics ultimately depend on non-invariant LVLN choices; v188's fail-closed actor census cannot safely seed him as a normal concrete actor.

## Runtime regression and fix

The v187 unloaded scheduler processed actors in batches of 64, but could start at 08:00 and process the remaining actors after the game clock had jumped to 20:00, selecting different packages within one batch. v189 freezes `batchHour_` for each full batch, then starts a separate evaluation for the new time; its transfer suppression now keys off the batch minute rather than the current frame's minute.

The existing v187 suite is extended with:

1. **37 independent synthetic actor identities** completing the full day/night selection and XTEL round-trip, source/destination residency exclusion, and duplicate suppression. They use original-format synthetic fixtures, **not the 37 real Megaton PACK/CTDA chains**.
2. **130 synthetic actor identities** demonstrating time-jump isolation mid-batch and successful processing of the subsequent evening batch using the 64/64/2 cap.
3. All existing navigation, dialogue, combat, furniture, player-state and v185–v188 regression tests.

This is an integration **audit checkpoint with one bounded scheduler fix**, not a declaration of 37/37 in-headset AI parity. The source scripts cannot replace a physical Megaton day/night walk-through.

## Next actual parity blockers

1. Real Quest acceptance of NPC residency/XTEL, furniture, dialogue and authored animation transitions; log precisely which actor and package fails.
2. Quest/actor CTDA resolution for higher-priority packages, with actual conditional state including XESP and story scripts.
3. NPC-to-NPC Dialogue AI and animation-object props needed by ordinary residents.
4. Persistent full game day/calendar for dated and overnight schedules.
5. Original non-invariant LVLN actors, exterior streaming, respawn rules, DLC/plugins.

**No invented NPC positions, quest variable changes, dialogue content, performance optimisations or draw-distance reductions.**
