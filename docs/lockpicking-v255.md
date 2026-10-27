# v255: original lockpick animation and foreground presentation

Fixes the original-model animation and overlay presentation from v207.
No proprietary game assets are included in the repository or APK.

## Original data and rotation convention

Inspected the user's terminals/lockinterface01.nif and terminals/bobbypin01.nif,
and Fallout3.exe 1.7.0.4. The lock's frame is rotated +90 degrees about X and
its cylinder contains XYZ keys with a fixed -90-degree X rotation and a
0..-90-degree Z turn. NIF XYZ keys must compose as column-vector Rz*Ry*Rx.
The previous reversed multiplication made the cylinder tilt out of its face
plane. The pin's tension clip contains Euler decompositions near Y=-90 degrees;
reversing their order turns small shakes into large apparent tumbles.

The shared sampler now uses the correct order. Independent primary references:
[OpenMW NIF XYZ evaluation](https://github.com/OpenMW/openmw/blob/master/components/nifosg/controller.cpp)
and [OpenSceneGraph quaternion multiplication](https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osg/Quat).
OSG's quaternion multiplication convention reverses Hamilton product operands;
its `xr * yr * zr` corresponds to this engine's column-vector Rz*Ry*Rx.

Menu controllers seek absolute key times. Their authored LOOP flag does not
mean a fully turned cylinder or completed pin break should rewind. The new
clamped sampling option preserves ordinary actor/loading animation looping.
Each menu seek explicitly binds the selected clip's bone tracks.

The original executable places the pin model at (0,0,-1.6) in 0x943650 and
rotates it around the fixed point (0,0,0.6) in 0x944210. That transform now
replaces the incorrect rotation around the moving bobbypin node. Positive
right-stick input moves the pin toward screen-right; torque turns the cylinder
clockwise through the original Backward controller, including the screwdriver.
Forward supplies entry/insertion, pin Backward supplies tension, and Left
supplies breakage. Replacement restarts only the pin's insertion; torque stays
blocked during that insertion while the lock remains settled.

## Foreground rendering

Lockpicking now draws after world, hands, ordinary HUD and world post-processing.
Its models and text use a private modal depth surface. World models cannot cover
the minigame, while the pin, frame, cylinder and screwdriver retain self-occlusion.
Vulkan allocates the two-layer depth image lazily and retains it, caches the
compatible modal pipelines/framebuffers, and recreates swapchain framebuffers
safely. The scene depth submitted to OpenXR remains unchanged. GLES attaches a
retained depth renderbuffer only to the final eye colour framebuffer, then
detaches it. Scissor/stencil/offset state cannot hide the modal and is restored.
The stationary 1.4 m VR panel remains the existing presentation adaptation.

## Verification

Original-file checks cover face-on clockwise cylinder rotation, pin left/right
orientation, fixed pivot, bounded tension shake, held break endpoint, and fresh
replacement insertion. Synthetic tests cover combined XYZ rotations and clamped
versus ordinary LOOP playback. Existing actor and loading/door animation tests
also pass. The real software Vulkan test places both modal pieces behind a
nearer world model and checks their visibility and mutual depth in both eyes,
direct and legacy output, buffered frames, and swapchain recreation. It also
reads submitted compositor depth to verify that the world depth is retained.
Physical Quest presentation still needs headset verification.
