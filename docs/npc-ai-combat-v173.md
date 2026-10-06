# Authored Patrol follow-up — version 173

Version `0.46.1-authored-patrol`, extending main `b5b81c9d146759c7166266c405712fc594175560`.
The resident combat implementation and limitations in [version 172](npc-ai-combat-v172.md) remain applicable. This follow-up does not complete all requested AI/combat acceptance criteria.

## Original data and execution

FO3 xEdit definitions identify PACK type 13 as Patrol, PKPT byte 0 as Repeatable, REFR XLKR as the next linked reference, and XPRD as marker idle time. The [GECK Patrol documentation](https://geckwiki.com/index.php/Patrol_Package) specifies closest-point entry for repeatable routes, circular traversal or reversal of open routes, and first-point entry for nonrepeatable routes. [Idle Markers](https://geckwiki.com/index.php/Idle_Markers) specifies idle time in seconds.

The catalog compiles a bounded immutable marker chain once before reference pruning. Supported Patrol packages use a specific PLDT reference or the actor’s own XLKR linked reference in one resident cell/world. Actor-specific chains are compiled from actual inherited PKID lists and cached by actor/package ID. Each leg uses the existing authored NAVM graph, reciprocal seams, surface grounding and collision rejection. A repeatable route starts at its nearest authored point, follows circular links or reverses at the endpoints of an open chain. Nonrepeatable routes visit the first point through the endpoint (including returning to the first point of a circular chain); a duration holds the endpoint, while no duration permits restarting the route. Markers wait for their authored XPRD time.

Missing/malformed chains, intermediate loops, routes longer than 256 markers, unavailable cells, nonfinite/negative waits, marker scripts, nonzero INAM idle or TNAM dialogue actions are explicitly rejected. Higher-priority rejected packages allow lower-priority supported packages. Heading-marker orientation, furniture/idle actions and NPC door traversal remain unsupported. No EDID/name-specific gameplay logic is used.

Patrol leg progress uses the existing revision-8 actor sequence field. Combat and save/stream restoration retain that leg and rebuild its path from the actor's current canonical position. No pointer or transient path is serialized. A marker wait interrupted by unload/save/combat is not persisted: the next leg resumes without the remaining wait. Exact interrupted wait restoration remains a limitation.

Guard's completion path now checks the correct type 14 throughout and holds its authored destination instead of rebuilding the route. Follow sets package ownership even when already inside the authored follow distance.

## Verification

Host regressions exercise Guard arrival/hold, Patrol catalog-chain retention, ordered legs, marker waits, nonrepeatable endpoint holding, closest repeatable entry, endpoint reversal, scripted-marker rejection and restored leg routing. Original Fallout3.esm also verifies multiple valid actor-linked Patrol routes and the four-point `0007E6DD` Patrol chain through `0007E6DE`, `0007E6DF`, `0007E6E0` and `00019038`, including its nonrepeatable flag and authored 20-second final wait. These IDs appear only in regression tests, not runtime behavior.

Existing combat, dialogue, physical weapon, save migration and navigation suites must pass, as must the Android APK workflow for the delivered HEAD. Headset validation remains required for actual Patrol movement, package interruption/resumption, marker waits, collision and unchanged combat/loot/dialogue interactions. Escort, Eat, Sleep, authored Flee, NPC local/load doors and the other parity gaps recorded in version 172 remain unfinished. No Bethesda assets are committed or packaged.
