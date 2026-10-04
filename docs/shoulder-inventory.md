# Right-shoulder loose-item inventory (Quest v155)

Grip a loose item with the right hand, move the solved palm behind the right
shoulder, keep it there briefly, then release. Outside releases retain the
existing sampled linear/angular velocity and dynamic-body release code.
Left-hand movement does not inventory items. Empty-hand gestures and equipped
weapon holsters are not implemented.

## Body frame and gesture

The anchor is `Bip01 R UpperArm`'s authored bind pivot from the **same first-valid
right arm master partition** used by `Q213SolveMasterArm`. The hidden canonical
body is deliberately loaded first by the existing rig. Transform that pivot
through `gQ210PlayerRoot`, which follows `gQ213TorsoYaw` (existing torso deadband
and locomotion rotation), rather than raw HMD yaw. There is no average of mesh
partitions and no guessed shoulder fallback: missing master disables stowing.

The first headset implementation used a shoulder-centred 18/20/18 cm ellipsoid
plus an 80 ms dwell. Real use showed that volume was too precise: a natural hand
over the shoulder can sit 25-40 cm from the upper-arm pivot. The revised trigger
is a broad rounded backpack-mouth volume centred slightly outward/up and 17 cm
rearward, with approximate right/up/rear radii **27/30/28 cm** and a 5 cm exit
shell. A rear plane still rejects ordinary chest/face motion. These dimensions
remain VR interaction policy rather than authored game geometry.

Because a REFR is already physically latched by the right-hand grab, entering
the valid rear shoulder volume while grip is held arms immediately; releasing
at or below the existing .25 threshold commits. There is no consume-on-entry.
Exiting, tracking loss, focus loss, ineligibility, reference change and scene
reset still disarm the gesture.

## Original UI evidence

Inspected the supplied `Fallout - Misc.bsa`, `Fallout3.exe`, `FALLOUT.INI` and ESM.
No copyrighted game asset is copied into the repository.

* `menus/main/hud_main_menu.xml`: `HUDMainMenu/Messages` owns general HUD
  messages, width 460, height 90, system colour HUDMain. `QuestAdded` is separate.
* `menus/prefabs/hudtemplates.xml`: left-justified text uses font 7;
  message icon/bracket templates are separately defined.
* `FALLOUT.INI`: font 7 is `Baked-in_Monofonto_Large.fnt`, already loaded by the
  canonical FNT/texture pipeline. Its atlas contains the baked glow.
* Supplied executable: string defaults `sAddItemtoInventory = added` and
  `sPlural = (s)`; formats at VA `0x00F53368` / `0x00F5335C` are `%s %s` /
  `%i %s%s %s`. Pickup paths around `0x00627666` and `0x00627AD5` select these
  based on quantity >1. Thus **Bottle added** / **3 Bottle(s) added**, rather
  than “added to inventory” or a parenthesised count after the item name.
* Pickup call at `0x0062771B` passes float **2.0** to the message entry point
  `0x008FE390`, which forwards to queue routine `0x00933F80`. The latter stores
  duration at entry+0x30C, appends messages and suppresses identical tail text.
  We retain each successful transfer, even identical item text, to satisfy
  deterministic one-notification-per-transfer feedback.

Runtime GMST string overrides are used when available; executable defaults are
used when absent from the ESM. Exact desktop placement, message fade curve and
full icon layout have not been established. The following are explicit VR
adaptations, not claims of recovered vanilla behaviour:

* two-metre head-relative stereo plane, shared midpoint pose for both eyes;
* text starts 22 cm right / 43 cm above centre, left-justified, .00125 m per
  authored font pixel, wrapping at 460 pixels into at most two 45-pixel rows;
* original green (26,255,128) and baked font glow, no controller/button icon;
* recovered 2-second hold followed by a chosen linear .5-second fade;
* bounded 16-entry FIFO; overflow drops the oldest entry, backlog age capped at
  40 seconds; loading, focus loss and shutdown deliberately clear messages.

## Inventory and lifecycle

`CollectFo3WorldReference` is shared by shoulder release and A-button pickup:
revalidate scene/loading/save-block and `Player::CanPickup`; call `Player::Pickup`
for authored XCNT and condition; clear both matching grabs; erase its dynamic
body; publish collected collision refs; then notify/play authored pickup audio
and flush. The existing collected-ref renderer policy hides every matching GPU
shape. Failure leaves the object for ordinary release. Render-thread serialization
and `IsCollected` prevent a shoulder/A double transfer.

Containers retain `TakeContainerStack` and selected-stack semantics. Both world
and container paths share `NotifyFo3ItemTransfer` for actual count, audio,
notification and save flush. A save-write failure retains the canonical in-memory
inventory and is logged by the existing flush path; later flushes retry. Rejected
existing saves block shoulder pickup before mutation. This does not introduce a
separate inventory or a new disk transaction policy.

Notifications advance/build geometry once per logical frame after actor/hand
preparation. Each eye reads the same queue and cached geometry. Resource warming
is outside stereo draws after scene install paths are ready. A cache-only GL guard
snapshots renderer-owned state, never calling glGet/glIsEnabled (including cold
cache). Depth testing/writes are disabled for the view HUD. The existing v154
interaction-HUD cache fix remains intact.

## Verification

Host tests cover gesture geometry, left/outside/exit/brief-crossing/tracking-loss/
blocked-save rejection, canonical pickup, quantity/condition, grab/body/collision
retirement, exactly-once audio/flush/notification, duplicate rejection, A-button
and container paths, lifetime/fade, overflow/expiry and stereo read-only state.
GL tests exercise 200 visible-eye state transitions for both warm/cold caches
and assert zero queries. Source regressions enforce unchanged ordinary throw
release and no notification timer/asset/geometry work in the eye draw.

On Quest: use a loose bottle in Super-Duper Mart; right grip, move behind shoulder,
release; check disappearance, inventory, one sound, upper-right green feedback.
Compare front release and left shoulder, look sideways without turning torso,
then turn/locomote torso. Rapidly stow items and inspect frame performance. Test
tracking/focus loss, both CELL transition directions and resume. A successful
Android build is not evidence of these physical/on-device checks.

Windows diagnostics:

```bat
adb logcat -s FalloutQuest | findstr /C:"SHOULDER INVENTORY" /C:"ITEM ADDED HUD" /C:"ITEM PICKUP" /C:"THROW RELEASE" /C:"PLAYER SAVE"
```
