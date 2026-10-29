# v280 — shared NPC package evaluation and reusable door topology

The latest headset log shows the ordinary wasteland near its GPU budget, but
Rivet City additionally spends tens of milliseconds in NPC package/perception
work. The same door-travel intent is logged 5,316 times after fast travel.
`Q230PerceptionPackage` calls the complete package selector independently of
the package executor. That selector resolves remote XTEL destinations, scans
NAVM for door approaches, and previously floods the navigation graph again for
each actor/query. Skinning is not the source of these logged CPU costs.

## Changes

- Perception and movement share the existing quarter-second package evaluation.
  Reading policy neither adopts a package nor replaces an active door intent.
  Movement applies the selected intent. Schedule-minute changes, canonical
  package revision, existing retry/interruption invalidation, scene/graph changes,
  and changes to the active package invalidate the cached evaluation.
- Retry checks use current simulation time without consuming movement's previous
  update timestamp. Actor targets remain live between condition evaluations.
- CELL-route leg counts use one reverse search per target CELL on the immutable
  XTEL index, rather than a separate forward search for every candidate door.
  This was the dominant repeated cost in the original Rivet City comparison.
- Door eligibility shares validated, immutable NAVM topology. Each directed edge
  still passes the production portal test. Reverse reachability is computed once
  per door's goal triangle and reused by different actors/start triangles.
  Directed links are never treated as an undirected connected component.
- Door projections are cached by reference and exact placement coordinates.
  Ranking, actor keys/permissions and record flags remain live. Cached CELL
  distances retain the same directed shortest-hop ranking.
  Actual movement retains the crowd-aware route planner, grounding and handoff.
- At most four graph entries, 128 door projections and 16 goal masks per graph
  are retained, plus at most 16 CELL-distance maps for the current XTEL index. Weak graph ownership avoids extending old scene residency and
  prevents recycled addresses from reusing another graph's topology.
- Intent logging occurs on adoption/change. The existing AI performance line
  now includes package evaluations, topology builds, door scans, reachability
  floods, CELL-route builds and cache hits, allowing the headset capture to verify reuse.

These are ephemeral caches, not save-format or authored-package changes. A new
graph still requires its first build; no claim of removing all AI/frame stalls
or fixing the separately observed SpaceWarp fallback is made in this release.

## Validation

Production package, combat, XTEL, crowd, door and furniture regressions cover
selection, movement, locks, interruptions and handoff. Additional tests exercise
one evaluation shared by policy and movement, retry/minute/graph invalidation,
directed connectivity, a moved door target, replacement topology and live lock
changes after cache warmup. An optional original-ESM comparison checks identical
door choices with the retained uncached implementation, including Rivet City's
original exterior NAVM and XTEL data. In the local debug host run, 36 repeated
Rivet door queries returned the same choices and took 739.4 ms before versus
55.9 ms after (about 92% less query time, including first cache construction).
There was one CELL-route build, one NAVM build, one door projection and one
reachability flood. Query origins use original exterior NAVM near the door;
they are not saved headset actor poses. This is not an app-wide gain or Quest
FPS forecast. The original Megaton comparison also retains all 72 choices and
54 successful handoffs.

## Terrain fusion investigation

Fusion is **not enabled** in this build. The v243 recovery remains intact for
both cold and streamed admissions. Its earlier headset evidence is significant:
the fused program linked and geometry uploaded, yet the ground disappeared even
with zero folded groups. That does not establish a CPU fusion-topology defect.
The earlier reserved-word compile failure was a separate bug already repaired.

The production software GLES comparison still passes with 1–3 paint layers,
normals, specular, fog, local lights and shadows in FP16/RGBA8. It is not an
Adreno test, and it does not reproduce or explain the historical post-link
failure. The CPU geometry tests and program/fallback tests also pass.

There is a further concrete blocker for the current Vulkan backend:

| Interface | Working Vulkan LAND | Old fused LAND |
|---|---|---|
| Shader source | Frozen v237 program | Q76B factory fused program |
| Vertex input | Locations 0–4, 12 floats | Adds paint alpha at location 5, 15 floats |
| Material interface | Base diffuse/normal and original lighting | Adds paint count, six paint samplers, flags, round mode |

`tools/build_vulkan_shaders.py` deliberately extracts the frozen v237 shaders;
the Vulkan terrain pipeline supplies only attributes 0–4. Registering a GLES
program as terrain does not compile that program into a new Vulkan material.
Removing the quarantine alone would therefore remove original paint triangles
without implementing their Vulkan shading, independently of the old GLES bug.

A proper next terrain change needs a separate Vulkan fused material and matching
geometry/descriptor interface, comparison against the working multipass output
through the actual backend, and coverage for zero fused groups, original base
layers, mixed fallback stacks, indexed geometry, both eyes and streaming. The
existing isolated GLES pixel test cannot establish that end-to-end contract.
Only then should an on-device A/B trial replace the recovery path. No terrain
performance gain or fix for the historical Adreno defect is claimed here.
