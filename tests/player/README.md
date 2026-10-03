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
