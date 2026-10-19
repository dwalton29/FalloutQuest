# v230 — GameMode CPU and repeated detailed-render submission

## v229 Quest 3S basis
CSV uploaded 2026-10-19: active frame median work 25.15ms, eye0 CPU 9.68ms, eye1 CPU 4.76ms, gameplay CPU 5.36ms. Source GameMode interval roughly 7–10 frames; 195/1525 active frames exceed 10ms in Scripts, clustered ~13.9ms. GPU execution timing is not available. Reported v229 improvement was little or none.

## Targeted changes

- **Static script owner/quest discovery:** cache only immutable ESM reference GameMode eligibility across published-scene revision and storage identity; cache source quest GameMode IDs for Player lifetime. Recheck actual resident NPCs, live running quest state (including at original tick start), CTDA conditions, disabled references, original script event ordering and transaction execution each 4Hz tick. No reduction in GameMode cadence, NPC count, quest execution or script effect semantics.
- **Render submission CPU:** Q2017 prevalidated opaque objects already passed collected/LOD checks. Avoid performing these exact same checks a second time inside DrawSceneObject during its scoped detailed-opaque render. Keep **current-eye** frustum testing, reflection culling, original LAND, alpha/environment passes, water effects and material order untouched.
- **Merchant early-exit:** when no merchant has been visited, skip the full catalog restock walk from per-frame SetScriptPlayerLocation. Any actual visit continues the original path and timestamp behaviour.
- **Phase telemetry:** Q230 GAMEMODE CPU separates reference scanning from the remaining tick execution; bounded slow reports. This locates remaining transaction-cloning cost without pretending to measure an improvement before headset testing.

## Compare v229/v230
Same Quest 3S location, viewpoint and population: compare active CPU median/p90, scriptsUs >10ms count, first/second-eye CPU, render state calls/draws, and Q230 GAMEMODE scanUs vs executeUs. Do **not** claim 72Hz or improvement until same-scene device capture. Shader, texture, NPC AI, actor playback, scripts and draw distances unchanged.
