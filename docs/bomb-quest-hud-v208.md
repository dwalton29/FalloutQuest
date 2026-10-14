# v208: scripted bomb activation and quest HUD

Starting main: `1abca3af1bf193765d7a170f85ec055c3134266b` (v207).

## Activation fault and repair

Reproduced directly against the supplied Fallout3.esm, not a reconstructed script:
`DispatchReferenceEvent(00014BC8, OnActivate, Player)` returned Unsupported with
`line 23: missing expression operand`, and no MESG was queued.

Original SCPT SCTX has CRLF line endings. `CompileResult` used `getline` after
argument-free ShowMessage, retained the trailing carriage return, and passed it
as a nonempty formatting expression. Expression validation rejected it before
execution. The atomic script runtime consequently rolled back all local-variable
and message side effects. Trim optional ShowMessage expression whitespace in the
shared compiler. No FormID or Megaton branch is used to repair this.

The interaction API's false result means no door transition. It is not a script
failure indicator. Diagnostics now distinguish ACTI dispatch status, script,
reference, rejection reason, queued messages and actual message presentation.
The existing separate-press menu guard is retained.

Original-record integration confirms Explosives 25 skill checking, low-skill
formatted MESG, all three original choices, cancel, disarm, MS11 stage 30,
authored objective changes, 200 Karma, original tamper sound, and save/reload.
It also confirms repeated GameMode ticks do not replay the selected button.

## Quest notifications and asset provenance

The canonical Player quest APIs generate notices when a named quest first
reveals an objective, on visible objective changes, and on genuine completion.
StartQuest/StopQuest alone do not mean player-facing discovery/completion.
Objective completion alone is an update. Repeated identical state mutations
produce no event. Multiple updates for a quest coalesce before presentation;
added and completed transitions remain distinct. Source-event, stage-result and
dialogue-result rollback boundaries truncate pending notices. Dialogue previews
restore that queue as well as quest state. Failed rigging therefore emits no
success notice.

Optional QNF1 Pip-Boy save extension persists the first-reveal history. Existing
save formats remain readable; legacy quests with objective history are treated
as previously announced. Restore clears transient notices, messages, sounds and
button delivery, so historical events are not replayed.

Recovered from the supplied original files:

* SOUN UIPopUpQuestNew, 0001EA39: fx\\ui\\popup\\ui_popup_questnew.wav.
* SOUN UIPopUpQuestComplete, 00022582: fx\\ui\\popup\\ui_popup_questcomplete.wav.
* SOUN UIQuestUpdate, 0004FB02.
* Fallout3.exe defaults: sQuestAddedText / sQuestCompletedText /
  sQuestUpdatedText: Quest added / Quest completed / Quest updated.
  These are overridden by the catalogue's authored GMST strings.
  Default-value references: executable file offsets CFF738/CFF744/CFF750.
* Character animation defaults: fade-in 2s (D01430), remain 4s (D01448),
  fade-out 2s (D0143C), stagger 0.1s (D01454). A static vertex delay plus one
  age uniform reproduces per-glyph fade without per-glyph draw calls or uploads.
* Fallout - Misc.bsa: menus/main/hud_main_menu.xml QuestReminder 400x200,
  QuestStages y40 and QuestAdded y80; prefabs/hudtemplates.xml's original
  left-justified font 7. Existing runtime FNT/TEX/DDS interpretation is reused.
* Existing HUDMain green, original item/Karma text and Karma icons remain.

Native VR placement uses the existing two-metre head-relative plane, with quest
heading/title at upper left and item/Karma messages at upper right. The queue is
shared, so these messages cannot overlap. The engine's full native objective
cinematic, checkbox list and scrolling sequence are not implemented here;
quest updates display the authored quest title. The glyph stagger and fade use
recovered executable defaults; overridden cinematic timing GMSTs are not yet
applied. Exact original engine screen coordinates are adapted for VR.

Sounds resolve through the existing original SOUN catalogue and async audio
worker/BSA/loose-file resolver. Sound starts once when the queued notice first
becomes displayable, outside stereo rendering. Missing effects log their exact
original path. No substitute audio or original game asset bytes are embedded.
The supplied attachments do not include Fallout - Sound.bsa; the headset's
installed archive/loose sounds must supply the two popup WAVs and update sound.

## HUD priority and composition

Loading, dialogue, script messages, Pip-Boy and lockpicking take modal priority:
notification display and lifetime pause. Existing session-focus clear policy
remains. Nonmodal item/Karma/quest entries use the existing bounded FIFO queue
(capacity 16, maximum backlog age 40s); one entry displays at a time.

Within each eye: world/held models -> modal and controller/loot UI -> existing
post compositor -> final notification colour pass -> depth metadata/loading
handling -> submission. This prevents prompts, loot and held geometry from
painting over notifications, and prevents postprocessing from subsequently
covering or tinting the foreground HUD. Perspective and head tracking are
unchanged. Depth testing/writes are disabled only within a cached GL guard;
blend factors, equations, program, VAO, buffers and textures are restored.
The pass does not alter the submitted world depth.

Geometry and shader resources are prepared once per logical frame; steady
stereo rendering performs one text draw per eye (plus an existing icon draw
when needed), no asset loading, readbacks or buffer uploads. The additional
notification shader is compiled once and evaluates glyph timing in the GPU.

## Remaining original-game dependencies

Stage 40 requires `MS11DetonatorRef.Enable`. General persistent enabling of
original world references is not available. The original rigging path remains
atomic: inventory, quest variables/stage, messages and notices roll back;
the Fusion Pulse Charge is retained. The diagnostic gives the exact unsupported
command. Rigging without a charge still executes the original failure message.

Full quest reward stages 200/210 include additional original commands such as
CompleteAllObjectives, RewardXP, disposition/world-reference changes and
AddAchievement. This patch does not implement that larger scripting capability
or fabricate quest completion. A completed fanfare is generated only when an
actual supported completion transition succeeds.

No headset test was performed. Host tests verify state and composition order,
not visual comfort, device audio latency or original-engine pixel equivalence.

## Validation commands

```
cmake -S tests/player -B /tmp/fq-host-player
cmake --build /tmp/fq-host-player --parallel 4
ctest --test-dir /tmp/fq-host-player --output-on-failure
/tmp/fq-host-player/event_script_tests /path/to/Fallout3.esm
python3 tests/render/test_font_consumers.py
python3 tests/render/test_notification_layering.py
```

Verified locally: 27/27 gameplay CTest tests passed after a full Player-layout
rebuild. Original ESM bomb integration passed. Font-consumer and final-pass
ordering checks passed. The production notification shader compiled, linked and
passed RGBA pixel checks on Mesa GLES for both simulated eyes, character fade
phases and the ordinary item-alpha path. No device testing is implied.

The workflow also compiles/runs notification_render_tests.cpp with GL mocks:
actual warm renderer, both eyes, state restoration, no reads/queries/uploads,
modal lifetime pause, and one sound consumption. See build-apk.yml for command.

The requested `./gradlew :app:assembleDebug` cannot run in a fresh checkout:
the repository has no Gradle wrapper. Its existing CI provisions Gradle 8.9,
Android 35 and NDK 27 and runs `gradle assembleDebug`.

## Changed files

* `.github/workflows/build-apk.yml`
* `app/build.gradle`
* `app/src/main/cpp/core/fo3-runtime-loop.inc`
* `app/src/main/cpp/dialogue/fo3-dialogue-results.cpp`
* `app/src/main/cpp/pipboy/fo3-pipboy-session.cpp`
* `app/src/main/cpp/pipboy/fo3-pipboy-session.h`
* `app/src/main/cpp/player/fo3-pipboy-player.cpp`
* `app/src/main/cpp/player/fo3-player-state.cpp`
* `app/src/main/cpp/player/fo3-player-state.h`
* `app/src/main/cpp/scripting/fo3-script-runtime.inc`
* `app/src/main/cpp/ui/interaction/fo3-item-notification-renderer.h`
* `app/src/main/cpp/ui/interaction/fo3-item-notifications.h`
* `app/src/main/cpp/world/interaction/fo3-interaction-runtime.inc`
* `tests/player/event_script_tests.cpp`
* `tests/player/interaction_runtime_tests.cpp`
* `tests/player/shoulder_notification_tests.cpp`
* `docs/bomb-quest-hud-v208.md`
* `tests/render/notification_render_tests.cpp`
* `tests/render/test_notification_layering.py`
* `tests/render/test_notification_shader.py`
