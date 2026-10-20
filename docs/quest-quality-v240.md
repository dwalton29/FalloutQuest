# v240 — 90% eye resolution, fixed foveation and Wasteland fog

The default eye dimensions are 90% of OpenXR's recommended width/height
(81% of its pixels). Both Debug and O2 Profile use the same setting.
MSAA stays 2x. The existing render_scale property permits 0.7 through 1.0
for comparisons; clear any old override to measure the new default.

## Foveation

Large opaque main-eye world and LAND draws use GL_QCOM_shading_rate if
advertised by the device, with a resolved glShadingRateQCOM entry point.
A central rectangle covering 70% of each axis remains at 1x1; four disjoint
peripheral bands use 2x2. Scissor intersections preserve existing clipping.
Each eligible draw is replayed in these five regions, preserving its original
geometry, per-pixel coverage/depth, MSAA and ordering. Draws below 3072
corners and blended layers retain one full-rate submission. This is fixed
foveation, not eye tracking, and is a regional VRS implementation, not an
OpenXR swapchain-only effect.

This deliberately avoids QCOM_framebuffer_foveated, whose scaled-bin depth
resolve/discard semantics conflict with the water and compositor depth paths.
Reflection, NPC, player, alpha, UI and post-processing passes stay full rate.
Prior shading rate and scissor state are restored when each scope ends.
Without the extension the renderer uses ordinary draws automatically.

Replaying regions increases geometry and CPU submission work. Net performance
must be measured on the headset; no FPS improvement is asserted. The original
telemetry counts every actual regional draw and its submitted vertices.
Use debug.falloutquest.foveation=0 before restarting for a matched comparison;
1 enables the default again. Q240 FIXED FOVEATION logs availability and mode.

Primary API specification:
https://registry.khronos.org/OpenGL/extensions/QCOM/QCOM_shading_rate.txt

## Wasteland distance

The outermost, coarsest Level32 terrain tier is omitted at asset discovery,
selection and draw time. Level16 > Level8 > Level4 > LAND remain. Object LOD,
landmarks, detail/collision residency and the LAND runway are unchanged.
The loading horizon gate requires the desired Level16 fallback tiles instead
of the removed Level32 tier, keeping the loader covered until that fallback
is resident. Its legacy internal output variable names remain compatible.

Fallout 3's existing WTHR NAM0 fog colours, FNAM day/night distances and fog
power still drive the effect. A render-only copy caps Wasteland fog near at
65536 game units (16 cells) and far at 98304 (24 cells), inside the existing
125000-unit far plane. Denser authored weather stays denser. Invalid distance
data uses this range with the already resolved authored fog colour.
Megaton, interiors and the underlying authored/time-of-day state are untouched.
World geometry and LAND share the same capped render environment.
This is a Quest distance adaptation, not a claim these shorter ranges were
Bethesda's original weather settings.

## Verification

CI runs portable tests for fog/world isolation, retained terrain selection,
and exhaustive disjoint raster coverage (including odd dimensions, tiny
viewports and pre-existing scissor clipping), plus existing render/GL/shader,
world, physics, player and APK checks.

Local execution and attachment inspection were unavailable in this session.
Headset checks still required: centre/periphery quality; water edges and
compositor depth; Wasteland transitions and horizon at different times of day;
matched GPU/CPU captures with foveation on/off. Inspect Q240 FIXED FOVEATION
before attributing any performance change to VRS.
