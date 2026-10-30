# Saloon scripting, door prompts and compass (v293)

Unlocked door activation no longer borrows item-theft ownership for its red
prompt. Picking or forcing an owned lock still uses the existing theft policy;
opening a door still obeys its lock, key, disabled state and OnActivate script.

The original GobSCRIPT fails before its radio exchange without reference-qualified
GetDistance, GetTalkedToPC, GetOpenState, Look/StopLook and SayTo. The interpreter
now supplies those shared operations. GetInCell uses canonical player/actor CELL
identity; settled GetOpenState reads saved door targets or authored REFR ONAM.
Transitional GetOpenState values 2/3 and general radio toggling are not added.
Reference commands tolerate original whitespace after a receiver's dot.
Say Once a Day now uses persistent per-actor/INFO history and the original
fSayOncePerDayInfoTimer (12 game hours in the supplied executable), including
ESM overrides. It does not mean a fixed midnight reset.
Say/SayTo selects and pins its INFO inside the script transaction, before later
local-variable assignments change its gates. The native voice adapter revalidates
result capability for that INFO without reselecting against changed variables.
Selection/priority/random-stack eligibility is shared with ordinary dialogue.
SayTo's listener context uses the actual NPC rather than player coordinates.

AddScriptPackage/RemoveScriptPackage maintain an ordered priority list ahead of
base/template packages, invalidate package evaluation and persist in optional save
version 23. INFO daily-use history uses optional version 24. Existing saves remain supported. Dialogue packages whose reference
start location is their actor target can execute through the existing resident
conversation director; their authored target distance replaces the fallback.

Original INFO response speaker IDLEs are prepared on the scene worker for matching
speakers and played on the live actor during speech, including their original
Sound text keys. Scripted Say/SayTo and NPC dialogue preserve Say Once bookkeeping
without setting GetTalkedToPC. Package-driven NPC HELLO uses the shared dialogue
transaction/audio-completion path, including original INFO results. Ambient
walk-past player remarks retain their existing script-free selection policy. This connects Gob's original
LooseRadioBangOn rather than introducing a Gob-specific animation. Responses that
need animation objects or seated pose layering remain unsupported. This does not implement every automatic
idle-tree, prop, eating, or sandbox activity; stationary no-wander packages must
not be replaced by invented roaming.

The head-anchored bottom-centre compass uses the original HUDTemplates/CompassIcon
345x64 window, 40x50 marker proportions and original direction/landmark/discovered/
objective/player-marker sprites. Physical size and head-relative placement are VR
policy. It draws after post-processing, without depth writes, through the shared
GLES/Vulkan UI bridge. Heading geometry updates once per stereo frame; target
membership updates at 5 Hz and immediately on CELL/world changes. Nearby NPC radar
perception and blinking are not implemented in this checkpoint.

Quest navigation now evaluates original objective-target conditions instead of
omitting every conditional target. World-map quest icons use canonical actor
positions and interior XTEL entrances; compass targets route through interior
XTEL hops. Full general quest-target metadata and cross-world pathfinding remain
outside this checkpoint.

World-map projection now follows the supplied Fallout3.exe: the image's inner
rectangle is 104/1024 + normalized * 816/1024 (00946172..00946197). Bounds include
whole NW/SE cells (009505B3..0095060E). ONAM offsets are game units; scale operates
about the NAM0/NAM9 midpoint, followed by translation (007C8D00..007C8DFC).
The old code omitted the image border and whole-cell extent, multiplied child
world offsets by 4096, and scaled about the origin. Projection, waypoint inversion,
selection, player position and quest icons share the corrected transforms. No
location-specific calibration is used. A WORLD MAP PROJECTION context log remains
available for headset comparison.

Validation: original-data tests exercise Gob/Nova's real proximity, look, speech,
SayToDone continuation, final script-added Moriarty package, its original HELLO
removal result, and script/package save/reload. They supply audio completion
events and do not certify actual headset voice playback. Existing source, package,
actor-animation, compass stereo-render and bomb reward checks are retained.

For headset verification, use a save from before first speaking to Gob: the
original GetTalkedToPC gate deliberately suppresses his radio exchange afterward.
Test ordinary unlocked doors and an owned locked door separately; check world-map
locations, D.C. child-world markers, zoom/pan and waypoint placement; track an
active quest for compass arrows, rotate across North, and save/reload during and
after the radio exchange. Physical Quest verification is still required.

The host save/reload checkpoints are between speech phases and after completion.
An in-flight audio/session continuation is not serialized by this change; exact
mid-line reload behaviour remains a separate engine gap.
