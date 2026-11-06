# v317: opening presentation and full-cell script handoff

Addresses reported headset failures in the birth opening and the shared dialogue
pose behavior. Appearance editing remains deferred.

## Speaking and furniture

Starting dialogue no longer requests a furniture exit. The package executor
retains the selected furniture during dialogue; combat still requests an exit.
Seated actors keep their body facing while dialogue can retain its head look.
The existing furniture pose sampler continues to supply the seated/lying pose.

Package idles now retain pose ownership after a clamped clip ends, until their
package changes or an explicit response animation replaces them. A spoken line
without a speaker animation no longer clears the active package idle. Authored
loop/clamp playback and lip updates remain in use. No synthetic animation is
introduced. A missing decoded clip cannot manufacture a pose.

## Birth-room handoff

The birth ending can disable live references and request a player MoveTo in the
same original transaction. Previously the fast publication path rejected live
reference removal and sent this to in-place resident resource preparation,
even though the destination requires a full new scene.

Birth and toddler markers actually share an interior CELL. The generic scene
handoff now admits a new prepared MoveTo whose marker's
original cell/world agrees with the pending destination. It commits canonical
state and synchronously queues the normal loading barrier, then publishes the
candidate reference snapshot. The scene loader rebuilds the destination against
that snapshot and acknowledges the pending move on arrival. It does not rewrite
stages, invent a destination or patch the birth quest. Busy scene/asset work
still blocks admission. Queue failure rolls the transaction back and republishes
the restored reference snapshot; side effects are not delivered on failure.

Tests exercise the production dialogue publication adapter with live actor
removal plus a different-cell move and the original same-cell birth move,
loading admission, rollback on queue failure
and reference-publication revision agreement. The ESM journey still covers both
sex branches through the birthday party. Headset verification remains required;
the previous host journey used immediate logical commits and did not exercise
this live scene boundary.

## Body, Pip-Boy, compass and movie presentation

While original scripts mark character generation and disable Pip-Boy control,
the VR body/hand/Pip-Boy render pass and compass/sneak overlay are hidden. Existing
Pip-Boy input restrictions already follow that control state. Original control
restoration brings presentation back; quest IDs/stages are not embedded in the
render policy. Birth and later birthday availability are checked in the ESM test.

Native Bink playback still requires original Data/Video assets. If a requested
file cannot play, the failure panel now titles itself from the original movie
filename (for example, `1 year later`) and explicitly says to press A to continue.
This is a text fallback, not replacement artwork or a claimed video playback fix.
The full-cell load completes before movie presentation and subsequent GameMode
remain suspended until movie completion or the explicit continue action.

Validation includes production pose sampling past clip end, furniture ownership
through dialogue, scene-handoff rollback/publication, the original ESM journey,
and the host/NIF/source suites. No headset was connected during development.
