# World interactions v288

Load-door crime prompts use destination ownership for unlocked transitions.
Original Craterside interior door 00003A43 retains a leveled XLOC while its
exterior partner 00003A1D has no XLOC. Interior exits now consult an unlocked
exterior partner when no explicit runtime lock overrides the interior record.
Explicit script locks remain authoritative. No Megaton IDs are used in policy.

Lockpicking accepts activation while original models prepare asynchronously;
the modal retains input focus and waits for presentation readiness.

Intact NPC assembly excludes authored BSDismember wound-cap partitions
101..113 and 201..213. Original Protectron meshes contain separate cap shapes;
they must not be rendered as intact body pieces. This does not add severed-limb
presentation.

Ambient player-directed HELLO uses original INFO/quest conditions and voice
resolution on the existing shared voice channel. Explicit conversations take
priority. Script-bearing, multi-response and Say Once lines remain excluded.
The 2.5 m proximity, half-second scan, 8-second shared cooldown and 30-second
actor cooldown are VR presentation policy, not recovered original-game timings.
This is an initial ambient greeting path, not a complete dialogue-event director.

Host checks cover lockpick input, paired exit/script-lock policy, conversation
selection and original Protectron assembly. Quest aiming, audibility, actual
transitions and visual placement still require headset verification.
