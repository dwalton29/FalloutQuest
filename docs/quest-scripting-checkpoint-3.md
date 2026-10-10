# FalloutQuest scripting checkpoint 3 — authored messages, skills and inventory (v204)

## Source and scope

Main is canonical. The original user-supplied Fallout3.esm is the authoritative source. This pass extends the bounded v202/v203 source interpreter and reference dispatcher rather than hardcoding Megaton outcomes.

Verified original record chain:
- MegatonBombRef `00014BC8` -> MegatonBomb ACTI `00003BC8` -> MS11BombScript SCPT `00078CB3`.
- MS11BombMessage MESG `00078CB2` retains its original title, description and three ITXT buttons.
- MS11BombMessageNoSkill `00078CB1` uses the authored `%.0f` required-skill placeholder.
- FusionPulseCharge MISC `00014F81` has quest-item record flag `0x400`.
- MS11 QUST `00014E9E` stages 30 and 40 are the original disarm/rig branches.

## Implemented

- MESG decoding: immutable FULL/DESC/ITXT records and source FormID mapping. No invented choice labels.
- SCPT expression additions: `IsActionRef player`, `player.GetAV <skill>`, `player.GetItemCount <item>`, `GetButtonPressed` and `GetDistance player`. Queries read the real event activator, saved player inventory/skill and scene player position; ungrounded contexts reject.
- Source commands: `ShowMessage`, `PlaySound`, `player.RemoveItem`, `SetQuestObject`, `RewardKarma` and actor-qualified `EVP`. Unknown source still fails closed.
- Original menu presentation through the existing FalloutQuest native font/panel renderer, with ordered ITXT options, right-stick selection, A confirmation and B acknowledgment/authorised original "Do nothing" cancellation. This is a VR presentation adaptation, not a claim that Gamebryo's XML menu system runs.
- Script message selection is passed to its owning reference/quest, and the next eligible GameMode receives the button index. Reference GameMode uses resident scripted ACTIs at the existing 4 Hz bounded simulation cadence.
- Script-origin item removal by FormID and count is distinct from manual drop restrictions; it validates the available non-equipped quantity before any mutation.
- Original MS11 disarm dependencies: mutable quest-item flag overrides, karma changes and package reevaluation through the existing PackageRevision mechanism (the authored PACK selector remains responsible for actual selection).
- FQPS v12 persists modified karma and SetQuestObject overrides. v1-v11 restore paths remain supported; synthetic legacy conversion fixtures were updated.
- Queued messages/sounds are rolled back with failing source events and failing dialogue/quest-stage transactions. Command preview also restores queues.
- Host synthetic event tests cover menu selection, inventory consumption and stage/disarm state persistence; host CI and headset validation must be reported separately.

## Important remaining limitations

- Stage 40's original `MS11DetonatorRef.enable` and conditional `MisterBurkeRef.MoveToMarker` require an actual persistent world-reference mutation, loaded/unloaded actor relocation and scene publication. They are not silently skipped: the script/quest-stage transaction still rejects and restores its state. Do not claim the bomb is armed.
- Stage 30 is implemented at the command layer but complete actual-game success depends on all original MSObjectives stage-item conditions succeeding against current state and successful headset validation; it has not been claimed until tested.
- Messages currently support unformatted text or one source `%.0f` parameter. Arbitrary compiled-only SCPT, other format specifiers, general bytecode, dynamic world commands and per-GameMode PC cadence remain unsupported.
- The modal is not persisted mid-selection; gameplay/quest state remains saved. Opening, closing, input and 4 Hz dispatch require testing on the physical Quest.
- No player item is granted, no quest stage is skipped, and no bomb action is simulated on failure.

## Validation

Host: `cmake -S tests/player -B /tmp/fq-player-tests && cmake --build /tmp/fq-player-tests --parallel 2 && ctest --test-dir /tmp/fq-player-tests --output-on-failure`.

Android: `./gradlew :app:assembleDebug`.

Headset: activate bomb at Explosives below and above 25; select Do nothing; disarm through stage 30; attempt rigging with and without one original Fusion Pulse Charge; verify unsupported stage 40 rolls back inventory/quest changes; restart and inspect original objective/persistent item flags. Inspect `SCRIPT EVENT` diagnostics in logcat and do not infer success from menu display alone.
