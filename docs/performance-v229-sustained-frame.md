# v229 — Sustained-frame and NPC-arrival CPU preparation

VersionCode 229; versionName `0.88.0-sustained-frame-and-arrival-prep`.

## Verified baseline (uploaded v228 Megaton capture)
- Active median full frame work 26.28 ms; clean gameplay approximately 23.3 ms.
- Quest 3S target 72 Hz = 13.89 ms; GPU eye queries missing (negative sentinel).
- NPC arrival part upload has caused 145–234 ms one-frame stalls; full upload remains on GL thread.
- Baseline reflection/draw counts are viewpoint- and resident-dependent. No FPS gain is claimed without a controlled headset comparison.

## Changes
1. **Original NPC arrival asset worker:** Expand the already-decoded NIF geometry into the same 18-float stream while the worker is preparing the original actors, with immutable scene-centre coordinates captured from the main thread. CPU-predecode up to 24 distinct diffuse/normal/glow/environment-mask DDS textures, bounded to 32 MiB per arrival batch. Do not stage generated FaceGen images. The batch owns decoded images so cancelled scene publications discard them. The main frame moves only the next mesh part's images into the existing texture upload route; OpenGL calls remain on the GL thread. One part per frame is preserved; large individual glTexImage2D/glGenerateMipmap/glBufferData calls **can still stall**.
2. **Original NPC animation:** Precombine the dialogue root and rig scene-placement once per rig, rather than one extra 4x4 matrix multiplication per weighted bone/palette slot. The authored animation clip, per-bone transforms, GPU skinning and visibility are preserved; normal float associativity can vary slightly.
3. **Terrain:** Cache base and painted LAND draw lists until the source batch vector changes. Rebuild on world upload, streaming window publication/retirement and reset. Preserve original base-then-painted draw sequence and blending, all textures, authored geometry, and Q20.18 culling rollback. Eliminate unused frustum reconstruction while culling is disabled.

## Acceptance and rollback
- Compare v228 and v229 at the same Megaton viewpoint, NPC population, head motion and capture settings. Record clean median/p90, active median/p90, one-part arrival upload p90/p99, 1st/2nd eye CPU, terrain draw calls and GPU timing availability.
- Verify full painted LAND visibility, reflections, weather/lighting, 1:1 NPC FaceGen/skin/animation, weapon interaction, exterior streaming and CELL transitions.
- v229 is a CPU-preparation/submission improvement, **not** a guaranteed 72 Hz solution. GPU upload stalls and GPU-bound draw work need future dedicated passes if the headset capture still exceeds 13.89 ms.
- If regression: revert this single commit; do not restore unsafe terrain culling or reduce asset quality.
