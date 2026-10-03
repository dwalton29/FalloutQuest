# Player state host checks

```
cmake -S tests/player -B build/host-player
cmake --build build/host-player --parallel 2
ctest --test-dir build/host-player --output-on-failure
```

Synthetic compressed NPC_, grouped ESM, GMST and item fixtures cover authored
stats, gender models, non-playable flags, auto-calculated aid value disclosure,
inventory counts/conditions, weapon replacement, biped slots, equipment stack
splitting, quest/cannot-drop removal, authored COED conditions and unresolved
ownership rejection, overflow, weight and health/AP boundaries.
Persistence tests cover stable identities and equipment, atomic restore failure,
checksums, truncated files, mismatched definitions, semantic corruption with a
valid checksum, write failures, blocked-save preservation and dirty flushing.
Missing settings and unresolved levelled starting inventory fail atomically.

Optional original-data integration (game data is never committed):

```
build/host-player/player_state_tests /path/to/Fallout3.esm
```

The supplied original yields 1,762 items, all seven SPECIAL values at five,
200 maximum HP, 75 AP, 200 carry capacity and the original PipBoy/PipBoyGlove
inventory. This verifies data interpretation, not full Bethesda gameplay parity.

`interaction_runtime_tests` executes the actual render interaction bridge with
recording stubs: nearest item/door choice, moved bounds, loading/occlusion guards,
blocked ownership, key access, pickup without origin reset, grab/rigid-body
retirement and collision/save notifications. Geometry checks cover slabs and
two-sided triangle occlusion. State fixtures cover REFR counts/condition,
duplicate pickup, collected-reference persistence and v1 save migration.

Container checks cover CONT/CNTO/COED and LVLI/LVLO decoding, Use All above-level
entries, parent counts, chance-none, cycle rejection, stable previews, equipment
condition, empty-container persistence, failed transfer rollback and v1/v2 save
compatibility. The runtime bridge exercises actual floating panel state, selection
and A transfer without travel or removal of the container. Cursor checks cover
neutral arming, edge/hold repeat and target changes. Original-data integration
loads 535 containers and generates 9,000 accessible placed references at the
authored baseline level (one unsupported result rejected). This is not a headset
visual check or full quest/respawn/RNG parity.
