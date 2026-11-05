# v315: birthday setup, package topics and v314 CI fix

Continues the original opening through the first birthday party objective.
Appearance editing remains deferred. Runtime changes use original data and
shared command/package support, with no embedded quest routes or stage advances.

## Original birthday setup

IgnoreCrime stores a per-reference actor flag. GetIgnoreCrime reads that same
state in source expressions and original CTDA function 192. The flag is saved
in the optional v32 ICR1 tail, including saves taken during a pending movie.
Versions 1–31 remain readable. Invalid actors/flags reject and failed script
transactions do not publish changes. Direct damage and self-defence remain
active. The engine’s theft witness/reporting consumer is still unresolved;
this adds the authored flag and queries, not a complete crime AI system.

Assignments to GameYear, GameMonth, GameDay and GameHour now modify the
canonical clock used by schedules, lighting, queries and saves. They never
create mutable-global shadows. Gregorian dates, zero-based months, integral
date fields and hours in [0,24) are validated. Each intermediate calendar
assignment must be valid. DaysPassed and TimeScale writes remain unsupported.
Calendar changes participate in transaction rollback. This allows the original
birthday script to set its date rather than retaining the adult-era calendar.

Unqualified EVP/EvaluatePackage now targets the implicit actor, alongside the
existing qualified form. This is required by Old Lady Palmer’s original result.
Command rejections include command type, reference and value for investigation.

## Dialogue package topics

The original PKDD layout is decoded separately from package procedure scripts:
FOV, topic, flags and Conversation/Say To type. Layout was checked against the
[xEdit FO3 definition](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsFO3.pas).

Say To packages with a supported configuration use their specified topic, then
the existing authored INFO selection, voice and result pipeline. They stop
repeating after real speech completion records package completion. Failure or
missing audio cannot claim success. Package speech sends package completion
rather than the SayToDone event reserved for script-issued Say/SayTo. Disabled
references are rechecked at delivery and cannot start ghost speech. Deferred
package-completion publication is retried through the existing resident barrier.

This supplies Dad’s original post-gift party line. The existing AI approach
code invokes this path; it no longer substitutes a generic GREETING for a
specified Say To topic. Explicit-topic Conversation packages and nonzero
PKDD control flags remain unsupported. Full FOV/target-control/animation parity
is not claimed.

ResetPipboyManager queues a post-commit reset of physical Pip-Boy tracking and
menu preparation. ForceRadioStationUpdate queues an immediate station/context
refresh and refreshes broadcast state without restarting the tuned playlist.
Station availability still uses original transmitters and enabled references.
Failed script transactions cannot deliver either presentation action.

## Validation and limits

The original ESM integration runs both sex choices from New Game through birth,
name entry, the deferred appearance step, toddler playpen/SPECIAL/Bible/exit,
the nine-year movie request, original child race/date/inventory setup, birthday
opening dialogue, Pip-Boy gift and CG02 stage 12. It checks restored movie/crime
state, original age links, enabled radio, movement/activation controls and the
queued manager updates. No test-only stage commands are used in this journey.

Voice/movie completions, projector capability, player movement, actor arrival
poses and resident publication are fixtures. The birthday package-arrival
fixture uses the actual original package/topic, then real INFO scripts. This
is not evidence of a complete headset playthrough or simultaneous group voices;
the current speech adapter serializes scripted voices.

Synthetic tests cover flag/query isolation and save/reload, transactional
calendar writes and rollback, leap dates, direct self-defence, PKDD decoding,
package speech completion without SayToDone, and committed/reset action
publication. The source/scene checks and Android ARM64 compilation are also run.

v314 CI failed before APK compilation because a source assertion still required
the pre-package actor-validation expression. It now requires the expression
covering both INFO and package actors. The original association checks remain.
The entire scene-collision/publication source check passes locally.

Remaining work includes birthday presents/cake/Butch branches (including
StopCombat), later range/photo gameplay, G.O.A.T., Escape, and headset testing.
The first party objective is this checkpoint’s endpoint; leaving Vault 101 is
not yet complete. Original Video files are still required for actual movies.
