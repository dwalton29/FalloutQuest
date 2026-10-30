# Human ragdolls — checkpoint 1 (v299)

## Implemented path

A resident, nonessential human reaching zero health transfers its current skeleton pose to the shared Bullet world. The authored human skeleton supplies 18 capsule bodies and 17 limited-hinge/ragdoll constraints, including malleable wrappers. Bone names, geometry, pivots, angular limits, mass, full inertia tensors, damping and contact material come from the NIF, without actor/quest IDs or replacement collision rigs. Definitions are reused within each scene-worker batch.

The render bridge applies one physical pose to skinning and attached bones for both eyes. Animation sampling stops for the physical corpse; existing loot, damage and script death events retain their canonical ownership. Rendering bounds follow the fallen capsules. Unloading retires constraints before bodies/GPU resources. Saved physical corpses can be admitted again by ordinary actor residency.

Save version 25 adds validated, immutable per-body model-space transforms, linear/angular velocities and sleeping state. Root/scene conversion supports cell-origin changes and authored bone scale. Moving saves resume motion; sleeping restores stay asleep. Body-pose revisions do not invalidate NPC package evaluation. Existing saves retain their prior format until a corpse pose is present, and versions 1–24 remain readable. Death events and depleted loot are not replayed on restoration. Older corpses without stored physical poses are not retroactively reconstructed.

Placed armor now selects its authored MOD2 world/display model in both interior and exterior loaders, while character equipment retains the MODL/MOD3 biped model. The original Moira display reference resolves to ArmorMounted.NIF instead of the worn outfit: this was the source of the exposed gore on the display. The intact-mesh staging path also hides BSDismemberSkinInstance wound-cap partitions (101–113 and 201–213). Source/cached geometry remains available for future severing. The original armored Vault 101 outfit loses 2,161 cap triangles and retains 5,502 intact triangles. Its separately authored mounted display mesh has no caps and retains all 4,872 triangles. This also protects intact worn apparel and previews, including the rewarded suit.

## Solver policy and remaining differences

This is a Bullet adaptation of authored assets, not Havok execution. Havok coupled cone limits use a Bullet angular-box approximation. RGDL feedback/pose matching, malleable strength response, the original biped self-collision table, authored death impulses and full dismemberment remain separate work. Creature/robot rigs retain the existing animation path. Player death physics and VR corpse dragging are outside this checkpoint.

Fast capsules request up to eight smaller shared solver steps within the existing 120 Hz physics tick. Independent per-bone sweep corrections were rejected because they stretched joints on stair risers. Supported islands sleep after two quiet seconds using capsule-surface speed; nearby support changes wake bodies, distant streamed geometry does not. These are explicit Quest solver policies. No headset performance gain is claimed.

## Verification

- Physics host tests: existing collision, rigid-world and grab-response tests; connected articulated fall, sleeping/recreation/teardown, distant-streaming wake isolation and fast fall through triangle terrain.
- Supplied original human skeleton: import all 18 bodies/17 joints; validate frames and reject truncated data; collapse onto a floor and a triangle stair/riser, retain joint connectivity, and settle/sleep. Original bind-space pivots already differ by up to about 3.2 cm in the arms; tests distinguish the initial settling window from subsequent constrained motion.
- Production NPC ragdoll bridge: pose handoff without a visible transform jump, attachment propagation, actual gravity/ground contact, canonical loot transfer, save/reload and resident reconstruction, retained depleted loot, restored airborne velocity, no repeated sleeping save/package mutations, and retirement.
- Player save tests: immutable snapshots, actor updates retaining corpse state, invalid-transform rejection, atomic corrupt-save rejection and backward-format regressions.
- Production interior/exterior placement loaders: ARMO world-model selection; original Moira display reference selects its mounted NIF, and the existing original MS11 detonator placement/collision assertions remain valid.
- Original outfit and mounted-display NIFs: triangle counts and intact-cap filtering verified directly. No original assets are committed.
- Original-data MS03, MS11, MQ01 and barter regressions are run separately from physical headset verification.

Android compilation/APK and physical headset verification must be reported separately; host tests do not establish headset visual correctness or CPU cost.

## Headset checks

1. Kill a moving human raider on flat ground and beside stairs/uneven terrain. Check the handoff, connected limbs, ground contact and stable body while walking and turning your head.
2. Loot it, save during another body's fall, and reload. Check motion resumes, looted items remain gone, and no death effects repeat.
3. Leave the cell and return, then restart/reload a settled corpse. Check pose, placement, loot prompt and quiet resting state.
4. Inspect Moira's displayed suit, receive it through the original dialogue and equip it. Check all intact surfaces and absence of wound caps.
5. Compare CPU time with a few active falling bodies and after they settle. Large simultaneous ragdoll counts have not been profiled on a headset.
