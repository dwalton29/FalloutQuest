# M1B3p — reservation frame exclusion

Continues canonical main c7a1dce77de905f07d865d837210d2d26d37c6d1 (v212).
M1B3o Android CI passed (run 38797477661). No general resident Enable/Disable
caller is added by this checkpoint.

## Reservation boundary

The real resident asset lane now also excludes ordinary frame mutations. The
gate derives directly from lane ownership, with additional executing and fatal
recovery guards; there is no independently releasable frame ticket. Draining,
CPU preparation, incremental GPU staging, the prepared interval and cooperative
cancellation all remain blocked until the lane actually releases. Wrong-ticket
release/cancellation cannot resume gameplay. Fatal recovery remains blocked.

Acquisition requires the existing read-only idle-interaction check before it
reserves anything. The future caller must acquire before preparing the script
transaction, retain that transaction and packet, and use the existing canonical
and resource guards at commit. Frame exclusion does not replace those guards or
add ownership of the still-unimplemented outer workflow.

While reserved, the real OpenXR frame loop skips game-clock advancement,
script-location updates, quest/reference GameMode pumping, locomotion, UI input,
dialogue/barter updates, weapon updates, haptics, script-effect delivery, local
door updates, targeting and activation. Actor stereo preparation skips AI,
packages, actor persistence and unloaded scheduling. The loose-object bridge
also refuses physics/grab advancement. Targeting, activation and inventory
collection are independently guarded at the production interaction bridge.

The world-clock baseline is still sampled each rendered frame, avoiding a
reservation-length game-clock increment on release. This is mutation exclusion,
not a complete pause/rebase of every NPC, animation, weapon or physics wall-clock
deadline. That timing reconciliation and lifecycle interruption policy remain
outer-workflow work before command admission.

Head/controller sampling, player body tracking, view preparation and rendering
continue. Interior light-selection caches may refresh for rendering. Ordinary
asset entry points remain called so their existing reservation rules can park
results; worker completion and private CPU/GPU progress are not gated by the
gameplay branch. This checkpoint does not automatically drive the reserved job.

## Input and saving

Input sampling continues, but the reservation suppresses gameplay control values.
An allocation-free input barrier requires all sampled sticks/buttons/triggers/
grips to return to neutral before a subsequent frame can interpret new actions.
The release sample itself is ignored. Another reservation or a failed action
sample rearms that barrier. Tracking poses remain available to body rendering.

Flush requests during exclusion are deferred before WeaponPersistWorld or the
session save can run. The next valid unblocked gameplay frame services the
request. A failed save retains the existing log-and-return behavior rather than
retrying every frame. A newly installed Player session clears old deferred work.
saveBlocked/fatal recovery return before weapon persistence, including teardown
after reservation reset. Queued script effects stay queued during exclusion.

## Validation and remaining work

All fifteen world, 31 player and one physics CTest cases pass (47 core host cases).
The lane test covers repeated reservation/held/release/neutral/rearm sequences
with allocations disabled. The actual interaction bridge test confirms that
targeting, activation and collection leave Player unchanged and sound delivery
leaves its queue intact while reserved. Both changed host targets pass ASan/UBSan
with LeakSanitizer disabled. Structural guards check the actual native frame
branches, actor/loose-object guards, deferred save ordering and continued tracking
and streaming calls; these are not execution of an OpenXR frame on the host.

Original ESM regressions pass for Lucas/nonresident/barter dialogue, merchant
trading, all MS11 reward branches/save-reload and Brass Lantern load-door
resources. MS11 still logs the existing unsupported showmap fragment; this
checkpoint does not implement that command. Body, skin, Pip-Boy, shoulder,
opaque-path and render-dispatch source checks pass. Android compilation runs in
CI; no physical Quest or frame-time claim is made.

The outer coordinator still needs transaction ownership, authored/navigation/LOD
admission, resource finalization, parked-result reconciliation, timer/lifecycle
handling and command dispatch. Actor/held-object/local-door/UI/exterior resource
ownership remains limited as previously documented. General resident Enable/
Disable is not admitted. Existing v212 gameplay and bounded nonresident Disable
are preserved.
