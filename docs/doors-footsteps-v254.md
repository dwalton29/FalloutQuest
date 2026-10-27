# v254: placed doors and player footsteps

The Small Ranch doors in Springvale use SubUrbanRanchDoorDestroy.NIF. The
NIF root is rotated +90 degrees around X; its geometry's long axis is raw Y.
REFRs 0009DF53 and 0009DF52 already contain the rotation that puts raw Y upright
in game Z. Baking the NIF root before applying those REFRs rotates the door
again. The old pipeline gives the door's height to a horizontal axis.

World mesh loading now excludes the replaced scene-root transform and retains
all child transforms. Standalone meshes, actors and UI keep their previous
contract. Authored collision roots and embedded door animation use the same
reference-local frame. No UV, winding or texture modifications are involved.

Footstep evidence was read from the user's original Fallout3.esm, first-person
locomotion KF files and Fallout3.exe (1.7.0.3). No game files are bundled.

| Clip | Cycle | Root travel | Contact keys |
| --- | --- | --- | --- |
| mtforward.kf | 1.2 s | 102.308 units / 1.46154 m | m:L at 0.4 s; m:R at 1.03333 s |
| mtfastforward.kf | 0.799999 s | 283.106 units / 4.04437 m | m:L at 0.333332 s; m:R at 0.733332 s |

The runtime decodes the installed original clips once on the audio worker.
Contact cadence scales with actual distance after collision resolution. Menus,
loading, lost focus, stationary movement and teleport discontinuities reset or
suppress the clock. A contact sounds only when a Havok or LAND surface supports
the resolved feet. Snap turns do not add walking distance. Physical room-scale
head movement is not treated as joystick locomotion.

Material dispatch and fallback categories come from the executable's footstep
sound initialization (0x8e84f0) and dispatch (0x8e945a). Tables at 0x1114928,
0x11149a8, 0x11149d0 and 0x11149f8 map Havok material IDs to original
FST<surface><action> SOUN names, left/right walk/run/stairs variants and fallback
chains. Terrain uses LTEX HNAM and the strongest painted LAND layer at contact;
packed collision uses subshape materials, primitive collision its leaf material.
Existing SOUN gain, loose/BSA resolution and randomized directory variants apply.
Missing animation data stays silent and logs FOOTSTEPS readiness.

Host validation covers original ranch-door geometry/collision and original KF
contact/root travel, root-versus-child transforms, stationary/menu/teleport gates,
walk/run wraparound, material fallback and grounded floor selection. In-headset
confirmation of both ranch doors and audible surface transitions remains useful.
