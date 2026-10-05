# FalloutQuest v167 authored NPC package runtime

This milestone adds a bounded live NPC package runtime using Fallout 3's installed data rather than invented schedules or random free-roaming.

## Authored inputs

- NPC_ PKID order supplies package priority.
- PACK PKDT supplies procedure type/flags.
- PACK PSDT supplies supported time-of-day schedules.
- PACK PLDT supplies movement anchors and authored radii.
- PACK CTDA is evaluated through the existing supported Fallout dialogue-condition evaluator.
- NAVM DATA/NVVX/NVTR supplies the walkable triangle graph and authored adjacency.
- Original character locomotion KFs supply walking/turning animation, including male/female mtforward.kf.

## Currently executed

The first runtime executes supported same-navmesh instances of:

- Wander (type 5)
- Travel (type 6)
- Sandbox (type 12)

Package selection follows the actor's authored PKID order. Unsupported or unevaluable higher-priority packages are skipped and logged rather than approximated. Supported movement uses A* over authored NAVTR neighbours. Dialogue temporarily suspends package movement and returns the actor to package activity afterwards.

## Deliberate limits

This is not yet the whole Gamebryo AI engine. The runtime deliberately does not fake:

- Escort, Eat, Sleep, Patrol or Dialogue package procedures.
- Executable package scripts.
- Cross-NAVM/external-navmesh links.
- Door traversal or package-driven cell changes.
- Full calendar/month/weekday/date schedule semantics.
- Missing engine actor-value movement-speed calculation.

Until Fallout 3's exact actor movement-speed calculation is decoded, movement along an authored path uses a documented standalone-VR bridge speed of 1.05 m/s. Sandbox/Wander destination selection is deterministic inside the authored radius so it is reproducible; the permitted area and path itself remain authored.

When a package cannot be represented safely, FalloutQuest leaves the actor stationary for that package and emits an NPC PACKAGE UNSUPPORTED diagnostic instead of inventing behaviour.
