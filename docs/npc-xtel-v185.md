# v185 — Authored XTEL / CELL destination mapping

Canonical base: v184 `d5f386ec9c0f369d5d9c75fadc144649ff842d16`.

## Scope

This is the **mapping checkpoint only**, not an NPC load-door traversal or
unloaded-schedule implementation. Existing player XTEL transitions, interior
door swing animation, NPC local doors, world streaming, drawing and grounding
have not been changed.

The new `fo3xtel::Index` scans a real Fallout 3 ESM from the caller's
supplied path once, off the render thread. It decodes REFR NAME + XTEL
(destination REFR, destination XYZ/rotation, optional trailing flags), nested
CELL group ownership and exterior WRLD ancestry, plus canonical DOOR bases.
It resolves source and destination CELL/worldspace, preserves raw record and
XTEL flags and counts missing targets/non-DOOR links. Malformed payloads,
duplicate references, bad group bounds and truncated files reject the new
index atomically, retaining the previously built index.

API:
- `Build(path,error)` constructs the index.
- `Find(sourceDoorRef)` returns one immutable directed door connection.
- `Outgoing(cellFormId)` returns that CELL's authored source-door refs.
- `CellRoute(fromCell,toCell,doorRefs)` computes a directed topological
  CELL route using original door references (BFS).

These routes are **not movement plans**. No door is deemed traversable merely
because an XTEL exists: enable parents, disabled/locked/scripted doors, key
ownership, live NAVM approach, actor permissions and actual resident-cell
transfers must be validated by the later NPC traversal milestone. An initially
disabled REFR remains indexed with its original flags for those decisions.

## Unmodified original Fallout3.esm audit

The supplied original master contains:
- 42,410 CELL records; 568,107 placed REFR records.
- **1,118** XTEL-bearing source references.
- **1,118/1,118** destinations resolve to actual REFRs and both endpoints'
  bases are DOOR records.
- **1,118/1,118** authored reverse XTEL links.
- **1,114** directed cross-cell links (four same-cell links).
- **778** directed links crossing exterior/interior or worldspace contexts.

This is a **directed-link count**; do not confuse it with physical door
pair count or with the number of NPC-traversable connections.

Megaton original-record examples:
- `00003A73` in interior `00003A34` targets `00003A1C` in
  exterior CELL `00000A96` under WRLD `00000A74`.
- `00003A1D` in exterior CELL `00000A96` targets `00003A43` in
  interior `00003A2A` (Craterside Supply).

Graph lookup therefore establishes a two-link *topological* path from CELL
`00003A34` through `00000A96` to `00003A2A`. Actual
NPC scene transfer is deliberately not enabled by this change.

## Validation

CI runs `xtel_index_tests` with a synthetic ESM fixture covering
interior/exterior ancestry, target and non-DOOR rejection, 28/32-byte XTEL
payloads, flags, reciprocal edges, route construction and malformed atomic
failure. For full original-data verification using a local installed master:

```sh
cmake -S tests/world -B build/host-world
cmake --build build/host-world --target xtel_index_tests
./build/host-world/xtel_index_tests /path/to/Fallout3.esm
```

No original assets are committed. Android build success does not count as an
in-headset NPC traversal test. Next milestone: load-door approach and actor
CELL transfer using this authored graph, with per-actor state and permissions.
