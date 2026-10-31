# LOD materials and walkway support — v305

Source baseline: main 67af28c. Originals inspected: supplied Fallout3.esm, Fallout3.exe, shaderpackage017.sdp in Shaders(1).zip, supplied Megaton meshes, and the original SuperDuperMart01/radio-tower/native block meshes and DDS atlas from the connected game files. No game assets are committed.

## Distant terrain

The native terrain LOD previously replaced live weather lighting with `0.48 + 0.52 * Lambert` and a fixed direction. The original SLS2003/2006 landscape-LOD pixel shaders decode world-space normals as `2 * texture - 1`, dot them directly against game-space LightData, and clamp the combined ambient plus signed directional contribution. Their normals are not tangent-space normals and are not renormalized. The common world shader now uses that lighting core with the existing raw-weather ambient/sunlight uniforms and the existing recovered light-direction bridge. Existing vertex fog, disabled terrain vertex-colour multiplication and LOD residency/refinement remain.

This is a correction of the lighting core, not a claim of complete original LOD shader reproduction: original parent-texture blending, geomorphing and LOD noise modulation are not introduced by this checkpoint. The sampled loose terrain DDS from the connected extraction was zero bytes; it cannot establish the contents of the installed headset BSA. Missing/corrupt assets retain existing diagnostics/fallbacks.

## Distant object transparency and depth

The original combined building LOD uses BSShaderFlags2 LOD Building and a transparent atlas without NiAlphaProperty. The inspected 2048x1024 DXT3 atlas has 744,667 fully transparent texels. A representative radio-tower LOD is likewise a billboard with transparent DDS pixels. The old generic alpha-property-only admission drew these as solid dark quads.

Native building LOD now enables cutout testing when its real diffuse texture has transparency and no explicit alpha property exists. Explicit original alpha properties, opaque textures, missing textures, ordinary meshes and landscape LOD retain their distinct behavior. The standalone fallback uses the material's existing 0.5 cutoff; the exact original LOD-pass runtime alpha-reference value has not been captured. No new global texture-alpha heuristic applies to detailed objects. Native cutouts use the same guarded GLSL/Vulkan selection; no-discard specialization cannot remove an enabled cutout.

Native landscape/building pass materials also explicitly retain depth testing/writes instead of interpreting their absent generic property bits as a depthless spacewarp overlay.

## Super-Duper Mart

SuperDuperMart01.NIF block 8 is an 86-triangle, NiMaterialProperty-only shape with no surface shader/texture. It spans the lower building section. Generic upload gave it the brown fallback (190,170,135), disabled depth test/write from absent shader flags, and drew it through walls. Placed FO3 world decoding first resolves inherited properties (preserving local overrides), then omits material-only shapes without a surface property; real shader/texturing/vertex-colour properties remain admitted, including missing-DDS diagnostics. Standalone/UI decoding remains available. This is a material rule, with no quest, reference, location or model-name exception. Original collision decoding is separate and unchanged. The original-data test preserves all ten textured Mart surfaces while omitting the extra shape.

## Walkway support

A moving trajectory over original ScrapGroundPlatesReg01 lost support at a facet boundary. The same scene/controller/gravity test across two original scrap plates and seven ramp meshes had one failure among 231 trajectories before the fix and none after it.

Support sweeps now filter unwalkable contact normals during Bullet hit selection, so a closer rail/edge does not mask the floor. Other capsule sweeps retain unfiltered behavior by default. If a grounded character's short downward cast misses, a bounded downward probe starts above the existing step envelope and searches to the original contact target. It does not move the player upward for the probe or snap down to distant floors. Accepted recovery must have upward support, fit the step envelope and retain overhead/head clearance. Airborne characters use normal continuous gravity/landing and do not receive this step recovery. Original collision meshes/layers/placement transforms and step heights remain unchanged.

## Verification and headset checks

Host physics: all four tests pass, including filtered support queries, ledge descent/landing, interior step height/ceiling cases and ragdoll/grab regressions. NIF: 19 tests pass, six optional original-asset tests skip; the Mart/native block test and all 231 original walkway trajectories pass separately. Local original-shader compilation and real Vulkan multiview software-GPU validation pass, including the new day/night world-normal and cutout tests in both generic and specialized pipelines. Android compilation is reported from the matching Actions run. Physical headset verification is separate; no device is attached.

Optional local originals:

- `world_material_tests <SuperDuperMart01.NIF> <native-object-block.NIF>`
- `collision_tests --walkways <extracted architecture/megaton directory>`

Headset: compare near/far hills in one stationary view and again at a different time of day; inspect mast openings against the sky; walk around the Mart and view it through surrounding objects; traverse the Megaton scrap plates/ramp joins in both directions, stop on joins, crouch, leave/re-enter and check a real ledge still falls. The exact reported Mart draw and all live walkway placements still require this physical check.

The broader placed-scene fixture subsequently exposed remaining walkway cases; v306 refines the support query. See placed-walkway-support-v306.md for the final solver and expanded verification.
