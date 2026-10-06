# NPC runtime v171 candidate

Based on main 1fd5d5fe9e541b09cc324478bb7230efca4863f9. Version 0.45.2, code 171.

## Original assets examined

Dropbox `/Fallout3/Fallout3.esm`, 288,771,262 bytes. The record walk ends exactly at EOF after 718,952 records. Actual Lucas greeting INFO 0003DA20, three linked topics 00003B80/7F/7E, response scripts and follow-up choices execute in the production dialogue tests. Megaton has 17 NAVMs, 1,568 triangles and 3,602 loaded neighbor links.

Original talking KF `talk_toplayerlhcasualb100.kf` decodes as 59 tracks, 29 compressed tracks and samples 360 frames on the original 66-bone skeleton. IDLE `LooseTalkToPlayerLHCasualB100` resolves the path from the ESM. Also decoded original `talk_lhidle255.kf` (BS version 25): 60 tracks, 21 compressed tracks. NifTools nif.xml confirms the single Anim Notes reference for BS versions 24..28.

## Movement

The old 0.01-unit portal comparison rejected 86 reciprocal Megaton external links. Real endpoint gaps range from 0.183 to 14.274 game units. A bounded 16-unit seam tolerance now requires the target edge to link back to the same source triangle. Internal edges retain their strict comparison. All 3,602 real loaded links are accepted; disconnected fixtures remain rejected.

Every walking step now projects its horizontal position onto the current route triangle and obtains Z from its plane. This removes gradual height interpolation from an off-surface starting placement. Wander/Sandbox can choose positions inside the current triangle and partial-radius boundary triangles. PLDT near-current anchors are retained for the active package. Targets in another world/interior scene are logged and excluded from this supported movement bridge.

Lucas has an authored Travel package MS11SimmsWaitForGreeting (0003DBCE), guarded by LucasSimmsRef.Greet == 0. His original LucasSimmsScript OnActivate sets Greet to 1 for the player, then activates. Talk now dispatches that event through a validated interpreter for local-variable equality, player activator checks, local numeric assignments and activate. Unknown commands reject the complete event before any variable is changed. Other NPC script events are not interpreted.

## Dialogue completion and animation

Android completion timing queries now catch IllegalStateException before release/native notification. This removes an uncaught exception path at exactly the voice-completion boundary. It has not been established as the headset crash root cause. Added stage logs: audio-complete, end-result, choices-built, panel-ready and panel upload. If the device still crashes, capture the AndroidRuntime/native fatal log and the final DIALOGUE stage.

Speaking uses the original casual talking KF, blending back to the original listening IDLE during choices. Playback-versus-choice selection is a VR presentation bridge, not a full Bethesda idle-condition tree. This adds body/hand/head gestures; authored LIP/TRI facial phoneme deformation is not implemented. Walking speed remains the existing explicit 1.05 m/s VR bridge. Unsupported package scripts/procedures, furniture, doors/cell traversal, GameMode and combat/death event execution remain unsupported.

## Validation

Host player suite (10 passes), NIF suite (8 passes and one optional asset skip), ASan/UBSan real-ESM Lucas dialogue (leak detection disabled because this environment is traced), real ESM Lucas dialogue and navigation tests, original talking clip sampling, and Android CI build are checked for this candidate. In-headset terrain alignment, animation presentation and crash elimination still require device verification.
