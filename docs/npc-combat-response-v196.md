# NPC Combat Response — v196

Based on main v195 `42b9d71860c973b0997e8373177f29a605228e36`.

## Repair scope

- Resolve candidate WEAP leaves of the original NPC_ inventory through
  canonical LVLI entries, including nested lists. This work happens when
  preparing the NPC's original weapon/KF/NIF assets, never every render
  frame. Only the already-existing canonical Player-owned inventory decides
  which weapon instance is equipped or spends ammunition. Duplicate leaves,
  cycles and unexpectedly large lists are bounded.
- Prepare original A/B/C IDLE hit reactions for head, torso and each arm
  and leg. Select an available authored clip per region with a deterministic
  per-actor sequence; retain a known original sibling if a source KF is absent.
  No fake animations or character-specific positions are introduced.
- Stop hit reactions overwriting a committed weapon attack or an ongoing
  reload. Bound other combat flinches to 0.15 s and noncombat reactions to
  0.35 s without clearing canonical damage attribution.
- Use the posed original VR Spine2 as before. If the torso mapping is
  unavailable while the scene and tracked headset are valid, allow the
  real tracked head position for enemy perception and aiming. Projectile
  collision still requires real player skin geometry, not a synthetic hitbox.
  Diagnose missing head/target instead of silently failing.
- Cowardly combat fleeing follows the original NAVM route using source
  `locomotion/(male|female)/mtfastforward.kf` and the game's own GMST
  `fMoveRunMult`. Walking/other NPC packages remain unchanged.
  Diagnose an unreachable flee endpoint.

## Original Fallout3.esm checks

`MegatonSettlerWeapon` LVLI is 0006C36B; `WithAmmoAssaultRifleNPC`
LVLI is 00029367. GMST `fMoveRunMult` is 4.0. Original IDLE groups
contain head/torso A/B/C and three variants per left/right arm/leg,
18 records total. The host suite tests recursive selection/cycles, original
LVLI candidates when optional ESM is available, source movement settings,
hit-clip rotation, hit-vs-attack/reload policy and existing combat firing,
fleeing, death and persistence regressions.

## Headset acceptance

Record `NPC COMBAT CANDIDATES`, `NPC EQUIP UNSUPPORTED`,
`NPC COMBAT PLAYER TARGET`, `NPC COMBAT START`,
`NPC HIT REACTION`, `NPC FLEE`, `NPC PATH`, `NPC EQUIP`,
`NPC FIRE`, and `NPC DAMAGE`. Test a Common House settler, the
Megaton bodyguard and Moira (Coward confidence); verify enemies fire,
reload and flee when conditions genuinely allow it, with differing
authoritative hit clips. Remaining gaps include accurate full Bethesda
detection/crime/critical/limb health, stamina/AI tactics, heavy weapons,
full aiming parity, essential unconsciousness and Havok ragdoll physics.
A green build cannot certify visual or physical behavior in Quest.
