# v321: original shooting-range hit events

Continues the original opening from the BB-gun handoff through target practice
and the radroach encounter. Runtime code contains no CG02 IDs, target counts,
stage transitions, dialogue text, or authored NPC routes.

## Physical impact and script delivery

The weapon bridge previously tested skinned actors and otherwise returned only
whether world geometry blocked the bullet. It now queries the nearest authored
world collision surface, including the placed reference ID, and compares it
with the nearest actor surface. Terrain and ordinary objects still stop shots;
only the nearest impacted object can receive its hit events. Hitscan and swept
projectiles share this path. Existing actor damage/event delivery stays separate.

The world trace walks ray-crossed collision-grid buckets, deduplicates triangles,
and checks the existing large-triangle list. It uses current published BHK
surfaces, including animated collision poses. AABB bounds do not manufacture
hits. Disabled, removed and nonresident resources continue to follow collision
publication. The original target NIF supplies 46 collision triangles; no target
hit volume or placement is invented.

QueueWeaponHit records a bounded transient impact for non-actor references.
The world adapter prepares OnHit and weapon-filtered OnHitWith in original order
as one transaction. The action reference remains the attacker; the event filter
uses the actual weapon form. Unsupported execution rolls back both script
phases. Disabled references are rechecked at delivery and actor impacts cannot
be delivered a second time through this object path.

Simple hits execute through the normal per-frame action pump, including while
the player holds the firing weapon. Resident resource changes use the existing
publication workflow when required, retaining pending impacts during busy
admission. Once a result reserves the world, animation updates stop for the
remainder of that frame. Normal script-save throttling handles hit results;
there is no full save per pellet. A scene/session reset discards transient hits.

## Original dialogue distance conditions

The original range greeting compares Dad's distance with an authored marker.
CTDA GetDistance previously resolved only the current speaker or listener. It
now also resolves other original references through the shared reference context,
including saved actor positions. Missing references still reject the condition;
no coordinates are substituted. This lets the original arrival greeting advance
the quest to target practice.

## Validation and boundary

Both original-data New Game branches now continue through the range greeting,
three original target OnHitWith results, Dad's original radroach warning, and
the existing radroach OnDeath result. They verify script-program evidence for
the original hit handler, targetCount, radroach enable/death, quest progression
and save/reload at stage 70. Actor arrival, player movement and voice completion
remain presentation fixtures; this is not a headset playthrough.

Production world-adapter tests cover event order, attacker context, weapon
filters, one-time consumption, disabled/actor rejection and rollback. Physics
tests cover nearest surfaces, walls, unknown-reference blockers, negative and
corner-crossing rays, invalid rays and exact triangle misses. The unmodified
original target NIF is separately tested for BHK reference identity and collision
pose movement; its Forward animation decodes at 2.5 seconds. Game assets are
kept outside Git.

The photo sequence, G.O.A.T. and Escape remain the next opening boundaries.
Headset verification is still needed for target impacts, creature arrival/AI
and the v320 party/door/crowd fixes.
