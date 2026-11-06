# v320: remaining vault scene blockers

Follows the user's v319 headset report. Range-impact work is preserved separately
and is not part of this checkpoint. No scene-specific positions, stage overrides
or character routes are introduced.

An interactive conversation's final world result could block its own resource
admission: the resident workflow treats active dialogue as a conflicting UI.
The session now enters Results before requesting publication. Only this finished
INFO's owned result admission/ticket can exempt that dialogue flag. Player input
remains focused until the original transaction finalizes. Barter, Pip-Boy,
weapons, held objects and affected-reference hazards still block admission.
Readiness now checks those other interaction gates, so temporary interaction
conflicts wait instead of rejecting the closing result.

Original Look commands used to disappear if their NPC had not been uploaded.
The world adapter retains targets and reapplies them when actor residency changes;
StopLook clears both the retained target and an already resident actor. This is
session-local presentation state, not a saved quest repair.

The resident path door picker now ignores doors already settled open. Previously,
an open frame could repeatedly win the AABB query and hide the closed door ahead.
Moving doors, locks, keys, scripts and XTEL isolation keep their existing rules.
Scripted activation door support is not expanded by this change.

Crowd movement now separates soft navigation spacing (70 game units) from physical
blocking clearance (35 units, a standalone movement policy). Original crowd
routing preferences stay in use; actors can occupy close staged positions without
treating a one-metre gap as physical overlap. No NPC is teleported and actual
near-root overlap still blocks movement.

Regressions cover finished-result interaction admission, other modal exclusions,
the original birthday closing INFO, delayed actor Look delivery, StopLook,
settled versus moving door selection, close staging and physical overlap.
Headset verification remains required for the reported Dad/Overseer routes and
visible party positions. These changes cannot reconstruct dialogue effects that
an older build already discarded in a saved game.
