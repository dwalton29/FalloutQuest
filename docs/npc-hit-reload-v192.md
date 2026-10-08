# v192 — Original NPC hit reactions and discoverable pistol magazine

Base: canonical main v191. No branches, no packaged Bethesda assets.

## NPC damage
The physical weapon hit ray still requires authored skinned triangle collision, world occlusion and successful canonical Player::ApplyAttack. Successful nonfatal NPC hits can now trigger a short, bounded reaction. At scene preparation the original Fallout3.esm IDLE records MTHitHeadA, MTHitTorsoA, MTHitArmLeft, MTHitArmRight, MTHitLegLeft and MTHitLegRight are resolved via the existing EDID-to-MODL catalog and their KFs decoded from the user's game meshes. Unavailable original clips fail closed (no fabricated motion).

The reaction temporarily owns Animation::Hit, cancels pending combat hit events, pauses package locomotion and resumes prior AI when the decoded animation finishes. Actor death still owns Death, including corpse persistence; combat/quest and core movement state stay canonical. This does not add ragdolls, pain sounds, armour reactions, force/knockback, limb crippling or fully replicate Fallout 3 staggering.

## Physical reloading
The existing reload controls are unchanged: draw 10mm with right grip; B (right controller) ejects the magazine; reach to the body's left waist and press left grip to take the spare; move to the original magazine port and release; then grasp/pull slide to chamber. Before left grip, the original ##Clip/##Magazine descendant geometry is rendered at the left-waist ammo pouch when the weapon has an ejected magazine and compatible reserve ammo. If the authored NIF lacks separate eligible geometry there is no fabricated mesh; asset logs include magazineShapes.

The existing weapon-relative Monofonto HUD now shows B: EJECT, LEFT WAIST GRIP or PULL SLIDE based on actual loaded/needsAction state. Left-pouch entry gives a haptic pulse. Exact ammo consumption and saves remain owned by Player::EjectMagazine/LoadMagazine/ChamberWeapon. No independent magazines or duplicated reserve ammo were created.

## Tests and acceptance
Host tests assert reaction animation ownership across NPC combat and package ticks, that combat resumes, and optional original-master integration resolves the six IDLE/MODL records. Earlier save-format migration test offsets in v191 were corrected before release. Android CI and Quest headset tests are necessary to verify original KF loading, visual reaction, separate NIF magazine shape, comfort and successful physical reload. Those are not implied by committing source.

Quest log filters: `NPC HIT REACTION`, `NPC HIT CLIP`, `NPC DAMAGE`, `WEAPON ASSET`, `WEAPON MAG OUT`, `WEAPON MAG POUCH`, `WEAPON RELOAD INSERT`, `WEAPON RELOAD CHAMBER`.
