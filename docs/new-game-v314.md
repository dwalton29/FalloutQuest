# v314: original playpen sequence and package procedure results

Continues v313, keeping appearance editing deferred. The runtime additions are
reusable package and trigger support; no quest IDs or stage advances are embedded
in the engine.

## Package boundaries

PACK POBA/POEA/POCA fragments retain their own source, compiled bytes, headers,
references and raw script fields. Start, completion and change execute the
corresponding package result and actor OnPackage callback in one transaction.
Actors without their own SCRI can execute package fragments. Existing saved
package-event ownership prevents repeated results after route reconstruction,
scene retirement or reload. Failed results leave effects and completion private.

Package fragments carry a distinct authored origin. Resident publication matches
the exact original PACK fragment and placed actor/base against the installed
master, alongside any nested quest stage programs. Compiled-only fragments and
unsupported source commands still reject. PACK local-variable execution is not
added here.

The old blanket rejection of scripted packages is removed. Use Item At uses the
same boundary dispatcher, avoiding its previous separate OnBegin execution.
Unsupported topic actions and unframed procedure scripts remain explicit errors.
INAM procedure idles are queued on committed boundaries and loaded from the
actor’s original package stack. They share authored response-animation playback;
root motion and Must Complete idles hold locomotion for the clip duration.
Missing idle assets are logged. Dynamic script packages with uncached idle
assets, creatures and full animation/action parity remain limited.

## Occupied triggers

Acknowledged player overlaps now receive OnTrigger once per active gameplay
frame while the player still overlaps the enabled original primitive. The
existing enter/leave discovery remains on the 4 Hz resident-event cadence.
Continuous callbacks stop on exit, disable, cell/world mismatch, menus or
resident publication barriers. This remains player-overlap support, not general
NPC/object collision-trigger delivery.

OnTrigger uses the normal throttled script-save path rather than flushing a
save every frame. Trigger callbacks are issued once per application frame,
independently of stereo rendering.

## Original opening validation

The supplied ESM integration now drives both birth choices through name entry,
deferred appearance, the toddler movie request, walking to Dad, his departure,
opening the playpen gate, leaving the crib, SPECIAL allocation, Dad’s return and
Bible speech, and following him to the apartment exit fade (CG01 stage 90).
Quest progression comes from the unchanged original scripts. The test fixtures
supply player movement, successful voice completions, animation capabilities,
package arrival boundaries/poses and modal presentation. Arrival fixtures allow
the intervening original speech to finish; immediate invented arrivals would
skip those lines and leave their Say Once INFOs eligible later.

The real package fragments and their nested stage programs are matched against
the original ESM during this integration. Separate original-data NPC fixtures
run the production Travel/pathfinding code on the original Vault NAVM and verify
that Dad’s close-gate and Bible packages execute their authored OnEnd results.
These are localized arrival checks, not a complete headset traversal.

Synthetic checks cover package start/end/change effects combined with actor
callbacks, single delivery, rollback on an unsupported completion, package-only
actors, reload/reconstruction protection, idle dispatch and continuous-trigger
entry/exit/cell behaviour. The full player suite and Android native build are
run before publication. v313’s hosted Vulkan, Debug/O2 builds and movie package
verification completed successfully while this checkpoint was developed.

## Remaining opening work

This checkpoint stops at the apartment exit fade. It does not claim the birthday
handoff or leaving Vault 101. CG02 setup uses IgnoreCrime, which still needs
reusable actor/crime support. Birthday interactions, the G.O.A.T., Escape and
headset testing remain. Physical OnGrab/OnRelease script delivery for the teddy
bear tutorial is also still missing. Appearance editing remains deferred.
