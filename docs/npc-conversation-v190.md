# v190 — Resident NPC-to-NPC Dialogue AI (script-free HELLO subset)

Baseline: v189 `a9c427db4a3f742c79cbddc28ee84d971fa75fff`.

## Intent and strict quest boundary

NPC interactions are authored via **PACK type 15 (Dialogue)** targeting placed
actor references (PTDT). Voice/words are independent authored DIAL/INFO
records. This checkpoint interprets the reusable, script-free **HELLO**
subset; it does not attempt to implement quest scenes, INFO result scripts,
player greeting menus, script-driven package target overrides, or dynamically
invent dialogue lines.

Original Fallout3.esm contains 386 type-15 Dialogue PACK records, including
Megaton residents' NPC-targeted packages for Nova, Jericho, Moriarty and
others. Its DIAL `HELLO` topic is type 1 and has authored INFO responses.

## Behaviour

- The original actor's PACK priority, clock schedule, CTDA checks, and PTDT
  actor reference remain authoritative. **For Dialogue AI**, CTDA
  RunOnTarget receives the actual NPC listener, **not the player**.
- Only one live, same-resident, alive NPC actor target is permitted. Actor
  references to the player, missing/disabled/unresident partners, unsupported
  location type, multiple targets, result scripts and procedure actions do
  not start a pretend interaction.
- The initiator approaches its moving target along the existing resident
  NAVM route. The interaction begins only inside an explicit short-range
  conversational envelope; it never teleports or pulls a distant NPC into
  position.
- One resident conversation is reserved at a time. The director checks an
  original `HELLO` INFO for the initiating NPC's real base and actual listener;
  unsupported CTDA or quest context fails closed. It requires an authored
  voice path, one response, valid INFO ordering, no begin/end scripts, and
  the existing active/start-enabled quest gate. **No INFO script, activation
  script, stage transition or quest side effect is executed.**
- Speaker and listener use the existing original speaking/listening idle KFs
  and face each other. The speaker's original voice uses the existing token-
  isolated audio channel and the matching LIP-derived facial morphs. Listener
  remains silent for this verified one-line subset.
- A player conversation has priority. The pair is released if the player
  starts talking, a participant moves away, dies, enters combat, leaves the
  scene, the player opens Pip-Boy, loading begins, or voice playback finishes
  or fails. Bounded timeout and per-actor package retry prevent lockup.
- The pair's reservation/voice token is scene-local and is **not persisted**
  as a completed conversation or fabricated save-game state. Existing actor
  canonical package, combat and persistence systems remain the owners.

## Test boundaries

`npc_conversation_tests` constructs original-format DIAL/INFO/quest/actor
records and validates matching actor identity, different NPC listener,
authoritative authored voice/text, quest gates, unsafe scripts, invalid
INFO order, and no player substitution.

`npc_conversation_runtime_tests` injects the scene-conversation request
seam into the production package executor to verify PTDT selection,
arrival/interaction, and dead-target rejection.

Full existing player/NPC, furniture, combat, XTEL, dialogue and Android build
regressions must remain green. Host mocks are NOT proof that authored voices,
lip sync, physical facing or NPC collision behave correctly on the Quest.

## Still excluded

- Multi-response exchanges and back-and-forth NPC dialogues.
- NPC-directed INFO scripts, quest scenes and scripted package events.
- DIAL categories other than verified HELLO, including Goodbye or custom
  conversation topics whose context/links require quest interpretation.
- Arbitrary NPC chatter outside authored Dialogue AI packages.
- Unloaded-cell conversations, turn-taking groups, faction reputation and
  special animation-object props.
- Real physical headset verification of the conversation pair.

Proceed with a real Megaton headset test for Nova/Jericho/Moriarty
packages with their original quest conditions met; log once-only reasons
where no safe HELLO exists instead of reporting a false pass.
