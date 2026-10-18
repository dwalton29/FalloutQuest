# v217 — Recover playable scenes after loading isolation

Parent: v216 `c07c0ab1e68c4568110201a598191304e4cb2555`.

## User-visible regression

On-device report: the isolated loading UI is slightly smoother, but after the
loading transition FalloutQuest can expose only a dark green/black cleared scene.
Initial loading duration also remains high. v216 passed host/CI, but there was
no headset logcat or actual VR visual acceptance capture.

## Code-confirmed regression and correction

The v216 scene GPU rewrite changed long-standing error policy. Any single
`UploadCpuObject` failure aborted the entire cell transaction, and a failed
`Q230UploadActorPart` caused the same fatal abort. The older playable v215
loader skipped unsupported optional NIF shapes and actor visual parts instead,
provided enough drawable world objects were prepared.

At the same time, v216 replaced proven GL texture/geometry upload with staged
image storage/mip generation and VBO capture, adding new GL failure paths.
When `AbortFo3SceneLoad` cancels a cold load, it can clear loading visibility
without ever publishing a first scene, leaving only the environment clear colour.

This checkpoint restores the v215 proven scene material and full-buffer upload
path and tolerant visual-part policy. It retains the v216 isolated loading-eye
draw, loading asset preparation, faster loading-catalogue scan, player-catalogue
copy elimination, shared upload admission and concurrent collision worker.
Original texture fidelity/LOD/scene readiness remain unchanged.

The source player catalogue CPU worker once again pre-decodes a bounded 64 MiB
set of unique visual textures, rather than decoding individual unique scene
textures serially in a render-thread-driven worker per image. Remaining cache
misses use the previous correct upload-time path, so no original assets are
omitted.

This is a targeted rollback of *unverified GPU resource-transfer changes*, not a
rollback of loading presentation isolation or gameplay systems. The 4 ms loop
slice limits cumulative GPU API work (not the maximum length of any individual
GL driver call). On-device measurement is still required.

## Validation

The host loading pipeline regression asserts that optional visual failures do
not abort the entire scene, that the original collision/publication safeguards
remain, and that the failed scene-specific staged image path is no longer used.
All previous host tests and native Android build must pass CI before on-headset
acceptance. Do not infer headset success from a green CI build.

Device acceptance: verify initial Megaton scene renders after loading, NPCs and
collision are present, re-enter interiors/exteriors, and compare time/animation
with v215/v216. For unresolved loading failures capture:
`adb logcat -d -s FalloutQuest:I *:S` plus `SCENE LOAD FAILED`,
`SCENE CPU READY`, `SCENE LOAD READY` and `LOADING PRESENTATION COMPLETE`.
Do not uninstall the APK or erase app data while diagnosing.
