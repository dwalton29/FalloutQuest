# HUD, quest traversal, door proxies and reloading (v295)

## Original evidence and changes

FALLOUT.INI defines HUDMain/System/Pipboy RGB (26,255,128) and crime
HUDAlt RGB (255,67,42). Ordinary overlays now share that palette and render
after world image-space processing. Vulkan decodes final display tints before
the swapchain's single sRGB transfer. Loading artwork and physical terminals
retain their separate original palettes; legitimate crime prompts stay red.

Quest guidance follows conditional QSTA targets and directed XTEL links across
CELLs and WRLDs. QSTA bit0 is Compass Marker Ignores Locks (xEdit FO3 schema).
Disabled references and unavailable locks block traversal unless that flag
permits it. Reference, lock, key/pin and skill changes invalidate cached routes;
actor pose revisions do not rebuild the graph. Heading updates every frame;
membership retains its five-Hz cadence and immediate CELL/WRLD refresh.

The original-data MQ01 objective40 test uses ChevyChasetoGNR01Ref and its
authored flags, not a premature Three Dog target or locked balcony. Starting
outside Megaton, original doors 000B548B, 0004EDAB and 0001B8E8 lead through
Farragut West Station and Tenleytown/Friendship Station into Chevy Chase.
The final marker points at the authored GNR front entrance. Synthetic tests
cover additional interior/exterior hops, reverse travel and lock invalidation.
This is portal guidance: walking edges within one CELL/WRLD use distance,
not a global NAVM island solver or a drawn pedestrian path.

Original GenDoor01.NIF has BSX bit5 and an EditorMarker branch. Render admission
suppresses only that branch and children, including when animation requests
hidden geometry. The actual door remains visible. Activation can separately
decode original proxy geometry when BHK is absent. No invented boxes or
per-door form exceptions are used. Original generic, BLD12 ruined, metro and
GNR door NIFs were verified for activation bounds. Hinged-door aiming,
activation, animation and collision now also work outdoors. Reconciliation
watches saved door targets instead of unrelated Player revisions.

## Reloading

The canonical absent-magazine state rejects repeated ejection, including after
save/load. Firing an inserted magazine empty still permits one ejection.
ReloadWeapon atomically performs the existing eject/load/chamber operations;
it conserves rounds and rejects full, dry or unusable weapons.

Grabbed magazine transforms sample the original FP skeleton and reload KF at
the authored reload-in sound, including model clip-node animation. Original
pistol and assault-rifle tests verify hand/component alignment. VR insertion
accepts any angle within the weapon bounding sphere plus 30cm. That radius is
an explicit VR access policy, not an original-game constant.

Pause → Debug → Manual reloading defaults On. Off changes B to animated
reload: original FP KF weapon/component motion, left-hand motion retargeted
through the existing VR arm IK, and original sound text keys. Ammo changes
once at completion. Dropping, stowing, changing weapon or losing gameplay
access cancels the transient animation. The toggle is local to the app session.
Missing original KFs do not produce invented motion or a free instant reload.

## Verification

Host checks passed: original ESM/executable world-script suite (including
Gob/Nova, persistence and real MQ01 approach), original weapon records and
reload persistence, Debug input, original weapon NIF/KF attachments, synthetic
and original editor/door admission, HUD state restoration, layering and source
checks. Software Vulkan with validation passed the production multiview and
compositor suite, including authored green/crime swapchain bytes in both eyes
and supported presentation formats. Android CI and physical headset results
are reported separately with delivery.

Headset checks still required:

1. Compare A prompts, ammo, HP, compass, XP and dialogue green in tinted
   exteriors and interiors; legitimate crime interactions must remain red.
2. Follow the selected GNR objective through both metro stations. Reverse a
   door transition and reload on either side; guidance should select the next
   local door. Check the front entrance rather than a balcony.
3. Try Chevy Chase North ruined/metro doors. Open/close an outdoor hinged
   door, leave/return and reload. Generic doors should have no orange editor
   rectangle while remaining interactive.
4. Eject a loaded and a fired-empty magazine; repeated B must do nothing.
   Save/reload with the magazine absent. Grab a spare; release above, beside
   and below the weapon within the enlarged sphere. Check its hand orientation.
5. Turn Manual reloading Off. Press B with pistol and assault rifle: check
   original motion/sounds, left-arm retargeting and one ammo transfer. Check
   cancellation by stowing or transitioning, and pause/resume timing.
