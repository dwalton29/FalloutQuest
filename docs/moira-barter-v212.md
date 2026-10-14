# v212 — Moira's missing barter topic

Baseline: v211 `e8b3d94580f0d9c3b48679b948251c0d083dbb10`.
APK version: `0.74.1-moira-barter`, versionCode 212.

## Root cause

The original trade topic is `0001E7D7` / MegMoiraBrownBarter:
`Let me see what you have for sale.` Moira reference `0002D2BC` uses NPC base
`0002D3C0` and her existing merchant chest `000B4207`.

MerchantAvailable incorrectly required a saved actor's active PACK to carry
Offers Services. Her normal daytime shop package `00004153` has general flags
`0x240`, with Offers Services (bit 0) unset. This is Bethesda's original data,
not a malformed package. Her normal trade INFOs do not contain
GetOffersServicesNow. Their explicit ShowBarterMenu result was rejected during
eligibility preview, hiding the topic entirely.

The previous original-data test had no saved actor package and selected the
first PACK with bit 0 for its dialogue context. That selected a vendor-follow
interrupt rather than the normal shop package and concealed the production bug.

## Fix

Remove the additional PACK flag restriction from explicit scripted bartering.
Original INFO/QUST conditions still determine whether the topic is eligible.
GetOffersServicesNow remains supported and restrictive wherever authored; its
flag interpretation is unchanged. Merchant identity, source inventory/services,
disabled/dead/hostile state, pricing and transactional checks remain intact.
No dialogue option, service package or stock is fabricated.

Native ShowBarterMenu at VA `0x7A7EC0` validates an actor and opens the barter
menu; it does not call the package-service query at `0x7EC210` used by
GetOffersServicesNow. Original PACK definitions and native query also confirm
Offers Services is bit 0; changing it to another bit would be incorrect.

## Validation

- Before the fix, original Moira with saved shop PACK 00004153 fails to open.
- After the fix, her original greeting/introduction reaches topic 0001E7D7,
  executes its original response/result, and opens normal 0% discount trading.
- Original Moira/Gob stock, mixed transfers and save/reload tests pass.
- Synthetic dialogue tests prove that a nonservice package permits an explicit
  ShowBarterMenu result, while an authored GetOffersServicesNow condition still
  rejects it; service conditions succeed for service packages, and hostility
  still blocks trading.
- The production controller bridge test now persists a nonservice merchant
  package and checks topic visibility, voiced phases, menu entry, purchase,
  cancellation and walk-away.
- Player regressions: 31/31 pass. v211's MS11 completion changes are preserved.
- Android build outcome is reported with delivery; physical headset validation
  is still required.

## Quest check

Install v212 over the current APK and talk to Moira in Craterside Supply.
Complete her original introductory conversation if required, then select
`Let me see what you have for sale.` Finish/skip her response and confirm both
inventories appear. Buy and sell, exit dialogue, then save/restart and verify
stock and caps persist. Existing quest-completion checks from v211 remain valid.
