# Arrival and actor transforms — v262

Loading: Vulkan gameplay reused the active cell horizon clear during loading,
while the standalone GLES loading renderer cleared black. Vulkan now uses
black for loading regardless of the valid cell environment. Original green UI,
art, models and their material colours remain authored; world post-processing
still does not run during loading.

Arrival facing: use the completed CELL transaction’s original XTEL rotation,
including startup and script-delegated doors. FO3’s clockwise heading with +Y
forward maps to negative OpenXR yaw with -Z forward. Subtract the current
physical headset heading once per committed arrival. A pending, failed or
cancelled load cannot publish facing; no cached controller aim is involved.
Interactions no longer reset the player position/yaw merely because Activate
returned true (that includes ordinary containers/items and local doors).

NPC appearance: v254’s placed-door root replacement was accidentally applied
inside the shared CPU mesh builder to assembled NPC/player parts too. Rigid
face parts need the original NIF root rotation before Head-bone attachment.
NPC_ geometry now retains that root, with distinct authored-root/reference-root
cache entries so a world model cannot contaminate an actor’s bind geometry.
EGM vectors and normals follow the same retained transform. This restores the
previous face-part contract without changing the per-part Head attachment or
FaceGen texture row conversion.

Validation: host arrival tests cover cardinal/oblique/wrapped angles with
multiple physical headset headings, one-time commit consumption, startup and
invalid input. Root tests distinguish actor bind geometry/normals/morph vectors
from placed-door geometry. Actor runtime and loading/actor ownership guards
pass locally. Vulkan software-compositor regression supplies a coloured horizon
and checks uncovered loading pixels are black in both eyes. Full Android/Vulkan
CI must pass before distributing the APK. Headset visual validation remains
necessary. Available attachments contain the original ESM and Megaton meshes;
the full meshes.bsa shared link is blocked in this environment, so this turn’s
NPC validation uses synthetic transforms and the existing original-asset contract.
