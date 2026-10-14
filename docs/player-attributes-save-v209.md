# v209 — Persistent player stats and fresh max-stat saves

Base: 5f99428df64c8df1fa62c625bfc5c08461c1f5e2. FalloutQuest FQPS v15 adds a required player-attributes tail to each newly saved game: an empty-or-populated RFE1 block followed by `PST1`, then seven SPECIAL bytes and fourteen skill bytes in the original DNAM order. The unused Throwing slot is preserved. No original Fallout3.esm record or bundled asset is modified.

The `Player::SetPlayerAttributes` API validates SPECIAL 1–10, skill ratings 0–100, derived maximum HP/AP and atomic state updates. Save validates the state too. Restore supports v1–v14 without changes to older layouts and keeps original ESM stats for such saves; v15 validates size, marker, values, fingerprint and CRC before state publication. HP/AP damage bounds are checked *after* reading restored SPECIAL so the boosted capacity is handled correctly. Original lock, RFE1 reference, quest, dialogue and inventory states are untouched.

Tests: max-stat persistence, 100-count extra inventory stack, invalid SPECIAL/skill atomic rejection with updated CRC, downgrade v15 fixture to v14 and baseline stat inheritance, and existing migration tests. v208 APK cannot load v15; v209 APK is required. For a clean custom save, use the original Fallout3.esm fingerprints and the 100 bobby-pin inventory stack; keep all quests/world state fresh.
