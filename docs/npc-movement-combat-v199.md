# NPC movement and combat — v199

NPCs now move continuously across intermediate NAVM waypoints, with acceleration,
braking at their actual destination, and turns while moving. A bounded corridor
shortcut must cross every original shared portal in order; the original surface
sequence remains available for grounding and obstruction recovery. Locomotion
keeps its gait clock across waypoints rather than restarting at each one.

Roaming destinations exclude living residents and their chosen endpoints. If all
32 bounded probes are occupied, the actor waits and retries its original package.
Swept crowd checks prevent crossing another resident's standing root. A blocked
actor can yield sideways only on its current NAVM surface and after the existing
world collision check. Existing overlaps can separate. Original XTEL arrivals
publish serially after their arrival root clears, without invented spawn offsets.
Shared Common House schedules and authored furniture/idle markers remain intact;
this does not guarantee different routes through a genuine narrow passage.

Combat uses decoded source confidence and optional original GMST flee settings.
Cowards flee; unarmed cautious/average actors retreat; injured actors use the
source confidence thresholds. **Comparing health ratio with those thresholds is
a Quest approximation**, not Bethesda's relative combat-strength algorithm.
Flee destinations must be reachable NAVM triangles, farther from the threat, and
unoccupied. Original distance settings govern escape selection. Real occlusion
and living allies influence retreat; armed actors can reposition inside their
weapon range and seek actual cover during firing pauses. Failed cover selection
waits rather than fabricating cover. Unsupported weapons remain unsupported.

Original CSTY CSSD fields now supply cover radius/chance, firing duration, pause
duration, wait-to-fire and semi-auto delay. A null actor/package style uses the
original `DefaultCombatstyle` record found by EDID; an unavailable explicit style
does not silently become a default. Original WEAP damage, ammunition, reload,
attack events, hit detection and committed attack/reload handling remain in use.

Male/female source `mtleft`, `mtright`, `mtbackward`, `mtforward` and
`mtfastforward` clips supply movement. Armed tactical movement combines the
original equipped weapon Aim pose above Spine2 with original movement below it.
That branch composition is a Quest bridge, not a recovered Bethesda animation
graph. Missing directional clips fall back to the existing forward movement clip.

Standalone policies are named in `MovementPolicy`: turning 3 rad/s,
acceleration 4 m/s², braking 3 m/s², standing turn threshold 2.1 rad,
crowd radius 70 game units and vertical separation 90 game units. Endpoint retry
is 0.5 s, tactical replanning 0.75 s, and morale reevaluation 1 s. Tactical work
is bounded to 32 occlusion candidates and 12 route attempts. These values are
Quest implementation choices, not asserted original engine constants.

Host regression coverage includes continuous portal traversal, exact endpoint
arrival, corridor rejection, swept spacing, overlapping-root separation,
occupied destination retry, confidence decisions, source burst/pause timing,
animation branch composition, local door waiting, furniture, package resumption,
unloaded travel and XTEL. Additional source checks load the attached Fallout3.esm
and execute package/combat parsing against its actual records and Megaton NAVM.

Headset acceptance remains necessary: observe the Common House morning exit and
bomb crowd, narrow paths and slopes, opposing traffic, moving local doors, loaded
and unloaded XTEL transitions, save/reload, and low-health/unarmed/confident combat
actors. Check weapon aim during side/back steps, firing/reload events, escape
distance, furniture exits and dialogue recovery. Host tests cannot establish
visual fluidity or full Fallout 3 combat parity.
