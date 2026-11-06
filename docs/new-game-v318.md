# v318: original intercom conversation and BB-gun gift

Extends the original opening through the automatic diner intercom conversation,
Dad's invitation downstairs, Jonas's greeting and the BB-gun gift. No quest IDs,
stage advances, dialogue text or NPC routes are embedded in the runtime.
Appearance editing remains deferred.

## Talking activators

SetTalkingActivatorActor accepts a placed talking activator and an optional
original actor reference. Omitting the actor restores the activator's ordinary
identity. Assignments are per reference, bounded, transactional and saved in
the optional v33 TAP1 tail. Versions 1–32 remain readable. The assignment
survives a save taken while the intercom is waiting for its conversation.

Dialogue context separates actor identity from the physical activator endpoint.
Original GetIsID/voice selection and INFO result ownership use the assigned
actor; position comes from the intercom. Disabled activators and dead assigned
actors cannot resolve a playable proxy. The command semantics follow the
[GECK function documentation](https://geckwiki.com/index.php/SetTalkingActivatorActor).

## Specified-topic package conversations

NPC-targeted dialogue packages with Conversation type and a specified original
topic can now bind that topic through the existing linked INFO/voice pipeline.
The local navigation target can be an enabled, assigned talking activator in
the resident scene. Its actor may remain in another room. No NPC mesh or
physical relocation is fabricated for the intercom speaker.

Live audio tracks physical endpoints and logical identities separately while
the linked conversation swaps speakers. Remote speech does not animate the
assigned actor's distant mesh. Speaker flags clear on completion/interruption.
Package completion belongs to the initiating actor, even when another actor
speaks last. Queued dialogue binds both actor identities; changing an assignment
before delivery cannot give its queued line to a different actor. Missing audio
cannot manufacture successful INFO/package completion.

Interactive player-targeted specified-topic packages, direct player activation
of assigned talking activators, and nonzero PKDD control flags remain unsupported.
The implemented route is the automatic NPC conversation used by the diner scene.
General attenuation and talking-activator NIF/LIP animation parity are not claimed.

## Once Per Day package scheduling

Dad's higher-priority Travel to the intercom uses the original Once Per Day
flag. The runtime now records its first selection time per actor/package and
keeps a running package eligible while it is executing. After completion the
package cannot be selected again for 24 game hours from that start time. This
matches the [GECK package flag semantics](https://geckwiki.com/index.php/Category:Packages).

Completed daily Travel packages retire through their real OnChange boundary.
Must Complete end idles still finish before retirement. This allows the lower
priority intercom conversation to be selected instead of leaving Dad on the
completed travel package. It also prevents immediate reselection of daily cake
packages. Cooldowns persist in the v33 OPD1 tail alongside activator assignments;
failed package-start transactions cannot publish their cooldowns.

## Validation and next boundary

Both original-data New Game sex branches now continue through birthday presents,
the original failsafe timer and intercom assignment, the original linked intercom
dialogue, assignment clearing, Dad's departure invitation, Jonas's greeting and
the original BB-gun/ammunition gift. They check restored fighting controls and
the unlocked range door. The journey never injects stage commands. Voice/movie
completion, package arrival and player movement remain presentation fixtures.

The production package admission code is compiled and tested for mapped-activator
targets and one-time specified conversation selection. Production speech tests
check speaker swaps, proxy identity, stale assignment refusal, actual audio EOF,
end results, package-owner completion and remote-mesh isolation. Separate tests
cover save/reload, preview, rollback, clearing and invalid bindings.
An original NAVM/package test checks Dad's Travel-to-conversation selection;
synthetic tests cover cooldown isolation, completion, save/reload and the full
24-hour boundary.

Remaining opening work includes range target hit delivery, the radroach/photo
sequence, aggressive Butch branches, physical grab/release events, G.O.A.T.,
Escape and headset verification. Receiving the BB gun is this checkpoint's
endpoint; the complete Vault exit is not yet claimed.

The preceding APK build failed because the cell-handoff public header was first
included inside an anonymous runtime namespace, creating a conflicting nested
fo3script namespace. The core now includes it at global scope before internal
runtime includes. A source-scope regression check covers that integration order.
