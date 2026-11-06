# v323: original G.O.A.T. exam and tag skills

This checkpoint continues from v322's exam introduction. It implements shared
runtime capabilities needed by the original CG03 classroom trigger, teacher
script, slideshow activation, ten questions, results dialogue and optional
skip-and-select route. Runtime code contains no CG03 IDs, stage routes, actor
positions or replacement quest scripts. Appearance editing remains deferred.

## Script commands and expressions

`SetPlayerTagSkill` assigns an actor value to one of four tag slots.
`IsPlayerTagSkill` reads those slots. `SetTagSkills` opens an editable selection
with the authored total and optional existing-selection flag. Throwing is
excluded from the selectable Fallout 3 skills. The original `fAVDTagSkillBonus`
setting supplies the grant; duplicate slots cannot stack it. The actual grant
at the skill cap is recorded so replacing tags restores the previous base.

Expressions now support multiplication and division with ordinary precedence,
division-by-zero rejection, and `GetRandomPercent` (integer 0–99). Script draws
belong to the transaction candidate, persist after commit, and roll back with a
failed fragment. Syntax/provenance preflight does not consume random draws.

`Activate Actor 1` runs the target's original OnActivate synchronously inside
the same transaction, with the specified action reference. `IsActionRef` can
compare original actor references. Nested activation has a bounded call depth;
a failed child rejects the parent. The existing flag-zero default activation
path is retained. This lets Brotch execute the slideshow script through its
authored activation rather than silently losing INFOs which use that command.

Original-master matching now admits a nested OnActivate context only when its
active parent contains the matching target, actor and flag-one activation.
The child still authenticates its original SCPT body and owner/base association.
Stage children continue to inherit the parent's implicit reference/activator.

`SCAOnActor`/`StopCombatAlarmOnActor` clear attackers targeting the specified
actor, through canonical state and the live combat adapter. Unrelated combat
targets remain active. This supports the original classroom stage cleanup.

## Sitting observations and selection UI

The world adapter samples resident NPC furniture state before its cached asset
registration return. GetSitting, GetCurrentFurnitureRef and GetFurnitureMarkerID
read that observation, including authored marker reference and entering,
sitting and leaving states. Off-scene observations expire; unknown actors are
not reported as seated. Plans prepared against an older observation cannot
commit. Player furniture queries retain their existing physical seating path.

Tag selection uses the existing original MessageMenu presentation, original
skill labels and the Quest controller scroll/A input. It requires the authored
number of selections before Done. It owns modal input and pauses GameMode;
held opening buttons and tracking/focus restoration cannot select a skill.
It does not reproduce the dedicated flat-screen CharGen menu layout.

## Save compatibility

Optional save version 34 stores tag slots, actual bonuses, an unfinished tag
draft, the committed script random state, owned message identities/format
arguments, and unconsumed button results. Displayed questions are reconstructed
from original MESG definitions after restore. A submitted answer is delivered
once rather than showing the question again. UI state resets on runtime
generation changes. Existing versions 1–33 remain readable; ordinary states
without the new fields retain their previous save format.

## Validation and limits

Synthetic checks cover tag caps/replacement, rollback, input edges, save/reload,
random preview isolation, arithmetic, nested activation, master matching,
stale sitting plans and native furniture/combat/message/selection adapters.

The optional original-data journey tests both sex branches through the birth,
baby, birthday, photo and exam introduction. A separate CG03 boundary fixture
starts after the hallway introduction, then executes the original teacher
dialogue, classroom trigger, package callbacks, all ten question messages,
slideshow activation, scoring, results dialogue and tag selection. It also
checks the skip route. The exam saves/restores while a question is displayed
and after an answer has been submitted. Original program matching covers the
nested slide event and classroom trigger.

NPC arrival/seating, animation capability registration, voice completion and
movie completion are host presentation fixtures. These checks do not establish
a headset playthrough, verify original slide/video asset availability, or
validate the hallway encounter and Escape end to end. Those remain the next
questline boundary.

Semantics references: [SetPlayerTagSkill](https://geckwiki.com/index.php?title=SetPlayerTagSkill),
[SetTagSkills](https://geckwiki.com/index.php?title=SetTagSkills),
[Activate](https://geckwiki.com/index.php?title=Activate),
[GetSitting](https://geckwiki.com/index.php?title=GetSitting),
[GetRandomPercent](https://geckwiki.com/index.php?title=GetRandomPercent),
and [StopCombatAlarmOnActor](https://geckwiki.com/index.php?title=StopCombatAlarmOnActor).
