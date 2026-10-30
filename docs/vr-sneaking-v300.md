# VR sneaking — v300

Physical crouching and the right thumbstick click now share one player stance. Stand normally once after launch to establish the HMD baseline. Physical crouch enters after a 22 cm drop and exits within 14 cm of that baseline. Clicking toggles a virtual crouch to 65% of standing height. Physical lowering consumes the virtual offset, so combining the controls does not double the camera drop. These thresholds, smoothing, head-relative HUD position and two-bone leg retarget are Quest VR policies, not original game settings.

The stance updates the shared player state, movement, collision capsule, camera, tracked hands, carried objects, player body and SpaceWarp application transform. Expanding the capsule checks ceiling clearance; tangent walls do not prevent standing. Feet retain their floor anchors as the torso lowers. `Player.IsSneaking` and original CTDA 286 now read the same state; script preview copies it without mutating live state. Stance remains transient and derives from current tracking/input rather than the save file.

## Authored inputs

`tools/extract_sneak_settings.py` recovers 74 native numeric defaults and the four original bracketed labels from the supplied Fallout3.exe. It resolves PE sections and original GameSetting records, rather than depending on fixed executable offsets. `fo3-sneak-defaults.h` contains the reproducible result. ESM GMST values override those defaults, including movement multiplier 0.57, base detection -35, maximum distance 2500 and silent weapon sound level 10. Original NPC DATA supplies Perception; WEAP VNAM supplies the loud/normal/silent sound class. No original binary assets are added to the repository.

The footsteps worker loads `characters/_1stperson/sneakmtforward.kf` and uses its original stride/contact timing with original material-specific SneakL/SneakR footstep actions. Missing authored gait data fails quietly. The HUD uses the original SneakMeter proportions, font, green, labels and `glow_stealth_metre_seperator.dds`, displayed above the compass only while sneaking. It reuses cached geometry/textures across stereo views and frames.

Loaded NPC observers evaluate the documented Fallout 3 detection terms using actual skill, equipped torso armour weight, movement, authored NPC Perception, original coefficients, distance, view cone, world occlusion and weapon noise. Hostile observers drive HIDDEN / CAUTION / DANGER; being observed by a nonhostile actor produces DETECTED. Visible detection connects to the existing attack path. Lost detection cancels unseen attacks and searches the last observed position, not the player's unseen live position. Receiving player damage creates an alert even for an unseen silent shot. Expiry ends the search and resumes the existing package.

## Boundaries

This is a reusable player sneaking checkpoint, not a complete reproduction of Bethesda's detection subsystem. Renderer ambient/directional/local-light inputs provide a bounded light estimate; this is not the original GPU light measurement. The loaded player/observer alert bridge uses original thresholds and expiry settings but does not implement the complete original multi-target detection-event list, asynchronous processing-level timers, animated bracket widening, NPC sneak stance, Stealth Boy/Chameleon visibility effects, Silent Running/perk adjustments, theft/pickpocket systems or sneak critical damage bonuses. NPC-versus-NPC acquisition retains its existing implementation. Observer memory is reconstructed on scene load; this checkpoint does not serialize the complete original search state.

## Verification

Automated coverage: stance edge/hysteresis and blocked input, physical/toggle combination, original-data numeric recovery/ESM overrides, original human leg segment continuity, original sneak gait contacts, original IsSneaking condition and result-script preview/commit, production NPC combat detection acquisition/loss/search expiry/friendly detection/view cone, ceiling clearance and tangent walls, plus cached stereo HUD rendering/state restoration. Existing player/quest, physics/ragdoll and renderer suites remain required.

Physical headset verification remains necessary:

1. Stand normally at launch. Duck and stand, then toggle with right-stick click. Combine both and inspect body/feet, hands, carried items and SpaceWarp stability.
2. Toggle crouch under a low ceiling; standing should wait until clear. Test beside walls and on ledges/terrain.
3. Sneak around a friendly actor for DETECTED, then a hostile actor across light, distance and cover: HIDDEN → CAUTION → DANGER; retreat behind cover for CAUTION → HIDDEN. Confirm the enemy searches the last observed position.
4. Fire loud and silent weapons, including an unseen hit, and compare reactions. Check original sneak footsteps on different materials.
5. Open/close modal UI, lose/regain focus, travel and save/reload. A held stick click must not toggle on closing UI. Verify the existing Megaton bomb and Survival Guide dialogue/reward paths, then ragdolls and Moira's displayed suit.
