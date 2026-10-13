# v207 lockpicking evidence and integration

## Dependency assessment (before runtime integration)

Main a4508d5 already implements v206 theft and Karma. No lockpicking minigame,
mutable lock overlay, XP progression, or broken-lock query exists. XLOC currently
retains only presence/key. Door and container eligibility share CanActorOpenDoor;
NPC navigation also calls it. Container transactions, inventory quantities, saved
Karma and feedback already have canonical Player APIs. Scripts copy State during
preview/rollback. New locks belong in that State, never in scene objects or UI.

The UI can decode original rigid NIF hierarchy and embedded controller sequences;
loading screens already upload original DDS materials. Audio resolves directory
FNAM variants from loose files and archives on its worker. A dedicated modal loop
channel is required for tension without touching radio/music. Modal focus must
also gate locomotion, snap-turn, wrist Pip-Boy, weapon inputs and loose grabbing.
XP earning can be persisted; level-up/skill-allocation is currently absent and
must not be represented by a fabricated level-up notification.

## Primary evidence

User-supplied Fallout3.esm, Fallout3.exe, Fallout - Misc.bsa, FALLOUT.INI;
Dropbox lockpick_menu.xml, both NIFs, both DDS textures and all 46 lockpicking WAVs
were inspected locally. None are shipped in the repository/APK.

XLOC: level byte 0, key uint32 at 4, flags byte 8 (bit 2 leveled), reserved bytes
1–3/9–11, optional opaque bytes 12–19. Both 12 and 20 byte records are retained.
Original levels include 0,7,20,25,40,50,75,80,99,100,255. Classify using the upper
bounds 0/25/50/75/100, not equality or object names. 255 requires a key.
Leveled locks remain explicitly unsupported for picking pending formula recovery;
keys continue to work. Immutable metadata is preserved for later support.

Bobby Pin is MISC 0000000A, EDID Lockpick. Infiltrator PERK 00044CB0 entry point
32 Ignore Broken Lock, float 1; original description permits one extra attempt.
No development pins or perks are granted.

Executable force chance 0x601AD0: clamp(trunc(skill)+trunc(fLockSkillBase)-tier,0,100).
Base default 10. 0x941F29 samples 0..99 and succeeds strictly below the chance.
0x941FC7 increments the reference force-failure counter. Successful force enters
the same 90-degree success path as manual picking; force does not consume a pin.

0x941A00 creates sweet/concentric regions. Sweet defaults 400/200/100/50/25;
concentric defaults 200 at all tiers. Min/max interpolate by skill over a two-tier
span. Vanilla min=max. 0x942A6A rotates 90 degrees per second times torque;
0x94292F releases at 90 degrees per second. 0x942E61 damages pin health at
1/fPickBreakSecs per second while binding (default 1 second total). It persists
between releases, resets for a replacement; removal of one real pin occurs only
at zero. 0x943DB0 drives the active original lock controller by rotation/90.

0x942BA1 awards XP only if the reference pick reward counter is zero; force uses
this path too. Fallout3.esm rewards 20/30/40/50/60 override executable defaults.
Keys do not award XP. 0x942BDD–0x942C7C checks NPC/faction owner alignment and
applies fKarmaModStealing (-5) on successful owned unlocking. Failed attempts,
viewing, cancellation and key use do not enter this branch. Subsequent item theft
continues through v206 independently. No witness is invented.

Original XML: lower-left 400x175 information box; font 7; Skill/Bobby Pins/Lock
Level rows at y35/80/125. Lower-right Force Lock (Xbox X), Exit (Xbox B), status
at y450. Debug detect-meter is hidden. Original NIF lockinterface01 has frame,
lock and screwdriver with Forward/Backward controller tracks; bobbypin01 has
bobbypin/Pin2 with Forward/Backward/Left. All geometry uses lockinterface01 DDS
and its normal map. XML UI proportions are retained on a stationary VR panel.

## Explicit adaptations / incomplete engine dependencies

The logical detect meter uses 1280 units across 180 degrees; six discrete stop
bands mirror the decoded executable, but exact cross-resolution engine layout
and extreme outside-region interpolation are not fully recovered. VR pin input
uses 80 degrees/second thumbstick motion; torque uses trigger pressure. Hidden
position uses runtime entropy rather than Bethesda's unavailable RNG sequence.
Skill interpolates the original min/max settings; vanilla min=max, so no additional skill bonus is invented.

Leveled lock picking, guarded crime pursuit restrictions, complete witness
reactions and level-up allocation remain unavailable. The persistent earned-XP
counter is real state; no fake notification or automatically granted skill/perk
is used. Original full-screen menu compositing and normal-mapped lighting need
headset review; the world-space panel is a VR adaptation.

## Implemented controls, presentation and feedback

Right stick X positions the pin; right trigger applies torque; release returns
cylinder tension to zero before repositioning. A selects Force Lock, B exits.
Entry requires A/B/trigger/stick release. Exit retains a weapon/grab input latch
until A/B/trigger and both grips are released. Focus loss cancels the transient
session and stops the dedicated tension loop. Locomotion, snap-turn, wrist
Pip-Boy, new weapon input, loose grabs, world activation and dialogue selection
are suppressed while the modal is active; tracking continues normally.

The authored lock Forward intro is sampled on entry. Backward contains the
0..90-degree lock and screwdriver motion and is sampled by cylinder fraction.
Pin Forward supplies insertion, Backward supplies accumulated tension bending,
and Left supplies committed breakage before replacement. The pin's independent
Y-axis orientation is driven by VR input. Both eyes draw the same logical state;
only the once-per-frame updater transfers items, mutates locks or emits sounds.

The original font 7 is Baked-in_Monofonto_Large.fnt (FALLOUT.INI). Information
rows, original TAI fade/solid bracket sprites, A/B button artwork and Main HUD
colour (26,255,128) use the existing HUD resources. A replacing original Xbox X
is a Quest input adaptation. The EXE format at 0x9417FA is `Force Lock [N%]`.
The panel captures the entry head pose at 1.4 m and remains stationary. Its
1.28-by-.72 m logical layout and text scaling are VR adaptations, not new art.
The existing UI mesh lighting path uses the original diffuse DDS; the inspected
normal DDS is not yet applied by this renderer. Untextured authored shadow
geometry is omitted rather than supplied with invented artwork. Original
full-screen compositing, depth/proportions, lighting and stereo comfort still
need physical Quest validation. Unlocking currently returns to world interaction;
a subsequent A activates a picked/forced door or accesses the container.
Keyed doors retain immediate existing activation, with persistent key-unlock
state. Keyed containers first commit unlocking without collecting a loot row.

SOUN resources (all paths prefixed `sound/` at runtime):

| EDID | FormID | Original FNAM |
|---|---|---|
| UILockpickingEnter | 000C5C22 | fx/ui/lockpicking/ui_lockpicking_enter.wav |
| UILockpickingForceFail | 000C3398 | fx/ui/lockpicking/ui_lockpicking_forcefail.wav |
| UILockpickingUnlock | 0004589F | fx/ui/lockpicking/unlock/ |
| UILockpickingPickMovement | 00045B3C | fx/ui/lockpicking/pickmovement/ |
| UILockpickingPickTensionLPM | 000384AC | fx/ui/lockpicking/picktension/ |
| UILockpickingPickBreak | 00044393 | fx/ui/lockpicking/pickbreak/ |
| UILockpickingCylinderTurn | 0003F5C6 | fx/ui/lockpicking/cylinderturn/ |
| UILockpickingCylinderStop | 0003F5C7 | fx/ui/lockpicking/cylinderstop/ |
| UILockpickingCylinderSqueakA | 00045B3A | fx/ui/lockpicking/cylindersqueak/a/ |
| UILockpickingCylinderSqueakB | 00045B3B | fx/ui/lockpicking/cylindersqueak/b/ |

The worker resolves original directory variants, uses SNDX attenuation through
its existing gain conversion, and logs canonical missing paths. Enter, break,
stop, unlock and force-fail are transition events. Tension uses a dedicated
looping MediaPlayer; it cannot replace ambience, music, radio or dialogue.
Movement/turn/squeak are limited to one event per .18 seconds; this cadence and
the health-based A/B squeak choice are explicit runtime approximations pending
complete original event-scheduling recovery. Original pitch randomization is
not implemented. Haptics are VR-only 20 ms pulses: .08 resistance, .12 binding,
.22 success, .28 force failure and .30 pin break. They do not determine outcomes.

## State, scripts and migration

FQPS v14 appends earned XP plus sparse eight-byte lock overlays after the intact
v13 ownership table. Each entry has original reference ID, change reason,
resolved level, force-failure count and one-time XP-reward flag. Key, picked,
forced, broken, script-locked and script-unlocked states are distinguished.
One Infiltrator allowance permits retry after the first force failure; a second
failed force remains broken. The perk is never granted automatically. Keys can
unlock broken locks. Cancellation never clears broken state or writes success.

The shared GetLock API drives CanActorOpenDoor and therefore local/XTEL doors,
NPC navigation and container access. Original definitions never mutate. Source
script queries GetLocked/GetLockLevel/GetIsLockBroken and Lock/Unlock mutations
use that same overlay. Script preview and failed transactions roll back State,
including lock changes. Optional unsupported script arguments or leveled queries
fail safely; compiled-only bytecode is not executed as source.

v1–v13 saves retain their existing decoding. v13 migration starts locks from ESM
and earned XP at zero; no inventory/Karma/ownership migration is invented. The
v14 decoder rejects unknown IDs, duplicate lock entries, invalid change bytes,
force counts, flags, impossible picked/forced states and malformed lengths.
Restore validates into a temporary State before replacing the live player.
Save retains the existing checksum, temporary-file write, fsync and rename.
A failed write retains the preceding committed save; in-memory changes remain
dirty for the existing save lifecycle. Effects are queued only after committed
player operations, never after failed checks. Interrupted minigames are transient.

XP is a persistent earned-experience foundation with original base rewards and
one reward per reference. Complete level-up allocation, XP perk modifiers and
any additional difficulty modifiers are not implemented. No disconnected XP
or level-up notification is displayed. Existing Karma state/feedback is reused.
Crime events distinguish Take, PickLock and ForceLock and retain original owner,
reference, CELL and available location. Witnesses, arrests and detection remain
unresolved; nearby NPCs are not fabricated witnesses.

## Validation and Quest procedure

Host tests cover original difficulty categories, access gates, mechanical
travel/binding/durability, real pins, force rolls/breakage, perk/key behaviour,
actual door/container access, crime/Karma, script rollback/queries, no duplicate
XP and v13 migration. Headless modal tests exercise actual runtime entry/exit
latches, held inputs, focus loss, one-shot audio/save emission and replacement.
They do not substitute for GPU/headset tests.

Original-data integration identifies Megaton references from CELL and record
metadata rather than production exceptions. This supplied master selected door
00003A6F and container 0001FF1A (both Very Hard). An explicit test-only character
has Lockpick 100 and three real pins. The two unlocks earned 120 base XP and
changed Karma by -10; owner identity, loot accessibility and saved locks survived
reload. No fixture inventory or skill is added to production player state.

Quest checklist (not yet executed on a physical headset):

1. Ensure the original five interface files and 46 WAVs are present under the
   user's game installation; confirm the player's real skill and pin count.
2. Approach a supported locked Megaton door/container. Check low-skill,
   requires-key and no-pin messages; owned targets must retain red text.
3. Enter, release controls, inspect original lock/screwdriver/pin in both eyes.
   Confirm stationary comfortable placement seated and standing.
4. Move stick X, press/release trigger, feel binding, deliberately break a pin.
   Check one pin removed, original break animation/sound, and replacement count.
5. Pick successfully. Activate the door/loot panel with A. Check original local
   animation or XTEL destination, intact contents and owned red theft rows.
6. Test successful force and failed force on separate eligible locks. Verify
   failure remains broken, only the real Infiltrator perk permits the extra
   allowance, and a correct key remains usable.
7. Restart and revisit: unlocked/broken state, pins, earned XP, stolen contents
   and Karma must agree. Attempt repeated A to check duplicate prevention.
8. Cancel, lose/regain OpenXR focus and hold trigger on exit. Check no movement,
   snap-turn, weapon firing, grabbing, dialogue or wrist-menu input leaks.
9. Check original audio/haptics and radio continuity, then regress weapons,
   Pip-Boy, NPCs, dialogue, streaming and ordinary unowned looting.

Local validation: 48 host CTest cases passed, two existing external NIF fixture
cases skipped (50 registered across player/assets/world/physics/NIF/audio).
All ten render source/shader Python test files passed. The lock UI header passed
a syntax check against real GLES declarations. Original ownership and new
lockpick integrations passed against the supplied master. Android/Java/native
link validation and APK production are delegated to the canonical GitHub build;
the run result is reported with the delivered APK. No physical headset test has
been performed. The skipped NIF fixtures concern player-body/combat animation
assets, not fabricated successful lockpick visuals.
