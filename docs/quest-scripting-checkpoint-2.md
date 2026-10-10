# FalloutQuest scripting checkpoint 2 — native event dispatch (v203)

## Repository and provenance

Built on the v202 quest-stage foundations. The source of truth is the user's
Fallout3.esm, not synthetic quest outcomes. Event scripts are SCPT source
blocks selected through original base SCRI and placed reference identities.

## Implemented

- Immutable SCPT event-block index in scripting/fo3-event-scripts.h:
  name, optional filter, source line and original result body retained.
  Compiled bytes stay in Definitions and are not executed.
- Catalog eventBaseScripts resolves base SCRI for ACTI, NPC_, CREA, doors,
  furniture, scripted items and additional selected types. eventPrograms is
  built once from ESM. New event-only metadata does not modify save fingerprint.
- Player DispatchReferenceEvent reuses the transactional, bounded v202 quest
  interpreter. All side effects from a failing event roll back. Unsupported
  filters and expressions fail rather than guessing. Only absent filters and
  the explicit Player activator filter are admitted.
- Canonical player/NPC weapon damage dispatches OnHit; a nonessential fatal
  hit dispatches OnDeath once at that death transition. Invalid source scripts
  never roll back physical combat damage.
- VR A activation recognizes authored scripted ACTI bases and displays their
  original FULL name where available. Scripted items and blocked scripted
  doors may dispatch OnActivate without silently opening or collecting them.
  NPC dialogue retains its existing validated OnActivate bridge.
- Focused simulation at four passes per real second dispatches GameMode blocks
  on running quest scripts (not per eye/frame, never while paused/loading).
  The 4 Hz cadence is a bounded Quest adaptation, not PC timing parity.
- Unsupported events are logged once per script/event/error, with source
  location. Compiled-only content is never treated as source.
- Launcher restores a valid FQPS save rather than silently deleting it.
  Invalid saves remain untouched and writes are blocked.
- Added synthetic tests/player/event_script_tests.cpp with parsing,
  transaction rollback, filters, event routing and GameMode cadence cases.

## Not implemented

This remains a bounded event dispatcher rather than a complete Bethesda VM.

- General OnLoad, OnTriggerEnter/Leave, OnAdd, OnEquip, SayToDone,
  package/script-effect events and SetScript.
- GameMode on resident NPCs/objects, unloaded reference simulation.
- Most SCPT instance-local and ref-valued variables; arbitrary event filters.
- Bytecode, compiled-only event execution, full expression/command semantics.
- World operations such as Enable, Disable, MoveTo, EVP, Kill, scripted packages;
  UI and inventory actions such as ShowMessage, GetButtonPressed, RemoveItem;
  rewards and scripted effects.
- Exact Fallout 3 GameMode timing, additional death sources and event priority.

The original MS11BombScript OnActivate can now be reached via an available
bomb ACTI reference, but cannot yet disarm it: required menu and skill
operations are unsupported and the event must reject atomically. Nothing
should simulate a successful disarm.

## Validation instructions

Host:

    cmake -S tests/player -B /tmp/fq-player-tests
    cmake --build /tmp/fq-player-tests --target event_script_tests
    ctest --test-dir /tmp/fq-player-tests -R event_script_tests --output-on-failure

Also run the existing player/dialogue test suite. Android:

    ./gradlew :app:assembleDebug

On headset, check Lucas acceptance, bomb activation diagnostics,
nonessential actor death, persistence over restart and event cadence.
For unsupported event diagnostics:

    adb logcat | grep "SCRIPT EVENT"

These tests must be actually run and their results reported before claiming
build/headset correctness. No arbitrary quest completion is claimed.
