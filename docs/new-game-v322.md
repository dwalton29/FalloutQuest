# v322: birthday control release, original photo and G.O.A.T. introduction

## Birthday freeze: unsupported local resource publication

Dad's final birthday INFO runs CG02 stage 12. That original stage restores
controls and enables RadioVault101REF in one transaction. The radio is a TACT
with XRDO transmitter metadata and a control-panel model in the same interior
CELL. The existing in-place admission accepts a narrower set of static/light
resources; the real radio reference fails its authored reference checks. A
mocked successful deferred commit in the previous test did not exercise that
limitation. Failed preparation could end the speech while leaving stage 10
and disabled controls intact.

When the existing fast publication path cannot handle a local script result,
the dialogue/world-script bridge can now commit it under the normal full-scene
loading barrier. It queues a rebuild of the current interior at the existing
scene origin. It does not insert a player MoveTo or an authored marker. A new
preserveView request does not request a player reset or a new arrival-facing
generation. Saved actor state, collision, visuals, lighting, doors and
other scene resources rebuild through the established scene loader against
the committed reference snapshot.

Admission still requires a ready, idle scene with no asset reservation, other
interaction, incoming actor preparation or pending move. A rejected queue rolls
the entire original result back and republishes the restored reference state.
If a deferred retry follows that rollback, it prepares a fresh transaction.
The in-place admission rules remain unchanged. No radio, quest, stage, NPC or
location identity is embedded in the fallback. This currently costs a loading
screen when an unsupported local resource changes; it is not a seamless
in-place update.

## Original photo readiness

GetHeadingAngle now resolves original/saved actor positions and scene yaw,
returning the signed clockwise offset in degrees. Player heading comes from
the tracked VR view, sampled together with script player position before speech
results and trigger evaluation. Missing view/location and different interiors
reject the query. Static callers return zero; coincident positions return zero.
Heading is transient input, copied into prepared transactions, checked for stale
commits and cleared on restore. There is no new save format.

The original photo trigger can therefore set PlayerReady when the player is
inside its authored volume and faces Jonas. DadReady and JonasReady still come
from their original package callbacks. The original quest drives the picture,
flash, inventory changes, six-year movie request and exam scene setup.

## Authored farewell on player dialogue exit

Pressing B now resolves the reserved GOODBYE topic through normal quest priority,
conditions, voice and transaction selection. It plays the farewell and executes
its results before ending. Repeated B cannot restart that line or drop a pending
world result; A uses the ordinary response-skip path. Missing/unsupported
farewells retain the existing exit fallback. Combat, loading and other external
interruptions retain their immediate cleanup behavior.

This matters for CG03: Dad's original farewell advances to stage 10, restores
controls and displays the exam objective. The runtime contains no CG03 exception.
Appearance editing remains deferred.

## Validation and remaining boundary

The production publication bridge exercises the real birthday final INFO with
scene refresh, failed queue rollback, deferred success and cancellation. The
original photo endpoint tests actor changes, exam setup and MoveTo/movie
publication together, including queue failure. Synthetic native B/A tests cover
farewell playback, repeat input and one-time result delivery; existing original
Lucas exit/reentry tests now wait for his farewell.

Both original-data New Game branches continue from target practice through
the actual photo trigger and package callbacks, image-modifier request,
six-year movie, inventory/book transfer, movie save/reload and suspension,
Dad's exam introduction and farewell, and control/objective restoration.
Actor arrival, voice completion and movie completion in that journey are
presentation fixtures. Host checks do not verify a headset playthrough or video
asset availability. The G.O.A.T. exam, hallway encounter and Escape remain
subsequent questline boundaries.

Semantics references: [GetHeadingAngle](https://geckwiki.com/index.php/GetHeadingAngle)
and [Conversation topics](https://geckwiki.com/index.php?title=Conversation_Tab).
