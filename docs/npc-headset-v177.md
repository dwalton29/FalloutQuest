# Headset NPC dialogue and physical grounding — version 177

Version `0.46.5-headset-dialogue-grounding` follows v176 after Quest testing exposed two device-visible failures: Lucas Simms remained vertically offset until his first forward step and then followed NAVM height rather than the rendered collision surface; the first voiced greeting still crashed at the transition to player choices.

NAVM remains the authoritative route/topology source. It no longer drives the production actor root height. Resident NPC movement samples the same loaded authored Havok collision world and LAND fallback used by player locomotion, selecting the walkable surface closest to the actor's current root so stacked floors retain continuity. NAVM plane projection remains only as the host/no-collision fallback. Grounding now occurs before turn-in-place, after horizontal movement and at waypoint arrival; waypoint completion no longer copies NAVM Z into the actor root.

Android MediaPlayer completion now detaches and releases the completed player and posts the JNI completion notification to the next audio-loop turn rather than entering native code from inside the MediaPlayer callback. When a spoken INFO resolves to choices, choice input and panel publication are deferred by one stereo frame, separating result/audio teardown from the GL text transition. Additional native transition logging remains in place for device diagnosis.

No Fallout dialogue content, route destination, animation, world-streaming, player weapon or Pip-Boy semantics are changed.
