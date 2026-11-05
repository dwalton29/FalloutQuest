# v311: original Vault inventory scripts and development-loadout removal

The birthday, G.O.A.T. and Escape scripts reset the player's inventory, equip
original clothing and the Pip-Boy, hand out weapons, and store presents for later.
These shared commands now operate on saved inventories. The runtime contains no
new quest IDs, quest-stage transitions, item IDs or quest-specific branches.

## Implemented behaviour

- Player `RemoveAllItems` without arguments removes ordinary inventory, including
  equipped weapons/clothing. Quest objects and unplayable biped items survive;
  original `SetQuestObject` overrides determine protection. The Pip-Boy therefore
  survives the authored childhood inventory resets. Destination/ownership/remove-
  quest-item variants remain explicitly unsupported.
- Player `EquipItem` and `UnequipItem` accept the native default/zero locking flag
  and optional message flag. Scripted equipment can select unplayable biped items;
  ordinary inventory UI restrictions stay in place. Save validation now accepts
  this legitimate equipment. Missing equipment is never manufactured. Persistent
  NoUnequip/NoEquip flags and NPC EquipItem remain unsupported. Equipment-message
  presentation and OnEquip/OnUnequip event parity are not implemented here.
- `AddItem`/`RemoveItem` support named and event-owner inventories as well as the
  player. NPC and container inventories use the existing original loot/template
  resolution and save data. AddItem's silent flag suppresses player item notices;
  NPC/container grants do not generate player notices. Count arguments accept a
  numeric script variable/global, as required by Escape's weapon-surrender scripts.
- `AddItemHealthPercent` preserves the supplied instance condition, including
  Amata's damaged pistol. Non-durable items still reject non-unit condition values.
- Scripted RemoveItem can consume equipped/quest items and clamps removal to the
  amount present. Loose copies are consumed first. Removing a saved NPC's equipped
  weapon clears its selected instance. Player Drop restrictions remain unchanged.
- `PipBoyRadioOff` clears the saved radio selection consumed by the radio runtime.
- `autosave` queues an effect in the private script candidate. Only a committed
  request reaches the existing native save adapter, including its publication
  barrier. It writes the current FalloutQuest save, not a separate Bethesda-style
  autosave slot. This does not make transient voice/animation queues reloadable.
- Automatic development-pistol grants are disabled for original scripted games.
  Both current character-generation state and the saved scripted arrival identify
  these sessions, covering existing v309/v310 saves and the period after Escape.
  Legacy development sessions retain their existing behaviour.

## Verification

Synthetic tests cover equipment/quest protection, unplayable saved equipment,
silent versus normal notices, variable counts, item condition, NPC/container
inventories, removal of equipped items, commit/rollback and rejection of
unsupported variants. The production world-script adapter test verifies that
preparation does not save and a committed autosave request flushes exactly once.
Older event/transaction tests used insufficient RemoveItem quantities as a fake
runtime failure. They now check successful clamping, and use unavailable or
ineligible equipment to retain meaningful runtime-failure/rollback coverage.

The original-data integration executes unchanged contiguous inventory portions
from CG02 stages 5, 44, 70, 95 and 100; CG03 stage 5; and CG04 stages 8 and 50.
It checks birthday clothing, the original BB gun, removal of the gun's quest
protection, adulthood inventory replacement, both Amata pistol handoffs, and
save/reload of the resulting items. These are subsystem tests, not a simulation
of the complete childhood quests. The existing original birth/name tests still
run independently.

## Remaining route to the vault exit

The immediate live birth blocker remains ShowRaceMenu: original race/face choices,
face preview and family appearance inheritance. Player scale/age/toddler behaviour,
original time-jump movie playback, SPECIAL book allocation, G.O.A.T./tag skills,
and remaining authored NPC/package/event support still require implementation.
This checkpoint does not bypass these scenes, automatically accept menus, or
advance the player to an adult start. The full vault opening is not yet playable.
Headset verification is outstanding.

Local validation completed: Vulkan-enabled Android ARM64 native library build;
the complete 52-test player CTest invocation (50 passed, two original-data tests
skipped by default); both skipped MQ01 tests passed separately with the supplied
ESM; the original birth/inventory integration passed; and the HDR-order,
render-dispatch, loading-pipeline, UI-layering and native-dispatch source checks
passed (24 source tests across those five suites).
