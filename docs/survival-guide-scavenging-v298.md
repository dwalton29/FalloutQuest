# Wasteland Survival Guide: foreword and Super-Duper Mart (v298)

Scope: original MS03 opening conversation, accepting the scavenging assignment,
food-only or food-and-medicine completion, and returning to Moira. The complete
three-chapter quest is not claimed implemented. No MS03 IDs, stage shortcuts,
custom dialogue or task-completion checks were added to production code.

## Original progression

Source: supplied Fallout3.esm, its INFO results, QUST stage items, original SCRI
and enable-parent links. Original files are required at runtime and are not
copied into the repository or APK.

| Checkpoint | Original records and effects |
| --- | --- |
| Foreword | Moira's GREETING links to the vault questions. All five replies use the original INFO results. The available armored suit is disabled in the shop and its inventory version is given once. |
| Assignment | Book explanation and acceptance execute stage 5; the food task's acceptance enables its target parent, displays objectives 10/12, records its start time and reveals the Mart marker. |
| Pharmacy | The Super-Duper Pharmacy Key is in the authored metal box. Its XLOC association opens the pharmacy door without consuming the key. |
| Food | Food Storage's MS03FoodRefrigeratorScript runs on activation, delegates to normal container access, completes objective 10 and displays return objective 16. |
| Medicine | Pharmacy Supplies' MS03MedicineKitScript records medicine and completes objective 12. Both collection orders retain the original variable transitions. |
| Reinforcements | The original intercom trigger's OnTriggerLeave selects both loudspeakers' INFO, enables its linked group and saves DoOnce. |
| Turn-in | Original dialogue chooses stage 10 (basic) or 14 (bonus), finishes objective 16 and records task/style/bonus tallies. The levelled food reward is resolved from LVLI. Bonus completion gives the original Food Sanitizer and note. The overall guide remains running. These task stages author no XP reward; none was invented. |

## Shared engine fixes

- Source tokenization accepts if(...) and elseif(...) without spaces.
- Expression syntax and block structure remain checked throughout a program.
  Runtime operand capabilities are required when their branch executes; skipped
  nested conditions and already-taken ElseIf arms do not execute queries.
  Resolvable original bindings and provenance checks remain in captured plans.
- Script AddItem accepts levelled lists using the same resolver as container
  inventories: player level, flags, count, nested entries and chance-none rules.
  Concrete item notifications and outcomes are committed atomically. The runtime's
  existing deterministic loot RNG policy is reused, not claimed Bethesda RNG parity.
- Scripted containers receive their original OnActivate on A. Their Activate
  command prepares the real inventory transactionally and queues its normal
  loot action. Aiming alone cannot execute that script or expose its loot.
  Delegation ends on aim loss, cell leave or reload; locks and enable state are
  checked again at delivery. Taking a row does not rerun OnActivate.
- Key unlocking is independent of the script gate. A script must still delegate
  the default action. The matching key remains in inventory and the lock overlay
  saves the unlock. Valid scripted containers can enter the shared lockpicking
  system without bypassing their activation script.
- TACT voice definitions use their original VNAM field. Scripted speech admits
  an enabled talking activator without requiring NPC health or a skinned mesh.
  Ordinary NPC talk and dead-actor restrictions are retained.

## Verification and remaining limits

ms03_progression_tests with Fallout3.esm uses original dialogue selection and event
transactions, not forced stages or quest-variable seeding. It covers all five
foreword answers, assignment, real key-box transfer, the pharmacy lock, both
collection orders, basic and bonus turn-in, a snide bonus answer, levelled food,
sanitizer/note rewards, linked reinforcement state, return conversations, saves
at intermediate checkpoints and after turn-in, and prevention of repeat rewards
or intercom speech. Its world adapter commits the actual reference projection
without renderer resources. It does not simulate walking, combat or headset audio.

CI runs data-independent control-flow/LVLI regressions. The production interaction
and world-action adapters are exercised separately for A-to-script-to-loot, key
unlocking, transient delegation, re-locking before delivery and non-actor speech
with actual audio-completion tokens. Existing dialogue, barter, lockpick, bomb
and quest regressions remain required. Original assets are unavailable to CI, so
original-data runs are reported separately from host and Android build outcomes.

Outside this checkpoint: radiation, Minefield and later chapters; optional
Protectron automation; Food Sanitizer passive ingestion effects and its inventory
OnAdd/OnDrop notifications. The sanitizer and its authored instruction note are
awarded/persisted, but the inventory-item event and ingestible-effect runtimes
are not completed here. Attribute-gated turn-in answers, the lie route, early
scavenging before accepting, Moira's death/ghoul variants and physical traversal
are not established by this checkpoint's tests. Speech uses the existing serial
ambient voice queue; simultaneous spatial loudspeaker playback is not claimed.

## Headset checkpoint

1. Talk to Moira in Craterside Supply. If this save has not passed her introduction,
   answer the original vault questions, ask about the book and accept the food task.
   Existing progressed saves retain their original dialogue conditions; no reset is added.
2. Check the journal's food/optional medicine objectives and revealed Mart marker.
   Leave Megaton, travel to and enter the original Super-Duper Mart.
3. Collect the Pharmacy Key from the authored metal box. Open the pharmacy door;
   verify the key remains in inventory. Activate Food Storage and Pharmacy Supplies
   with A, then take their real loot rows. Check objectives as each is opened.
4. Cross the original intercom trigger when leaving that section. Listen for the
   returning-raider broadcast and check that its enabled actors appear. Leave and
   return, then reload; the broadcast must not repeat.
5. Save after collecting food or medicine, reload, return to Moira and turn in the
   task. Verify the food reward and, for the optional objective, sanitizer/note.
   Return/reload once more and verify no duplicate reward or suit gift.
6. A separate food-only run should permit the basic report without granting the
   medicine bonus. Recheck normal Moira barter and Lucas's bomb disarm/reward path.

No physical headset is attached to the implementation session. Android compilation
and original-data host tests do not establish the physical checks above.
