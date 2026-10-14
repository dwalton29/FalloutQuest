# FalloutQuest v210 — Fallout 3 bartering

Bartering runs through original dialogue results and the existing persistent
player/container inventories. No merchant stock, money or dialogue is invented.
The Android version is `0.73.0-barter`, versionCode 210.

## Source data and behaviour

The supplied Fallout3.esm confirms Moira Brown `0002D2BC` → merchant chest
`000B4207`, and Gob `00003B3D` → chest `00020478`. The chest can be locked,
buried or nonresident; its personal inventory is independent of NPC equipment.
Original AIDT services, AI-data templates, PACK Offers Services and ETYP item
categories gate trading. Quest objects, nonplayable/non-droppable items,
unsupported item scripts and unknown auto-calculated values are excluded.

`ShowBarterMenu` / `SBM` resolve the calling actor or an explicit named reference.
An optional integer discount, including a supported script expression, is
validated in [-100,100]. Effects are candidate-owned and become pollable only
when the shared script transaction commits. Begin/end results retain authored
ordering. The UI waits for the authored spoken sequence to finish. Cancelling
returns to the existing dialogue choices. Death, combat, walk-away, loading and
actor unload cancel pending trading. Barter focus suppresses locomotion,
grabbing, reload input and firing; a trigger held for quantity adjustment cannot
shoot an equipped weapon.

Info Refusal was incorrectly excluding Moira's normal trading INFOs. It is now
accepted: this flag keeps the topic highlighted. Player GetEquipped conditions
and the authored ModDisposition command needed by Gob's introduction are also
supported. Instance disposition deltas persist; the broader effective disposition
and speech-challenge evaluator remains separate.

## Prices

GMST values come from the installed ESM, including fBarterBuyBase/Mult,
fBarterSellBase/Mult and fItemConditionValueBase/Mult. The supplied executable's
fItemConditionValueExp default is 1.5 (file 0xCFD424). An ESM override wins.
Barter skill uses the saved player skill, not the merchant's rating.

Executable inspection identifies the multiplier routine at VA 0x601900 and the
menu price routine at VA 0x916EF0. The latter applies the signed discount,
Calculate Buy Price (entry point 17), rounds positive values half-up to whole
caps above one cap or tenths at/below one, then bounds buying below by item value
and selling above by it. The 0.5 and 0.1 constants are at 0xF3ED74 / 0xF4E140.
Unconditional float set/add/multiply perk entries are implemented, including the
ESM's Master Trader 0.75 multiplier. Conditional/item-context/scripted entries
are rejected conservatively rather than silently ignored.

Unit prices retain fractional caps; stack totals are computed before the integer
currency settlement. Exact original final settlement behaviour for fractional
mixed baskets still needs an executable/runtime comparison. This is a remaining
parity limitation, not a claim of complete Bethesda numerical equivalence.

Reference documentation:

- https://geckwiki.com/index.php/ShowBarterMenu
- https://geckwiki.com/index.php/Barter_System_Formula
- https://geckwiki.com/index.php/Item_Condition_And_Value
- https://geckwiki.com/index.php/IHoursToRespawnCell
- TES5Edit FO3 definitions: AIDT services, ETYP, PACK flags and PERK parameters.

## Transactions and persistence

Selections are a read-only basket. Confirmation revalidates actor/container
identity, service state, skill/perks, exact source inventories, item eligibility,
quantities, prices, caps and save capacity. A private candidate transfers both
sides and caps; the next UI snapshot is allocated before publishing state.
Complete transfers preserve stack/weapon identity, condition and loaded-ammo
state. Partial stacks receive a new identity. Transfers unequip sold items and
clear explicit item ownership. Existing weapon runtime retires missing equipped
instances. Legal buying/selling never queues theft or Karma effects.

Save v16 adds merchant visit timestamps and authored disposition modifiers.
Existing saves without these extensions retain their v14/v15 byte layout. The
original container and weapon extensions continue to save stock, caps, condition
and instance fields. Pending baskets are not saved.

Respawning, nonscripted merchant storage uses the canonical game clock and
`iHoursToRespawnCell` (supplied executable default 72 hours). Continued visits to
merchant/storage cells postpone reset. Reopening dialogue cannot restock.
After sufficient absence, source CNTO/LVLI expansion replenishes stock/caps.
Encounter-zone overrides and scripted reset cases remain conservative. The
existing leveled-list RNG is FalloutQuest-owned, not Bethesda RNG/save parity.

## Quest headset check

1. Install v210 over the existing development APK. Enter Craterside Supply.
2. Speak to Moira. Finish her original introduction if this is a first meeting;
   select her original trading topic. Listen through the authored response.
3. Confirm both cap balances and inventories. Right stick up/down scrolls;
   left/right chooses player/merchant. A selects one unit or removes a selection.
   Hold right trigger and move the stick left/right to reduce/increase quantity.
4. Select a purchase and a sale. Check the net caps. Scroll to Confirm transaction
   and press A. Check quantities, caps, condition and carry weight.
5. B returns to dialogue. Reopen, exit/save and reload; verify depleted/sold stock
   and both balances. Leave pending selections and cancel: no changes should occur.
6. Repeat with Gob in Moriarty's Saloon, including his original introductory
   responses. Verify the original 20% discounted result where it is eligible.
7. Try insufficient caps, a held trigger, walk-away, combat, actor death and
   loading. Confirm no duplication, firing or partially committed basket.
8. Leave the merchant/storage cells for over 72 game hours, return and inspect
   supported respawning storage. Ordinary reopening should retain stock.

Physical Quest testing and actual voice/visual appearance on the headset have
not been performed here. Repair, medical services, dynamic SetMerchantContainer,
conditional price perks, item-script transfer events, auto-valued ingestibles,
encounter-zone resets and full original menu-art reproduction remain outside
this supported subset. The UI is a two-column VR adaptation using the existing
original Fallout font, tint, controller and menu sound infrastructure.

## Validation results

- Player CTest: 30/30 pass, including the production barter/dialogue bridge.
- World CTest: 12/12 pass, including M1B3l resident door-resource ownership.
- Original ESM: Moira/Gob XMRC mapping, isolated stock/caps, original introductory
  dialogue, authored trade results (Moira 0%, Gob 20%), mixed buy/sell and
  player/merchant save/reload pass.
- Original ESM regressions: Lucas dialogue, MS11 acceptance/nested objectives,
  ownership/theft and Megaton lockpicking pass.
- Font, Pip-Boy, shoulder inventory, render dispatch, opaque, actor skin,
  notifications, scene publication and scripting source guards pass.
- Android native compilation/APK delivery is validated by the main push workflow;
  its outcome is reported separately with the delivered APK.
