# v316: party interactions and final package actions

Continues the original opening through birthday cake, presents and the peaceful
sweetroll branch. Appearance editing remains deferred. There are no new quest
IDs, stage numbers or actor-specific routes in the runtime.

## StopCombat

Qualified and implicit NPC StopCombat commands use the canonical actor instance.
They clear that actor's saved hostile target, invalidate package selection and
queue live combat cleanup only after transaction commit. Other actors retain
their targets. The live adapter ends combat, cancels pending attacks/reload,
clears the combat route and resumes the suspended package wait. It cannot
resurrect dead actors. Failed transactions publish neither state nor actions.
The existing actor save record already preserves the cleared target.

Player-owned combat and combat-group-wide stop operations remain unsupported.
StartCombat and automatic OnStartCombat delivery are still separate gaps;
the aggressive Butch choices are not claimed playable by this checkpoint.

## Completed travel packages with final idles

Original package callbacks distinguish OnEnd from OnChange. Andy's cake package
plays the cake animation and his cutting idle on OnEnd; its OnChange script
advances the scene. Previously the completed Travel package remained active,
so that second script could never run while its entry condition remained true.
Package idles are also cached for original creatures/robots, including Andy;
humanoid dialogue gestures remain excluded from creature rigs.

For Must Complete Travel packages with an authored OnEnd idle, the runtime now
waits for a valid decoded clip's duration before retiring the package through
the existing OnChange transaction. Selection then evaluates the original
conditions again. Missing clips leave completion pending. Voice interruption
invalidates the completion timing. After restore the final presentation action
can replay without repeating the committed OnEnd result script.

This is a bounded subset, not a complete implementation of every package flag
or procedure action. Must Complete is bit 2 in xEdit's
[shared package definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsCommon.pas).

## Verification and remaining work

The original ESM integration extends the existing New Game journey for both sex
choices. It uses real greetings and choices to receive Palmer's sweetroll,
Amata's comic and Stanley's cap, complete the cake dialogue and give the
sweetroll to Butch. Original stage items set the all-presents checkpoint.
Save/reload still checks identity after those interactions.

Voice/movie completion, physical player movement, package approach and final
idle completion are recording fixtures. The cake fixture verifies the package
result scripts against the original master. A separate production package
runtime test checks timing, missing-clip refusal, one-time retirement and the
OnChange result. Combat tests cover preview, rollback, per-actor isolation,
save/reload, live delivery, pending attack cancellation and route cleanup.

The earlier v315 CI linkage fix is fully green before this checkpoint. The
opening still needs the talking-activator intercom bridge, aggressive Butch
branches, physical grab/release events, range/photo gameplay, G.O.A.T., Escape
and headset verification. This is not yet a complete exit from Vault 101.
