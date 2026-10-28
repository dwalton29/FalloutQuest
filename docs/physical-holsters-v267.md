# Physical holster access — v267

Hip access is now a 28 cm radius sphere (previously 16 cm horizontally / 20 cm vertically). Shoulder access uses a 38 cm radius sphere, with a front plane to reject ordinary reaching/aiming in front of the body. Entry/exit hysteresis remains in place. Loose-item shoulder stowing shares that same geometry. These distances are explicit VR interaction policy.

Weapon holster checks use the valid tracked controller position independently of the solved arm palm. IK reach limits and physical hand constraints must not move a body-access target away from the controller behind the head. A recently entered stow can finish through up to 200 ms of positional occlusion; an untracked hand cannot enter a zone, and stale coordinates cannot stow a weapon.

Sidearms use the right hip and long guns use the right shoulder/back. Both slots can expose an owned, supported weapon independently of the currently active gun. Retrieval switches the canonical equipped instance and preserves ammunition, condition and identity. Each slot prefers its active or recently used weapon, then a stable owned instance. Enlarged zones that overlap choose the closer occupied slot. Stored weapon display placement stays separate from access size.

Validation covers spherical horizontal/vertical reach, rotated body axes, the shoulder front guard, overlap selection, occlusion expiry, inactive-slot retrieval and instance preservation. Existing loose-item collection, weapon input, authored asset, reload, projectile and grip tests remain in the suite. Physical reach and controller occlusion still need headset validation.
