# Authored conversations and interaction regressions — v304

## Moira's vault interview

The data-only MS03 test used a publication adapter that could commit the display suit's Disable. The actual adapter admitted nonresident removals and rejected a resident inventory display, so preflight removed all five vault answers from the live menu. The shared adapter now admits disabling unheld inventory references. Every affected reference must be a real inventory item and become disabled; enabling, actors, furniture, doors and held objects remain outside this path. No Moira/reference/quest ID is embedded in production code.

The immutable draw resources remain owned until normal scene retirement. The published reference snapshot, draw eligibility, item targeting, loose-body retirement and collision-removal overlay agree on the disabled item. It is not marked collected, and the original INFO remains responsible for giving the different inventory suit. Pickup refreshes preserve script removals. The production dialogue test now places the original display ARMO in the current scene and checks all five original answers, held-item rejection, real publication, exactly one gift and removal. Existing Lucas/barter lifecycle checks remain applicable.

## Megaton load doors

A door uploaded while the old live scene was the Wasteland could be looked up exclusively in the Wasteland index. An index miss then became a cached no-XTEL result and the door was classified as a local swing door. A miss now resolves the original generic XTEL before either result is cached. The shared cache is synchronized, and true swing-door misses remain cached: aiming does not scan the master. The production-function test verifies both positive and negative caching, a stale world context and the supplied original Megaton saloon XTEL.

## Chair camera

The avatar's eye anchor is expressed in scene coordinates, including FloorY and SceneForward. Its former conversion back to game coordinates omitted those origin offsets. The seating bridge now converts both offsets before applying the original skeleton pose. The original chair/skeleton/KF test checks a non-zero scene origin against the original game-space eye target and standing exit. This corrects the camera target; it does not invent a seat height or relocate furniture.

## Full-body response ownership

The supplied Radio_BangOn KF has no 180-degree root turn: its root rotation is identity and its small root translation is authored. Package movement and dialogue facing could resume while the response clip was still active after voice completion. A response KF that controls its accumulation root now owns motion through its actual duration. Package movement and procedural dialogue facing wait; combat/death can interrupt. The sampler preserves the response's original accumulation motion. Gesture-only KFs retain normal movement behavior. A production package test checks stationary motion through the ownership interval, resumption at completion and combat interruption. Headset confirmation of the reported Gob fault remains separate.

## Cromwell's prayer and reusable conversation flow

ConfessorCromwellSCRIPT already supplies the location checks, 160-second timer and StartConversation call. Its original INFOs use Next Speaker=Self and Link To; the previous structure filter rejected them. StartConversation now queues the authored selected INFO, and the shared speech session follows original links with Self/Target roles and executes each line's original results. Goodbye/absence of a valid link ends the conversation. Say/SayTo remain single-INFO operations. Next Speaker=Either and branching multi-party scenes remain unsupported. Medical Dialogue AI packages may also approach the actual player and start the normal player conversation; completed temporary script dialogue packages are removed.

The original-data production speech test runs Cromwell's OnLoad/GameMode and all 17 prayer INFOs, checks all 15 authored gesture references, the final Tick reset, voice completion ownership and prevention of an immediate repeat. It does not play audio on a headset. The existing original IDLE cache and response-animation bridge supply the preaching assets.

## Survival Guide continuation foundations

The current shared actor-value reader covers existing player SPECIAL/skills/resources plus RadiationRads and six condition AVs. Source GetHealthPercentage, DamageAV/RestoreAV, ResetHealth, dialogue GetAV conditions and supported instantaneous aid share canonical values. The existing saved Pip-Boy radiation field is reused. Limb damage, temporary player-control restrictions and restraint have an optional v26 save tail; old v1–v25 saves remain readable and unchanged when no new tail is needed. Healing/restoration rolls back with a failed script transaction. Controls use the original seven argument positions; tracked head/hands and the system pause action remain live as a VR adaptation.

These foundations are not a claim that the remaining eight MS03 tasks are fully playable. The unchanged original Super-Duper Mart route remains covered. The remaining quest dependency map is in survival-guide-remaining-work.md.

## Verification boundaries and headset checks

Local checks: 48 player tests passed with one optional MQ01 data test skipped; that original MQ01 test passed separately. The NIF suite passed 18 tests with six optional asset tests skipped. The original MS03, bomb outcomes, Lucas/Moira production bridge, Cromwell production prayer, original chair/origin, original saloon lookup and source-publication/render guards passed separately.

Host tests, original-data tests, Android compilation, Vulkan validation and physical play are distinct. Use the matching Actions run for native build outcome. No physical headset is attached to this session.

1. Start a fresh Moira vault interview with the displayed suit still enabled. Check all five replies, the display removal and single gift; leave/reload and check it stays removed.
2. Enter Megaton from the Wasteland, then enter/leave Moriarty's Saloon and other buildings. Load doors must transition; genuine local swing doors must still animate.
3. Sit at a chair near a terminal. Check camera height/facing, hand tracking, terminal access, stand and re-enter the area.
4. Watch Gob's radio-banging response through voice completion. His body should stay in place until the full-body animation ends, then resume its package.
5. Approach Cromwell while he is at his original prayer marker. Check the complete prayer, gestures and ambient voice. Walk away, reload/return, interrupt with dialogue, and check for overlapping or stuck speech.
