# Physical grips and falling — v263

Fallout-authored NIF collision remains the source geometry. Bullet 3.25 CPU
rigid-body/contact dynamics is a standalone VR adaptation, not Havok emulation.
Source is pinned to commit 2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5;
CMake rejects other commits. No Bullet demos, GPU compute or Python is included.
The upstream zlib notice is in third_party/bullet-LICENSE.txt.

LAND grounding now uses the same p00-p11 triangle diagonal as cold and streamed
rendering. Committed stitched heights and force-hide masks supply physical LAND
triangles too. Entering/changed cells rebuild; unchanged cells reuse their BVHs.
The active three-by-three LAND window retires old chunks. Animated door/gate
collision invalidates individual REFR chunks; full scene publication replaces
static chunks after the existing resident transaction settles.

The XR player uses a gravity-bearing controller around the existing authored
sweep/step controller. Grounded characters may step up; leaving support begins
accelerated descent. Vertical capsule sweeps prevent crossing floors even when
the travel in one frame exceeds a floor's thickness. Exact LAND height provides
initial-overlap recovery. Loading/arrival preserves the existing safe-spawn and
input/time barriers. Jump controls and fall damage are not introduced here.

Loose-object geometry and physical properties prepare on the existing asset
worker, not on grab/finger updates. FO3 bhkRigidBodyCInfo550_660 mass, centre of
mass, friction, restitution, damping and inertia are decoded. Original inertia
is transformed into render coordinates with Havok-to-metre squared scale and
converted to principal axes internally without changing model-space poses.
Authored zero mass remains immovable; it is not replaced by the VR fallback.
Each authored collision leaf supplies a convex hull; concave movable leaves
therefore use their convex envelope. NIFs without BHK use a hull of authored
visible geometry and a disclosed 1 kg/default-material VR fallback. Compound
rigid hulls are supported; articulated multi-body/ragdoll graphs are not rebuilt.

One render-thread physics world owns held/dropped clutter and runtime weapons.
World and object contacts, angular response, sleeping and fixed 120 Hz steps
replace bounding-box settling and contact-only damping. Swept convex children
also guard fast spinning thin items against tunnelling. Original character
horizontal collision remains separate; dynamic contacts participate in vertical
capsule sweeps. SI gravity 9.80665 m/s² is the existing documented VR choice,
not a recovered Bethesda engine default.

Grab selection measures the authored visible mesh surface (with collision-hull
fallback) after a cheap
bounds broad phase. Selection itself does not create/activate a body or mutate
ownership. The selected surface anchors a six-axis spring grip; the object is
not teleported by its centre. Reaching a plate rim holds it there. Held objects
remain collidable, including against other dynamic bodies. Springs, a 1.5 cm
palm gap, 3.5 cm hand proxy radius and 45 cm grip-break distance are VR policies.
The physical palm follows the blocked object while the target continues to use
raw controller motion, avoiding feedback from the constrained visible hand.
A private visual-mesh BVH supplies finger contact independently of coarse BHK.
Finger and thumb flex stop at first contact along their bend, including thin
surfaces crossed before the fully curled pose. Unheld hand proxies sweep against
the same world. These are contact proxies, not full anatomical hand simulation.

Collection, scene/stream retirement, weapon pickup/holstering and teardown remove
physical bodies and constraints. Runtime weapon IDs occupy a separate namespace;
no fabricated ESM REFRs are used. Canonical weapon save transforms and momenta
remain in the existing format. Resident save exclusion and gameplay clocks remain
in force; physics never advances while that workflow is blocked.

Host tests exercise the production world (gravity, stacking, rim anchors, held
wall contacts, thin rotating drops, principal inertia and capsule sweeps), the
actual player ledge/landing bridge, LAND interpolation, contact curl limits,
existing collision publication, actor attachment, weapon data and inventory
ownership. Android/Vulkan CI and headset acceptance are separate gates. On-device
contact comfort and CPU cost still require a headset run; no performance gain or
exact Havok equivalence is claimed.
