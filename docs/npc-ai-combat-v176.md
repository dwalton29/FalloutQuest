# Authored package combat policy — version 176

Version `0.46.4-package-combat-policy`, extending main `2f48f4c4ff771507e34ad688248b3691bff0d7d8`. This increment preserves the resident package, navigation, combat, death/loot and save infrastructure from versions 172–175. It does not complete all requested Fallout 3 AI/combat acceptance criteria.

## Original data

The [FO3 xEdit definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsFO3.pas) define PACK CNAM as its CSTY combat style. The [shared FO3 flag definition](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsCommon.pas) defines PKDT general bit 22 as Defensive Combat. The [GECK package documentation](https://geckwiki.com/index.php/Category:Packages) specifies that a defensive actor does not initiate combat and identifies a Follow/Escort/Accompany exception when the package target is attacked. It also identifies the package combat style as the style used while running that package.

The supplied original Fallout3.esm has 161 packages carrying this defensive flag and 17 nonzero package combat-style overrides. Original-data tests audit all 17 CNAM references against the existing decoded Fallout 3 CSTY definitions. They include multiple source packages/styles and are not a Lucas-specific implementation. Unsupported package families are still not executed merely because their policy metadata parses.

## Threat acquisition and package ownership

Resident perception consults the eligible authored package before initiating combat, including on the first simulation and after a schedule change. Dialogue and combat retain the temporarily suspended package owner. This uses cached definitions and existing schedule/condition/priority evaluation at the existing 4 Hz perception cadence. No path is built, asset loaded, ESM scanned or actor moved while resolving policy.

Defensive packages suppress ordinary AIDT/faction aggression and unrelated faction-assistance acquisition. Canonical damage hostility still permits retaliation and combat still interrupts dialogue. Supported actor-reference Follow and player Escort packages can defend their living resident target. The evidence is either the leader's canonical saved damage hostility or an actively combating resident actor targeting that leader. Automatic leader defense retains existing range, target-validity and world line-of-sight checks. Accompany remains unsupported.

This is a bounded use of existing damage/combat evidence. It does not add a complete Bethesda crime, friendly-fire forgiveness, detection, alarm or attack-attribution system. Persisted hostility can outlive a particular hit until the existing combat owner clears it. Player leaders have no newly invented combat/attacker store: only existing resident attackers targeting the player establish automatic leader defense. Damage received by the defender itself retains the existing immediate hostility path.

A shared package-adoption helper is now used by ordinary execution and combat acquisition. If perception selects a newly scheduled package before its first movement step, combat suspends that package's intent/progress and resumes it afterward. The actor's current transform is preserved. Existing Patrol waits and Escort/Flee phases remain retained when their package owner is unchanged.

## Combat style, weapons and persistence

An active package's nonzero CNAM overrides NPC ZNAM/template traits for the existing supported CSTY decision fields. It feeds the current weapon restrictions, semi-automatic timing and other already implemented style decisions. A null/absent CNAM falls back to the original NPC traits. A malformed CNAM rejects the package and permits a valid lower-priority package; an unavailable/unsupported nonzero CSTY prevents weapon selection and emits a deduplicated diagnostic instead of silently using an invented default.

The style is derived from the canonical package FormID, not serialized as a pointer or a parallel actor inventory/combat value. Save revision 9 remains unchanged and older revisions remain readable. Save/load and combat restoration re-resolve the style through cached original definitions. Firearm/projectile/damage formulas, physical player weapons, clip/reload ownership, sounds, authored animation assets, corpse inventory and world/player transitions are unchanged.

## Verification and remaining work

Production host regressions cover defensive first-frame acquisition, schedule changes, dialogue immunity to ordinary aggression and interruption by real damage, player/NPC retaliation, suppression of unrelated faction assistance, Follow/player-Escort leader defense, line-of-sight rejection, package adoption/resumption, CNAM precedence, null fallback, malformed/unavailable overrides, actual weapon restriction selection, supported firing timing, and save/combat style restoration. Parser regressions cover valid, null and truncated CNAM. Optional original-data tests verify all 161 defensive flags and 17 combat-style references, alongside existing package/NAVM, Flee, Lucas dialogue, physical weapon, death/loot/save and actor-key door regressions.

Headset validation remains required for defensive NPC responses and weapons/style switching, plus unchanged movement, dialogue, shooting and corpse looting. Remaining gaps include Eat/Sleep/furniture, Flee To/start locations/run/cower groups, Follow start/end procedures, NPC/object Escort, independent NPC CELL/load-door transfer, other package/combat directives and scripts, full combat/damage/detection parity, essential unconscious recovery, and creatures. No Bethesda assets are committed or packaged.

Validation for this increment: all 31 applicable host CTest checks passed across player, assets, world, physics, NIF and audio suites; the two asset-input checks skip without their optional external fixtures. All six ESM integration suites passed, including the new policy audit, original package/Flee/NAVM execution, multiple armed NPCs, six representative weapon definitions, Lucas greeting/choices, and three original actor-key door cases. Android workflow status is reported with the delivered commit.
