# XP and levelling — checkpoint 1

The original Fallout3.esm defines `iXPBumpBase = 150`. Fallout 3 starts
at level 1, with 200 cumulative XP needed to reach level 2. The base-game
cap remains level 20 because Broken Steel is not loaded.

Implemented:
- Pure cumulative XP thresholds and a read-only pending-level calculation.
- Player catalogue reads the original iXPBumpBase game setting.
- Pip-Boy Stats shows earned XP / next-level threshold.
- Successful lockpicking awards XP through the existing perk-aware
  RewardExperience() path used by quests, with per-lock reward protection.
- Pending progression derives from the already-saved XP and committed level,
  so older FQPS saves stay compatible without a save-version change.
- C++ host tests verify vanilla thresholds, alternative XP bump, multiple
  pending advancements, level cap, overflow and XP save roundtrip.

Not yet implemented: committing an advancement (which must also allocate
skill points and select a perk), level-up menu and combat pause, animated
XP HUD notifications, sound triggers, and other reward sources. The runtime
deliberately does not advance the level automatically before skill/perk
allocation is available.

Test locally:
```sh
cmake -S tests/player -B /tmp/falloutquest-player-tests
cmake --build /tmp/falloutquest-player-tests --target experience_progression_tests player_state_tests
ctest --test-dir /tmp/falloutquest-player-tests -R 'experience_progression_tests|player_state_tests' --output-on-failure
```
