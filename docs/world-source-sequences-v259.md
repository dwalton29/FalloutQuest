# v259: source-driven world triggers and object controller sequences

The exterior Vault 101 reveal and Megaton gate mechanism now have a generic
source-event path. Runtime adapters contain no location, reference, quest,
sound, image-modifier or animation-group IDs for either sequence. Original
ESM/NIF assets remain external; no original assets or script bodies ship in
this change.

## Original chains inspected and exercised

- `CG04sunBlindSCRIPT` (`0002147D`) is attached to the ACTI underlying placed
  XPRM trigger `000ACD96`. Its player entry event enables `RadioEnclaveREF`
  and `SpringvaleEyebotREF`, plays `MUSExitTheVault`, conditionally applies
  `DEMOEyeAdjustISFX` between game hours 5.4 and 19.2, and sets its own
  `doOnce`. The original modifier has a ten-second duration, BNAM blur
  2.5→0 and NAM3 warm white fade alpha approximately .862745→0.
- `MegatonGateTriggerSCRIPT` (`000BF78A`), on placed XPRM trigger `000BF789`,
  writes `MegatonGateHouseREF.OpenNow` and Weld's `playerNear` numeric local.
  The gatehouse's `MegatonGate01Script` plays `Forward`, waits for
  `IsAnimPlaying == 0`, then selects `Left`. Its `OnLoad` restores the open
  idle when `opened >= 1`. The source gate opening does not wait for Weld's
  speech; no extra dialogue condition was invented.
- `Architecture/Megaton/MegatonGateHouse01.nif` supplies SpecialIdle, Forward,
  Backward and Left. Forward/Backward are fifteen-second clamped sequences;
  Left is a three-second loop. Each has eighteen transform tracks plus
  visibility, alpha, self-illumination and particle-emitter channels. The
  separate teleport entrance door does not supply these controller sequences.

## Runtime behavior

Original XPRM box/sphere data is retained, transformed by the REFR's scale and
rotation, and checked against the VR player's game-space head position at the
existing 4 Hz simulation cadence. Player entry/leave events do not depend on
rendering a trigger mesh. Disabled references and other CELL/world locations
are excluded. Residence tracking delivers `OnLoad` to newly resident objects;
ordinary visual re-publication retains the existing animation and does not
manufacture a second load event. Unsupported programs are logged and delivered
without effects; resource-busy transactions can retry. This remains a bounded
Quest spatial adapter, not the original Havok trigger/capsule event dispatcher.

PlayGroup resolves a group only from that reference's decoded original NIF.
PlayGroup and image-modifier requests are transactional queues, and playback
state is part of candidate publication/rollback. IsAnimPlaying uses the same
pause-aware clock, source duration and cycle type as presentation. Immediate
start mode 1 and the original object scripts' mode 5 restart from the start.
Mode 5's restart behavior is a bridge policy inferred from these object scripts,
not a claim that the full Bethesda animation-manager flag behavior is recovered.
Normal mode 0 is accepted only when no sequence is active; other initialization
modes and queued transitions reject explicitly. GetSecondsPassed receives the
accumulated script tick interval rather than one stereo frame.

World objects retain NiVis, NiAlpha and NiMaterialColor interpolators, including
step boolean keys and invalid-value sentinels. Geometry hidden at load is retained
only for decoded animated objects and stays hidden until its authored visibility
permits drawing. Per-shape scene transforms move the geometry and culling bounds.
BHK collision target-block identities survive flattening; per-node deltas move
the corresponding collision leaves without removing the rest of the gatehouse.
Collision updates are bounded to 10 Hz plus the final endpoint. Unchanged
collision targets do not rebuild the grid. Bind-pose caches remain immutable,
and prepared collision exchanges retain these node transforms.

Animated IMAD BNAM, NAM3, TNAM and cinematic saturation/brightness/contrast curves
are sampled over their source duration in the post pass on GLES and Vulkan.
The original base image space is never modified cumulatively. Blur uses a
five-tap post-process approximation. Single active animated modifiers are
supported; stacking, all HDR/DOF/radial channels and unidentified channels are
not implemented. Expiry restores neutral presentation, and focus/loading/pause
freeze elapsed time. Save restore clears transient effects and restarts source
residency, while saved script locals and enabled-state overlays remain persistent.

The Vault enable transaction admits resource-free CREAs and off-scene radio
transmitters only when the complete affected closure has no live resource
conflict. Radio terminals in the current CELL/world still require the resident
coordinator. Radio availability now queries the canonical enable overlay rather
than immutable ESM initial flags. Creature rendering/AI is still absent, so an
enabled eyebot is persisted but is not manufactured as a substitute model.

## Explicit remaining work

This does not complete the Vault character-creation quest or its scripted cave
exit door: name/race/SPECIAL/tag menus, face matching and other CG04 operations
still need native implementations. No forced quest stage or default-door bypass
was added. Weld's creature rendering and SayTo/SayToDone dialogue, particles,
animation sound keys and the full source animation blending/queuing system also
remain unfinished. The emitter channels decode and sample, but there is no claim
that the gate's sparks or smoke render yet. Existing unsupported commands keep
rejecting the entire event transaction.

## Validation

- `world_script_tests`: transformed XPRM entry/leave, CELL isolation, queued
  effects, candidate isolation and exact IsAnimPlaying completion; optional
  original ESM mode runs the actual Megaton trigger/gate/OnLoad chain and Vault
  enable/music/IMAD/doOnce chain. IDs occur only in this integration test's
  editor-name lookup, not in runtime adapters.
- `world_script_runtime_tests`: production event/action/render adapter with host
  platform doubles; source-driven playback, node transform and alpha at midpoint,
  distinct collision targets, source idle transition and retained streamed pose.
- `world_animation_tests`: step visibility/linear alpha plus optional original
  gate NIF decoding, all channel families and advancing transforms. An in-memory
  hidden-flag perturbation verifies retained hidden geometry; UI animation decoding
  remains strict about channels its renderer cannot consume.
- Collision regression covers separate moving/static targets, immutable cached
  bind geometry and suppression of updates for animated nodes with no collider.
- Local original-data runs use the supplied Fallout3.esm and Megaton NIF. Android
  CI performs the complete host regressions, GLES build and Vulkan shader build.
  Headset visual/playthrough validation remains necessary.
