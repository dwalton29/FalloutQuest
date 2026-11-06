# v319: dialogue motion and birthday control handoff

Addresses the reported opening animation and birthday progression problems in
shared runtime code. No quest stages, actor positions or character routes are
added to the runtime. Appearance editing remains deferred.

## Motion and looking

The dialogue-facing update previously ran after package steering and replaced
its walking animation with a standing idle. When an actor is moving, navigation
now retains body facing and locomotion; dialogue/scripted looking can only
adjust the head. Stationary actors honor explicit script Look targets instead
of returning to their previous yaw. This affects the original birthday guests'
Look-player commands and gives their stationary turns the existing turn clips.
It does not teleport actors or rewrite their original placements.

Repeated responses requesting the same IDLE retain its playback start time.
A response without an animation preserves an existing package pose or a
response with authored clamped playback or accumulation-root motion. These poses retain
their authored playback/terminal posture while stationary; movement releases a
response pose, and a different available IDLE replaces it. Missing clips cannot
clear a working posture. Existing sound-key and lip playback remain separate.

Pose sampling now binds track and property bone indexes to the actual clip and
skeleton identities. Previously, returning from a response to a base clip with
the same number of tracks/properties and accumulation-root name could reuse the
response's different bone mapping. This could drive the wrong bones and stretch
the skinned model. Regression tests switch equal-sized clips with different
bone targets, then switch skeletons.

## Birthday world result

The original final party-intro INFO advances CG02 to 12. That single result
restores movement controls, enables the Vault radio reference (a control-panel
world model in the same CELL), updates objectives/tutorials and requests autosave.
The previous live dialogue fast path could not prepare that resident model, so
it rejected the whole result and left controls disabled. Data-only journey
fixtures had committed the state directly and missed this boundary.

INFO end results that require resident resources now pass their exact prepared
transaction to the existing asynchronous resident workflow. Dialogue enters a
Results phase until that ticket has committed and finalized. The normal world
reservation, CPU/GPU preparation, original-program evidence, publication,
rollback and side-effect gates remain in force. This is not a logical-only
Enable shortcut. Immediate removals and full-cell handoffs retain their current
fast paths. INFO begin-result deferral remains unsupported.

After successful finalization, dialogue records completion and resumes its
original linked line, choices, goodbye or package completion. Repeated EOFs
while waiting cannot run results twice. Transient resource contention retains
the closing INFO and retries admission after the background work finishes;
selected scripted speech is likewise kept queued while admission is busy. Failed/cancelled publication cannot
record INFO success or complete the initiating package. Scripted speech,
interactive dialogue and NPC conversations use the same completion protocol.
No controls are restored unless the original transaction succeeds.

## Validation and limitations

The original birthday final INFO is exercised with the production dialogue
publication adapter, the original same-CELL radio reference and locked player
controls. It first proves the old fast path rejects the world change, then
checks pending, successful and failed publication. Production scripted-speech
tests separately verify that pending results retain their speaker and package,
and only a committed result can complete that package.

Animation tests cover retained response clocks/postures, missing-IDLE behavior,
walking/facing ownership, script-driven stationary turns and cross-clip bone
mapping. Existing birth/intercom, furniture, actor skinning, world event and
quest regressions remain required. There is no connected headset: actual birthday
blocking/collision and visible deformation must still be checked in headset.
Range targets, the photo sequence, G.O.A.T. and Escape remain later work.

The original CrouchTalkA KF and humanoid skeleton were also decoded locally.
CrouchTalkA is clamped and does not animate the accumulation root: retaining
root-owning clips alone is insufficient. Its terminal crouch is therefore kept
by its authored playback mode until replacement or locomotion. Every pair of the original idle, kneeling, birthday Dad cheer/talk and Pip-Boy
gift clips was sampled against a freshly bound pose. The corrected binding
matches exactly; simulating the former size-only cache produced a maximum
matrix error of 89.2875. Optional original-asset animation tests cover these
cross-clip transitions and distinguish separate authored prop tracks from
humanoid bones. Original clips remain outside the repository.
