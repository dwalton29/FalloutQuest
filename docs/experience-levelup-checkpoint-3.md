# FalloutQuest — XP level-up, skill and perk selection

Branch: main. Follow-up to XP checkpoint 1 and reward/HUD checkpoint 2.

## Original-file driven behaviour

- The PERK decoder now retains DATA's playable flag and the top-level
  CTDA prerequisites separately from PRKE-local effect conditions.
- Original Fallout3.esm GMSTs iLevelUpSkillPointsBase and
  iLevelUpSkillPointsInterval drive the skill budget (11 + (INT-1)*1 in
  the base game, equivalent to 10 + INT). Already-owned Educated uses its
  original Modify Skill Points entry-point, adding +3 on later levels.
- Requirement evaluation supports the original vanilla level-up condition
  functions GetPermanentActorValue (495), GetIsSex (70) and GetLevel (80).
  Unsupported conditions fail closed.
- All thirteen actual Fallout 3 skills are selectable; the unused Throwing
  DNAM slot is preserved but never displayed or increased.
- The engine distinguishes total earned XP from committed levels. Levels
  are only committed with an exact skill-point allocation and a valid perk.

## Playable first-pass VR interaction

The original LevelUpMenu two-page flow is presented through the game's
existing world-space readable VR panel renderer (not yet the original
Gamebryo XML visual composition). The player receives XP notifications,
then, when outside NPC combat and other blocking modal interactions, sees
a skill list with point additions/removals, Reset, and Continue. Next is
one perk choice; Intense Training additionally selects one SPECIAL increase.
Input uses the right controller's existing A/B and stick-scroll controls.

The menu does not automatically open until existing XP HUD notices have
cleared. Interaction, movement and Pip-Boy input are suppressed while it is
visible; future work should also pause the wider world simulation for fully
faithful modal behaviour.

## Supported selectable perks in this checkpoint

Only native-functional perk implementations may be selected: Swift Learner
(XP modifiers), Educated (future skill points), Infiltrator (broken lock
retry), and Intense Training (permanent SPECIAL increase). Rank/prerequisite
checks come from the ESM. The remaining vanilla perks must gain their
original ability, magic-effect, quest-stage or entry-point behaviour before
being presented as working selections. GrantPerk remains available to
scripts for original quest-awarded perks.

## Persistence

FQPS v18 adds the eight-byte `LVP1` block after the previous v17 tails:
magic followed by committed level. An advancement forces required v17
RFE1/PST1/BMR1/BDR1/CMN1 blocks (empty when otherwise unchanged) and
keeps SPECIAL, skill and acquired-perk state together. v1–v17 saves remain
readable and pending XP remains inferred from their original saved total.
On restore, levels are validated against the vanilla level cap and the
original cumulative XP threshold. Script transactions do not bypass this
choice process.

## Verification

Host tests include cumulative thresholds, supported perk prerequisites,
original PERK record decoding, skill points, Educated, Swift Learner, atomic
selection and v18 save round-trip. CI also compiles native Android/OpenXR.
Headset interaction/rendering remains to be manually verified.

## Remaining

Original levelup_menu.xml-style visual layout, screen-specific art/icons,
the complete vanilla perk runtime, combat/terminal/discovery/Speech XP
integration, pause semantics, full XP bar animation and definitive headset
input testing. Original Fallout - Sounds.bsa is still required for sound
sample extraction and comparison; SOUN identifiers are already used.
