# v312: defer appearance editing and support the SPECIAL book

Appearance editing is explicitly deferred at the user's request. The opening
keeps the selected sex, entered name, and existing race/face assets. There is no
race editor, generated face, or automatic SPECIAL allocation.

## Original-script appearance handoff

`ShowRaceMenu` records a saved, temporary menu owner. The next scheduler tick
runs the existing original MenuMode blocks without advancing GameMode time; the
owner's successful transaction closes the deferred menu. Normal GameMode resumes
on the following tick. An opening menu also stops later quest/reference GameMode
work in the current tick. This prevents Dad's original race-match logic from
running before the original menu-open transition.

The source scripts themselves perform the birth stage changes. There are no new
quest IDs, stage numbers or character-generation routes in the runtime. Menu
ownership survives a save taken between the request and the MenuMode tick.
Requests from owners without a running quest MenuMode handler close after that
single menu tick. A failed owner handler remains an explicit script error.

The original player-targeted `MatchRace`, `MatchFaceGeometry`, and `SetPCYoung`
default-hair requests are accepted as explicitly deferred cosmetic operations.
Their existing appearance assets are retained. This is a temporary policy,
not an implementation of those appearance operations. Matching to arbitrary
non-player targets and malformed arguments still reject.

## SPECIAL book

`ShowSPECIALBookMenu` and its original `ssbmp` alias use the point total supplied
by the script. The original Vault book supplies 40. The reusable menu supports
any valid total from 7 through 70, with each of seven attributes in 1..10.

The menu uses a controller list in the existing original MessageMenu frame;
it does not reproduce the original illustrated book pages. Attribute labels come
from original AVIF data. Up/down selects an attribute, left/right or A/B changes
it, and A on Done confirms once all points have been allocated. Opening/held
buttons and tracking restoration cannot accidentally change or confirm values.

Draft allocations are separate from canonical player attributes and persist in
save v30 (`CUI1`, following the existing v29 identity tail). Previous save
versions remain supported. Confirmation applies SPECIAL through the existing
attribute API, updating health/AP/carry calculations. It adjusts skill values by
the change in governing SPECIAL and Luck contributions, preserving existing
skill offsets/bonuses within this runtime's integer 0..100 representation. It
does not implement the later G.O.A.T. or tag-skill menu.

The two multiplier defaults were recovered from GameSetting records in the
supplied Fallout3.exe using the existing PE recovery method:

| Setting | Native value | Record file offset |
| --- | ---: | --- |
| fAVDSkillPrimaryBonusMult | 2 | 0xcf8bfc |
| fAVDSkillLuckBonusMult | 0.5 | 0xcf8b54 |

Installed ESM overrides take precedence. This is not full floating-point
actor-value or modifier-stack parity.

Both character menus use the existing gameplay, input, world-clock and resident
publication barriers. Closing SPECIAL commits the allocation; it does not set a
quest stage. The original book activation sets its own stage before opening the
menu, and the original stage-done guard prevents repeated activation afterwards.

## Validation and limits

Synthetic checks cover one MenuMode tick, stopping later GameMode work, menu
save/reload, budget/canonical-state separation, controller neutral/focus handling,
confirmation, derived stats/skills and transaction rejection. The interaction
adapter test verifies real modal/resident barriers, draft saving and release on
Done. Original MessageMenu geometry with the original font metrics keeps all
seven attributes and the confirmation row inside the visible panel.

The original ESM integration now drives both birth sex branches through name
entry, the deferred appearance request, original menu-open/menu-close stages,
family matching deferral, and the final birth fade (CG00 stage 90). Voice
completions and gene-projector animation capabilities are host fixtures; these
checks do not prove headset audio, animations or rendering. The birth test's
world-mutation callbacks commit canonical state without building a scene.

A separate original-data test starts with access to the SPECIAL book, executes
its unmodified OnActivate script, checks the authored budget and stage result,
allocates points, and verifies the original one-time activation guard.

The toddler transition remains blocked by player scale/toddler support,
AutoDisplayObjectives, and original time-jump movie playback. These have not
been silently skipped. Completing the full Vault opening still requires these
and the remaining childhood/escape gameplay features. Headset verification is
outstanding.

## v311 CI failure and validation

v311 compiled both APK variants successfully, then failed the optimisation guard
with `Baseline Debug must not have the O2 profile marker`. The checker labelled
builds from directory names, which is unreliable when a debuggable Profile uses
a native Debug directory. It now identifies requested configurations from the
independent `FALLOUTQUEST_OPTIMIZED_PROFILE` CMake cache option, then checks the
actual compile flags of every emitted configuration. Debug explicitly requests
OFF. The profile marker, final `-O2`, retained diagnostics and both APKs are still
required; an actual optimised baseline cannot pass by being relabelled from its
compiler flags. Six Python regression tests cover both AGP layouts, wrong
markers, optimisation order, NDEBUG, missing baseline and multiple configurations.
These tests run before the real APK/profile verification in CI.

Local validation completed: Vulkan-enabled Android ARM64 native library build;
50 player tests passed in the full 52-test invocation, with the two original-data
MQ01 tests passing separately; the expanded original birth/SPECIAL/inventory
integration passed; all 24 tests in the five loading/render/UI source-check
suites passed; and all six native-profile checker regressions passed. Final APK
packaging and verification run in GitHub Actions.
