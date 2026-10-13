# Ownership, theft and Karma — v206

Baseline: `345df75ce634ff69723d4fe10005eb4a12a806ff`, v205. The baseline had no newer ownership/theft implementation. Version: `0.69.0-ownership-theft-karma` (206).

## Dependency assessment

- `fo3-player-state.cpp` flattened CELL XOWN into references and blocked foreign-owned pickups. `fo3-container-state.cpp` blocked owned containers and rejected COED owner/rank/global metadata. These restrictions prevented theft rather than modelling legality.
- Both A pickup and shoulder inventory release already commit through `CollectFo3WorldReference` / `Player::Pickup`; firearm grip commits through `PickupWeapon` / `Pickup`. Collected-reference overlays already remove visuals/collision, survive streaming and persist in FQPS.
- Container stacks have stable instance IDs, bounded LVLI generation and all-or-nothing publication. Preserve that machinery and store COED metadata on remaining stacks; do not merge differing owners/ranks/globals.
- NPC_ ACBS Karma and SNAM memberships are already decoded in the immutable dialogue catalogue. Add FACT DATA flags and a template-aware ownership resolver rather than inventing faction membership.
- `RewardKarma` already owns the real saved player Karma. Extend its transient feedback queue and every script preview/rollback boundary; do not create another Karma value.
- Existing notification and audio-worker APIs support the original font, DDS assets, SOUN records and effect channel. Delivery belongs outside stereo rendering. Existing FQPS atomic file replacement and rejected-save blocking remain authoritative.

Original-record validation also exposed a pre-existing generic event-base fallback into the player-stat decoder (`Missing player stats`). The decoder now accepts only NPC_ FormID 7; event-only bases retain their script associations without being treated as another player. This does not change legacy catalogue fingerprints. A synthetic ACTI regression and the full supplied ESM cover this correction.

## Source audit and precedence

Audited the supplied Fallout3.esm, original Fallout3.exe, FALLOUT.INI and Fallout - Misc.bsa. No Bethesda assets are committed or packaged. Assets are loaded from the user's installation at runtime.

1. A non-null explicit REFR XOWN takes precedence over CELL XOWN. XRNK is the required faction rank; XGLB is retained as an unresolved global permission dependency.
2. Missing/null reference ownership inherits CELL ownership, including CELL XRNK. CELL DATA bit 5 (`Public Area`) does **not** erase item ownership. It is a trespass flag, not a licence to take contents.
3. A non-null COED item owner overrides the container/reference/CELL owner. COED's second uint32 is the required rank for FACT ownership or a global FormID for NPC ownership. Owner, rank/global and condition propagate through supported LVLI expansion. Null COED ownership inherits the source.
4. Player base FormID 7 is legal. FACT ownership is legal only with a non-negative player SNAM membership rank meeting XRNK/COED required rank. NPC_ Use Factions template bit 1 is followed with cycle/depth protection; Use Base Data bit 7 supplies owner Karma.
5. Ownership is independent of pickup validity, scripts, record/condition checks, container generation and locks. Unsupported owner/global permission semantics block taking instead of assigning an invented exemption or penalty. Viewing an unlocked owned container does not require permission to take. The existing dead-actor personal inventory path does not inherit CELL item ownership; explicit item metadata remains separately resolved.

Format interpretation is cross-checked against [xEdit's FO3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsFO3.pas), [GECK Reference](https://geckwiki.com/index.php/Reference), [Interior Data](https://geckwiki.com/index.php/Interior_Data), and [Faction documentation](https://geckwiki.com/index.php/Category:Factions). Public-area behavior is explicitly documented by GECK. No FNV reputation/bounty behavior is used.

## Taking and physical handling

Eligible owned references use the ordinary real-item transfer: original base FormID, count, condition, collected-reference overlay and collision removal. Repeat pickup fails before inventory/Karma/feedback changes. A now supports ordinary inventory pickup of eligible firearm references too; physical firearm grip retains its existing pickup-and-equip behavior. Both reach `Player::Pickup`.

Loose-object grip/movement remains physics only. Shoulder inventory release is the completed inventory transfer and can commit theft. Physical firearm grip is already an inventory/equip action, so it commits theft there. This is the existing VR weapon interaction adaptation, not a claim that vanilla's move-only Grab button equips or steals weapons.

Owned unlocked containers retain the floating right-stick/A interface. Locks, unsupported scripts, invalid generation and item restrictions remain in force. Viewing/generating does not charge Karma. Individual row legality is resolved from its own COED metadata and the current player/source ownership.

The floating panel's lifetime defines the VR container interaction: switching targets, losing focus/aim, loading, opening another menu or leaving the panel closes the session. The first qualifying successful take charges once; further qualifying takes in that session do not. Looking away and reopening starts another session. This adapts Fallout 3's container menu activation lifetime to the existing aim-driven floating panel.

## Karma rules

- Original ESM `fKarmaModStealing` FormID `000615A0` is **-5**. Use this GMST, never an item count multiplier.
- NPC-owned theft loses Karma if the owner is above the evil threshold and has no active membership in an Evil faction. `fAlignEvilMaxKarma` defaults to **-250** in the supplied executable (setting value at VA `011055D0`, alignment comparisons at `006E65B0`); an original GMST override is accepted.
- FACT DATA byte 0 bit 1 is Evil; byte 1 bit 0 is Track Crime. Non-evil crime-tracking faction property incurs Karma loss; Evil/non-tracking faction property does not. Legality/red highlighting remains separate from Karma eligibility.
- Witnessing is not required for Karma. No nearby actor is fabricated into a witness.
- Unowned, player-owned and permitted faction property, viewing, failed transfers and repeated collected-reference input do not lose Karma.
- Container loss is once per session's first qualifying theft, not once per stack/count. Reopening and successfully taking again incurs another loss. This lifetime behavior is documented specifically for Fallout 3 in [the Fallout 3 Karma reference](https://fallout.fandom.com/wiki/Karma_%28Fallout_3%29); the ESM determines the numerical modifier, while the community reference supplies the container lifetime behavior not encoded in records. This aspect still needs direct vanilla/headset comparison.
- Real saved Karma remains clamped to the existing -1000..1000 range and continues serving dialogue/scripts. Feedback retains direction at the limit; the original executable queues feedback after clamping too. Zero RewardKarma requests do not enqueue a notification.

## Original feedback and interaction HUD

| Resource | Original source |
|---|---|
| `UIKarmaDown` | SOUN `0005B540`, FNAM `fx\\ui\\ui_karma_down.wav` |
| `UIKarmaUp` | SOUN `0005B53F`, FNAM `fx\\ui\\ui_karma_up.wav` |
| Loss notification | `sKarmaMinorLost` / `sKarmaMajorLost`: `You've lost Karma!` |
| Gain notification | `sKarmaMinorGained` / `sKarmaMajorGained`: `You've gained Karma!` |
| Minor loss icon | `sKarmaMinorLostImage`: `Interface\\Icons\\PipboyImages_small\\Karma icons\\glow_karma_bad.dds` |
| Minor gain icon | `sKarmaMinorGainedImage`: same directory, `glow_karma_good.dds` |
| Major loss/gain icons | original corresponding GMSTs, `glow_karma_evil.dds` / `glow_karma_saintly.dds` |
| HUD definitions | Misc.bsa `menus\\main\\hud_main_menu.xml`, `menus\\container_menu.xml`, `menus\\prefabs\\hudtemplates.xml` |

Both original WAVs and all four icons were located in the connected `/Fallout3` Dropbox tree. Runtime uses installation files/BSA readers, not Dropbox network playback. Missing original audio/icon assets are logged; no generic substitute is supplied. SOUN SNDX static attenuation is handled by the existing audio catalogue/worker. No music-context reset or radio stop is introduced.

Executable `008C2B30` uses `iKarmaChangeThreshold` (default **250**, value at `0110276C`): gain >= 250 uses the major image, loss < -250 uses the major image. It queues the original text/image with a **2.0-second** hold. The existing notification queue's 0.5-second fade, backlog limit, metre scale, icon size and upper-right VR placement remain explicit VR adaptations. XML provides the Messages 460x90 region, font 7 and template icon; exact vanilla procedural icon animation/glow/positioning is not reconstructed. The accessible XML does not alone specify all engine-written crime/animation traits.

Normal prompt uses `Take`, illegal pickup uses `Steal`; `Steal`/`Open` are verified executable action strings. HUDMain INI RGB **26,255,128** is normal; HUDAlt RGB **255,67,42** is crime. Prompt, container title, individual loot entries, selected highlight and action/button tint follow resolved legality. Geometry/font/A artwork and VR panel placement are retained. Changing legality changes tint immediately even if the name/geometry is unchanged.

Notifications/sounds are consumed once per logical frame, outside both eye draws. Script previews, failed dialogue results, quest stages and source events restore feedback queue lengths with the authoritative state. Item transfer feedback and Karma feedback remain distinct messages from the single committed action.

## Persistence and guarantees

FQPS **v13** appends a bounded sparse table keyed by original container stack ID: owner, rank and global. Inventory, collected refs, depleted containers, weapon instances and v12 Karma stay in their existing sections. Catalogue fingerprints are unchanged. v1-v12 saves remain readable; v12 inventories could not generate foreign-owned COED stacks, so their missing metadata does not fabricate stolen ownership. Containers already generated in older versions retain their saved outcomes and contents.

Restore validates size, identity, duplicate IDs, owner forms, global references and faction/global combinations before publishing any state. Failed transfer checks do not mutate inventory/contents/Karma. Failed script transactions truncate feedback. Save restore clears transient feedback/session state, preventing notification replay after restart. The existing temp-file, file fsync, rename and directory-sync path preserves an earlier save on pre-rename failure. A disk error after a successful in-memory transfer is reported by the existing save path and retried; there is **no new promise of rolling gameplay back on disk failure**.

## Crime boundary and remaining limitations

`TheftEvent` carries player/reference/item/container/owner, quantity, CELL/world/player location availability and actual Karma delta after a successful transaction. It is a transient render-thread event with a bounded diagnostic backlog; current consumer logs it without inventing detection or NPC reactions. No full crime system, witnesses, confrontation, confiscation, arrest, bounty or pickpocketing is claimed.

- Global-gated ownership is retained but taking is blocked until its exact permission semantics are established.
- The current scripting subset has no SetOwnership/SetCellOwnership or faction-rank mutation command. No quest-dependent ownership changes are guessed. Immutable authored metadata is preserved for a future per-save overlay.
- No complete ownership-disposition exemption, stolen inventory tracking or NPC theft detection has been added.
- Exact vanilla procedural notification animation remains a comparison dependency; VR placement/fade is explicitly described above.
- Headset success requires testing; host tests cannot establish physical comfort, stereo appearance or actual device audio.

## Megaton headset checklist

1. Aim between an unowned item and owned property: green `Take` switches immediately to red `Steal`; return to green.
2. Move an owned loose object with grip without collecting: inventory and Karma stay unchanged. Release it into the shoulder inventory: exactly one transfer/loss sound/message; repeat A cannot collect again.
3. Take an owned item with A: actual count/condition enters inventory, object/collision disappear and Karma drops 5 where applicable.
4. View an unlocked owned container without taking: no penalty. Take multiple eligible rows in one session: one penalty. Look away, reopen and take a remaining row: one further penalty.
5. Confirm locked/scripted containers remain blocked and player/unowned property remains legal.
6. Save/restart and return: stolen reference stays absent, contents stay depleted, inventory/Karma remain and feedback does not replay.
7. Run with radio/ambient music playing: Karma effect overlays without resetting them. Check both eyes, prompt/row colours, icon/text placement and holding A.
8. Verify weapons, doors, dialogue, quests, NPC combat and Pip-Boy still work. NPC witness reactions are intentionally absent.

## Validation results before commit

- Player host suite: **25/25 passed**, including ownership, interaction/shoulder feedback, v12 migration, corruption/rollback, dialogue, scripts, NPC combat/packages/doors/furniture, weapons and Pip-Boy.
- Other CTest suites: assets **3/3**, world **5/5**, physics **1/1**, audio **2/2**; NIF **10 passed, 2 asset-dependent skips** (VR body and NPC combat asset fixtures require additional local game assets).
- All 12 Python source/shader checks passed, including world and interior shader validation. GL-state cache, timer/disjoint ring and stereo/reflection policy standalone regressions passed.
- Supplied ESM integration: ownership census **4,256 owned stealable / 32,075 legal eligible / 12,838 inherited illegal / 308 positive required-rank references**. The source-driven Megaton scenario selected loose REFR `000BB4D3` and container `00044FD8`, successfully took two container stacks with one penalty, and verified save/reload. These IDs are test output, never production exceptions.
- Full original player catalogue/actor census, Lucas dialogue/MS11 quest acceptance/save path, event source dispatch/scheduler/rollback and weapon records also passed.
- Downloaded original WAVs were inspected: loss stereo PCM, 32 kHz, **7.140 s**; gain stereo PCM, 32 kHz, **3.904 s**. Existing SOUN attenuation is retained.
- Local Java install-discovery test was unavailable because this environment has a Java runtime but no `javac`. The existing CI workflow runs it with a full Java 17 JDK.
- Android ARM64 compilation and CI results are recorded by the associated main push workflow; physical headset validation has not been performed.
