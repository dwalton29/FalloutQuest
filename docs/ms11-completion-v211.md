# v211 — original MS11 completion

Baseline: main `9c79c073b47102c2c9f58dfa0432e080b036502f` (v210).
Version: `0.74.0-ms11-completion`, versionCode 211.

## Failure and original chain

Eligible Lucas GREETINGs were rejected during transactional preflight. The
outer end result could not compile SetCellFullName; the nested stage 210 also
needed CompleteAllObjectives, RewardXP, the two-argument ModDisposition,
GetInSameCell/Disable, and AddAchievement. Eligibility is still prevalidated;
no completion option, reward amount or stage progression was fabricated.

Original INFOs:

- `0001501D`: disarmed before talking to Lucas. Begin sets MS11.SimmsBonus=1.
- `00095AA4`: previously accepted with payment (SimmsBonus>=1).
- `00095AA5`: previously accepted without payment (SimmsBonus=0).

All require Lucas base `00000A60` and MS11.Disarm=2 or 3. Authored ordering,
quest priority and say-once flags remain in the existing selector. Their end
results set Disarm=4, execute MS11 `00014E9E` stage 210, start HD01 `00040691`
and HD00RobotsDialog `0007F47A`, and rename CELL `000151E3` from MESG `0009A1E6`
FULL (`My Megaton House`). Native command-table metadata confirms the second
SetCellFullName argument is a Message, not another CELL.

Stage 210 completes objectives, awards 300 base XP, assigns original GNR quest
variables, grants 0/100/500 caps for SimmsBonus 0/1/2, adds key `000151E4` and deed
`000C402B`, modifies Lucas disposition towards the player by +75, stops MS11,
conditionally disables Burke and sets MS11Fin.moved, and records achievement 23.
The original stage item's completion flag finishes the quest. No ownership
command exists in this branch: the source house CELL has no XOWN override;
key-based door access uses the existing inventory/lock policy.

## Shared engine support

New result operations reuse the same private Player candidate and bounded
nested-stage runtime. SetCellFullName persists a CELL-to-MESG override without
changing immutable ESM data. CompleteAllObjectives preserves each objective's
displayed flag and leaves quest running/completion to separate commands. The
executable's CompleteAllObjectives body at VA 0x775690 preserves flag bit 0 and
sets bit 1, corroborating that distinction.

RewardXP updates canonical earned XP, including supported rank-specific numeric
Modify Experience Points perk effects (entry point 9). Real Swift Learner ranks
produce 330/360/390 XP for the original 300 base award. Conditional/scripted
entry-point effects reject rather than pretend success. Pip-Boy STATS shows
saved earned XP. Source AddItem results queue ordinary item notifications only
after transaction publication; previews, rollback and restore cannot replay them.
Existing objective/completion fanfare and dialogue audio/LIP/UI remain in use.

Save v17 appends validated CELL/MESG names and local achievement IDs after v16.
Earlier save layouts remain readable and saves without new fields retain the
previous schema. XP, stages/history, objective states, variables, inventory,
disposition and reference enable states reuse their existing save fields.

## World publication boundary

Disable is staged in the existing ScriptTransactionPlan. Direct Player execution
still rejects world changes without a publication owner. Dialogue has separate
read-only preview and commit hooks. The production hook supports only disabled
off-scene actors: affected references must be actors, disabled in the candidate,
without enable-parent children, outside the current CELL/world and absent from
live actors. Publication also checks current scene stamps and background-work
exclusion. Logical state and immutable reference snapshot publish together in a
single render-thread turn, with allocations before commit. No CELL reload is
used. Disabled actors are excluded from scheduler, arrivals and future scene
loads.

If Burke is in the player's CELL, the original script does not disable him.
If he is outside it, the tested nonresident path executes his removal. Resident
removal, static changes, enable-parent descendants, fade arguments and broader
resource exchanges remain rejected; they need the unfinished general resident
coordinator. No supported quest branch silently ignores that dependency.

## Validation and limits

Original Fallout3.esm tests cover original Lucas introduction and acceptance,
bomb OnActivate/GameMode disarm to stage 30, automatic completion GREETING,
all three authored payment conditions, stage 210, objectives, caps/XP/key/deed,
key-based doors, house naming, house quest activation, disposition, both Burke
location branches, save/reload and one-time stage/greeting behaviour. A separate
real-data case disarms before the first Lucas conversation and selects 0001501D.
Invalid pre-disarm progression is gated. Existing original bomb tests cover
insufficient skill and failed activation. Synthetic tests cover immutable
previews, resident rejection, commit/rollback, unsupported-command failure,
malformed v17 data, and v14 migration. All three original Swift Learner ranks
are checked.

Host regressions: 31/31 player and 12/12 world tests pass after rebuilding
all inherited objects. Rendering/font/Pip-Boy/notification and script-audit
source checks pass. Android build outcome is reported with delivery. No
physical Quest test was performed. Level-up skill/perk allocation is not yet
implemented by FalloutQuest; earned XP persists but this change does not claim
that UI. AddAchievement records the original event locally, without Xbox/GFWL
unlock integration. Effective disposition/speech challenge parity, house
furnishing/robot scripts and delayed MS11Fin behaviour remain their respective
broader engine milestones; starting their original quests is not proof of all
those scripts working.

## Headset acceptance

Install v211 over the current APK. Load an MS11 stage-30 save (or accept and
disarm the bomb), locate Lucas and initiate dialogue. Hear his authored
acknowledgement; finish or skip through every response. Check completed MS11 in
DATA, original caps/key/deed in ITEMS, XP in STATS and `My Megaton House` on its
load-door prompt. Open the house, save, exit and restart. Confirm persistence
and that later conversations cannot repeat rewards. Report any refusal along
with the `DIALOGUE UNSUPPORTED SCRIPT info=...` diagnostic.
