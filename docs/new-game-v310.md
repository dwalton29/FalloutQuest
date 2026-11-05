# v310: birth identity, modal input and v309 CI fix

Continues v309's original-script bootstrap. This checkpoint implements the
original sex-choice and name-entry portions of birth, with no quest IDs or
quest-stage transitions in the runtime UI.

## Behaviour

- `player.SexChange male/female 1` stores a player override; the original
  catalogue stays immutable. GetIsSex/GetPCIsSex dialogue conditions and level-up
  eligibility read the saved sex. Player body/hand NIFs come from the original
  RACE sex-specific model tables, and equipped armour uses its female model
  when present. This does not add NPC SexChange or a race/face editor.
- `GetPlayerName` opens the original `menus/dialog/texteditmenu.xml` presentation
  with a controller keyboard beneath it. Left/right selects a character, A adds
  it, B deletes, and selecting Done confirms. Entry supports 32 printable ASCII
  characters as a VR keyboard policy. The input field uses the authored frame's
  available width to avoid wrapping a long name into the prompt.
- Pending name ownership, the draft, confirmed name and sex survive save/reload
  in save v29. The reader retains support for previous versions. Confirmation
  trims outside spaces and rejects an empty name; opening/closing the UI never
  directly sets a quest stage.
- Scripted message/name menus run captured original MenuMode blocks. They freeze
  the GameMode clock, GetSecondsPassed deltas, reference GameMode events and
  world-clock advancement; closing a menu does not add elapsed menu time to
  scene timers. This is not yet a complete engine-wide MenuMode implementation
  with every original menu-ID filter.
- Scripted restrictions on game activation/turning no longer consume the menu's
  A button or horizontal keyboard input. Gameplay interaction stays excluded,
  including the confirmation frame. Head tracking and avatar hand preparation
  remain live while NPC simulation is held during these modal menus.

## Build fix

v309 CI reached the player suite and failed `npc_combat_tests`: its previous
contract rejected player MoveTo, which v309 intentionally implemented. The test
now verifies the pending authored player arrival independently of the NPC pose,
while retaining the restriction on moving an NPC to the tracked player. No
production movement functionality was removed to satisfy the old assertion.

## Validation and limits

Synthetic tests cover immutable definitions, sex-dependent conditions, modal
timer isolation, invalid names, held-button/tracking-loss input, confirmation,
and save/reload of a partially typed name. The original ESM integration runs both
sex choices, selects the corresponding original Dad/Mum INFOs, simulates
successful voice completions, reaches the original name request, and resumes
the original script to birth stage 42 after confirmation. It also restores the
confirmed identity from a save. Audio/device presentation is not simulated by
these host tests.

Original TextEditMenu geometry was checked with the original font metrics.
Headset verification is outstanding. ShowRaceMenu, face preview/editing, family
face matching, age transitions and completion of CG00/childhood remain for the
following checkpoint; they have not been replaced with automatic acceptance or
manual quest advancement.

Validation completed: Vulkan-enabled Android ARM64 library build; the full
52-test player CTest invocation (50 passed, two original-data tests skipped by
their default invocation); both skipped MQ01 tests then passed when run with the
supplied ESM; the original birth identity integration; and scene-publication,
loading, HDR-order, dispatch and UI-layering source checks.
