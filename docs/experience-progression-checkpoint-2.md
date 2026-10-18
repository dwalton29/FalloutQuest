# Fallout 3 XP HUD/audio — checkpoint 2

Repository: dwalton29/FalloutQuest, branch main.
Basis: actual Fallout3.esm SOUN/GMST references and original
`Fallout - Misc.bsa` `menus/main/hud_main_menu.xml`.
The game files have not been changed.

## Live behaviour

- Every successful `RewardExperience` result now queues a transient
  `ExperienceNotice { earned amount, cumulative total, threshold crossed }`.
  The threshold is calculated by the checkpoint-1 `ExperienceProgress`
  evaluator, without automatically committing a level and losing perk/skill
  choices.
- `CompleteLockpick` already goes through this same function and therefore
  produces an XP notice with any applicable original Swift Learner multiplier.
- The render-thread side of `DeliverFo3ScriptSounds` consumes notices once,
  presents a short green XP message using the existing VR notification HUD,
  and schedules `UIPopUpExperienceUp` or `UILevelUp` from the original ESM
  sound catalogue. The text uses original `sStatsXP` and `sLevelUp` GMSTs,
  with vanilla-English fallback.
- Notification effects are part of the candidate Player and swapped/rolled
  back with script transactions. Failed previews and rolled-back quests
  cannot generate live reward feedback. Successful save restoration clears
  transient XP notices, preserving stored XP without replaying sounds.
- `shoulder_notification_tests`, `lockpick_tests` and
  `ms11_completion_tests` cover one-time XP cues, award multipliers,
  rollback, commit and restore.

## Authenticity boundary

The original `XPMeter` is a separate HUD rect containing `XPBracket`,
`XPPointer`, `XPAmount`, `XPLabel`, `XPLastLevel`, `XPNextLevel`
and `XPLevelUp`. It refers to assets
`Interface\\HUD\\glow_XP_bar_over.dds`,
`Interface\\HUD\\glow_XP_bar_pointer.dds` and
`Interface\\InterfaceShared.tai`. Those original DDS/atlas assets must
be loaded from the user's game textures for faithful rendering.

Checkpoint 2 uses the **already existing VR text notification layer**
as a visible interim representation of the real XP event, **not** a claim
to have implemented the original bar image, pointer animation, or Bethesda
level-up sound timing. Named sounds resolve only when the user's original
Fallout sound archive/files are installed. No synthetic sounds or art.

Still to implement: complete original XPMeter rendering, skill/perk
selection from `menus/levelup_menu.xml`, combat deferral of level-ups,
other XP reward sources, and complete perk eligibility/effects.
