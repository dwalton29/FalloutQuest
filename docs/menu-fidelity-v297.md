# Menu fidelity and physical wrist rendering — v297

## Changes

The physical Pip-Boy now stays in the normal opaque/alpha player passes whether
raised or lowered. It no longer clears depth or draws its casing/screen/glass over
the finished world. Other hands and objects can cover it; its original controls,
culling/depth flags, local lighting and world image-space treatment share the same
path as the rest of the physical model. The v296 late geometry pass was missing
that pass context. This is a render-path correction, not replacement knob geometry
or an apparel mesh edit. Outfit-specific cuff fitting still needs headset QA.
Flat HUDs and dialogue/barter/message panels retain their overlay visibility.

The original Stats XML contains `&-sStats` without a closing semicolon. The menu
evaluator and executable-default extraction now accept that original syntax.
GameSetting resolution also handles original case variants such as `sOK`/`sOk`.
Non-text tiles default to font zero so the original text-box prefab can choose
its own regular/glow font rather than having that branch forced to font 2.
Text measurement uses the original post-load space glyph height (executable
B564AF), with the font cell centred on the XML Y anchor in the VR renderer instead
of adding a full line baseline. Source box padding and horizontal justification
remain intact. Host tests check real font metrics and label placement inside the
original padding; physical headset alignment is a separate verification step.

The quest list excludes unnamed/internal quests and quests never announced or
shown to the player; announced entries remain in the journal after objectives hide. It uses the original objective list (`MM_QuestObjectivesList`, item
type 4), source marked-row template and authored objective ordering. Completed or
failed objectives and quests use the source `_TextAlpha` channel at 128; completed
objective markers are filled. Production quest stages, scripts, rewards and save
formats are unchanged. Tests create journal fixtures through normal Player APIs.

Radio uses the supplied original `Interface/Radio Wave/wave.dds`, a 1024×256 DXT5
wave strip, and the original MapMenu image/axis geometry. The executable's
009502B7–00950306 loop advances UV phase by 0.1 after at least 10 ms and wraps at 1;
that loop is reproduced once per gameplay frame while a valid radio signal exists.
This is authored waveform artwork, not a fabricated live audio equalizer. A
horizontal-repeat sampler is connected to the native Vulkan texture bridge.
Wave UVs update resident geometry; static menu XML/text are not rebuilt per tick.
Out-of-range or disabled radio uses the original flat centre line.

Barter uses its original all-items, weapons, apparel, aid, miscellaneous and ammo
categories, filter arrows and side-selection art. The original QuantityMenu and
meter prefab display stacked amounts. Existing Player pricing, service filters,
pending selections and transaction validation remain authoritative.

- LT selects the player's inventory; RT selects the merchant's.
- Vertical stick selects items; horizontal stick changes the active category.
- A selects/removes an item; stacks open the quantity meter. Horizontal stick
  changes quantity, A accepts it, and B cancels it without staging a trade.
- Left-controller X accepts the complete pending transaction. Holding X does not
  repeat it. B leaves barter when no quantity dialog is open.
- Successful trades no longer create a confirmation text popup. Failed trades
  retain their actual reason.

Dialogue and barter retain the original solid-black backdrop rectangles. Their
world-space background opacity is raised to 160/255 for VR readability; this is
an explicit presentation adaptation. Source fonts, green, borders and text remain.

## Verification and limits

The original Misc BSA, original font 2/4/6/7 FNT metrics and ESM drive host tests
for all Pip-Boy pages, the title token, original quantity menu, filtered merchant
lists, quantity binding, journal ordering/dimming and cached radio UV updates.
Textures/GL calls are mocked in the menu binding fixture; it does not prove every
original DDS in a headset installation is present. Supplied loose atlas/button
files include empty entries, as recorded in v296; missing art is still diagnosed.

The production barter adapter is included in the host integration test. It drives
LT/RT, category changes, quantity adjustment/cancellation/acceptance, X commit,
caps/items, one-time save flushing, no success popup and B exit. Existing original
merchant transaction/persistence regressions are retained.

Production software Vulkan verifies horizontal wrapping through real descriptor
and sampler selection in both eyes, alongside the existing UI/depth/colour suites.
Android build and physical headset outcomes are reported separately on delivery.
No physical headset is attached to this session.

Headset checks: cover the raised screen with the right hand; inspect knobs and
lighting outdoors/indoors; verify text padding and dark backdrops; finish a quest
objective and check its dimmed marker; tune/disable/out-range radio; buy/sell
stacked goods through the slider, cancel once, accept once, hold X, then reopen
and confirm the exact caps/item changes.
