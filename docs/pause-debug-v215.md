# v215 — Original pause presentation and developer tools

Baseline: b637a1c16d9d27e7f1e17ab04c625c82d47fdcb4 (v214).
Version: 0.76.0-pause-debug-tools / 215. One consolidated main update.

## Controls and presentation

The left Touch Menu action binds `/user/hand/left/input/menu/click` on the
existing Oculus Touch profile. Right system input remains unbound. Right stick
up/down navigates, left/right adjusts values, A confirms, B backs out; left
Menu and Continue resume. Entry and exit require neutral controls, including
triggers, grips and both sticks. Unsafe loading, pending CELL transitions,
resident transactions/publication and blocked saves exclude debug mutations.
Focus loss dismisses the pause overlay and requires input release on return.

The source `Fallout - Misc.bsa` was inspected directly: `pause_menu.xml` uses
60-pixel spacing, a vertically centred list and right-aligned button edges;
`button_basic.xml` uses font 2, 40-pixel horizontal padding and a selection
rectangle. `globals.xml` supplies disabled-text brightness 128, selected-fill
alpha 32 / brightness 128, and outline brightness 200 (all out of 255).
`start_menu.xml` names `Interface\PauseScreen\PauseScreen01.NIF`.
The supplied FALLOUT.INI maps font 2 to `Monofonto_Large.fnt`.

The native renderer reproduces the menu on a head-facing stereo plane using
installed `Textures\Interface\Pause\pausescreen01.dds` and
`pausescreendarken01.dds`, the original Font 2 and HUDMain green. It does not
execute the original NIF's menu-specific animated scene/controller graph.
Missing assets leave a black backdrop and the existing original baked font;
no replacement art or Bethesda assets are bundled. Continue and Debug work;
Save, Load, Settings and Quit remain dimmed and inactive in their original order.
Original sContinue/sSave/sLoad/sSettings/sQuit GMST labels are used when present.
SOUN events UIMenuFocus, UIMenuOK, UIMenuCancel and UIMenuPrevNext use the existing
original audio resolver. Geometry/textures are prepared outside stereo eyes;
eye rendering restores GL state without rebuilding or decoding assets.

## Gameplay suspension

`Fo3GameplayBlocked` combines user pause, headset focus suspension and the
existing resident transaction gate. It does not change the resident gate's
meaning, save exclusion or background-worker ownership. Both steady and XR
gameplay clocks exclude paused intervals. Clock/calendar, GameMode scripts,
NPC AI/packages/animation, combat/projectiles, loose physics, doors, weapons,
movement and world interaction stop. Tracking, stereo rendering, loading,
resident lifecycle pumping and menu sounds continue. Raw OpenXR timestamps
remain in tracking/composition. Locomotion rebases on resume; masked held grips
cannot synthesize a weapon drop while input release is pending.

## Debug tools

* Set Time stages hour/minute and applies to the authoritative player clock.
  Earlier times advance one midnight with normal month/year rollover. It does
  not simulate skipped combat, scripts or NPC travel. On resume, schedules and
  exterior lighting observe the new canonical time. Restock timestamps retain
  their forward-only chronology.
* Add XP awards 100/500/1000 base XP through RewardExperience. Reach Next Level
  searches the same validated reward calculation for the smallest base award
  reaching the next threshold, including original entry-point modifiers. It
  never changes level directly. Pending levels use the existing skills/perks
  modal after closing pause. A ready pending level or level cap rejects another
  Reach Next Level request; the ordinary awards can still queue more levels.
* Add Items categorises the immutable original item records, sorts by name and
  FormID and shows ten per page. Eight hexadecimal digits are controller-editable
  for manual lookup. A selected item offers Add 1/10/100 and 10% condition steps
  for condition-bearing equipment. Player::Add enforces stack/instance limits;
  grants have no theft interaction or Karma event. There is no keyboard/search.
* Perk Points edits a separate extra developer selection budget (0–1000).
  Normal level-ups still award their ordinary perk; this budget does not replace
  pending levels or spend earned XP. Add rank spends one point; remove rank
  refunds one, with original PERK rank limits. Chosen/all filters and pagination
  expose original perk names, FormIDs and ranks. Debug selection bypasses level
  prerequisites but rejects additions whose effects the runtime cannot execute.
  Unsupported perks are labelled. Intense Training additions continue through
  normal level-up allocation; use SPECIAL to edit attributes directly. Removing
  a rank does not reconstruct/undo historical skill or SPECIAL allocations.
* SPECIAL edits all seven base attributes in a draft, constrained to 1–10.
  Apply uses SetPlayerAttributes and preserves existing skill allocations.
  An open, uncommitted level-up allocation is rebuilt after stat/perk edits so
  its INT/Educated budget and perk eligibility cannot become stale.
  Current HP/AP damage must fit the new maxima; invalid edits are atomic no-ops.

Every successful debug action uses existing Player APIs/revisions and the normal
FlushFo3PlayerState path. Highlighting, navigation and adjustments do not mutate
Player. Save versions 1–18 remain readable. A nonzero extra perk budget writes
v19 with DBP1 after the existing tails; zero-budget characters keep the existing
save version appropriate to their other state. Old application versions cannot
read new v19 saves. Source ESM/BSA/INI files are never modified.

## Validation

Focused production-model/admission tests cover invalid/forward clock changes,
original modifier-aware XP and pending levels, perk budget/rank add/remove,
unsupported rejection, staged SPECIAL, pagination, hexadecimal validation,
condition/quantity grants, no Karma change, persistence and held/focus/loading/
publication input handling. The optional Fallout3.esm argument exercises real
Swift Learner, item records, next-level XP, calendar and save/restore. A renderer
regression checks both eyes reuse prepared resources and restore GL state.
Resident clock tests cover overlapping menu/focus/transaction pauses.

Host tests cannot verify physical controller binding, headset comfort, original
texture appearance or audio playback on Quest. Those require on-device testing.
