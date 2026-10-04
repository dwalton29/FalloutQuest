Interaction follow-up (0.32.4)

The original Fallout3.esm identifies The Brass Lantern exterior door REFR
0x3A14, base 0x41714 (MegBrassLanternFrontDoor), with script 0x41719
(MegBrassLanternFrontDoorSCRIPT). Every OnActivate branch calls activate;
its time-of-day condition only enables/disables two customer references.
Permit this verified default activation while retaining the general block on
unsupported door scripts and respecting locked-door/key checks. Customer
schedule side effects still require the scripting/AI milestone.

The interior exit REFR 0x3A37 is owned but unlocked. Door ownership does not
prevent opening; crime/trespass reactions are separate gameplay systems.
Container ownership remains checked independently of the shared lock/key test.

Use the item catalog for pickup targeting rather than requiring the loose
physics flag, and include INGR and NOTE in the physics-supported types.
Owned items, unsupported scripted pickups, non-playable forms and blocked
saves remain unavailable. Owned/Scripted/Unavailable prompts now explain a
rejected pickup instead of silently displaying only the item name.

Regression coverage includes both original Brass Lantern references,
unlocked owned doors, protected containers, lock/key behavior, unsupported
scripts and catalog pickup targeting without a loose physics flag. Original
assets are used only for local verification and are not shipped in git.
