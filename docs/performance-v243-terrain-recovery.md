# v243 — restore original LAND rendering

The v242 startup capture from the headset reports `ready=1 fusion=1` on
Adreno 740, successful texture/geometry uploads, and submitted terrain batches.
Both recorded launches nevertheless have missing ground. The cold admission
and four streamed cells all report zero folded groups: the problem occurs with
the fused program even when no fused geometry is admitted. This rules out the
previous compile-error explanation for this capture; it does not identify the
remaining GPU rendering defect.

Restore the retained original v237 LAND shader and disable CPU layer fusion
for both cold and streamed terrain. Apply the same policy to all build types
and ignore the old `terrain_legacy` property, including `0`. The new startup
message identifies recovery explicitly. This is a recovery/quarantine of the
fused path, not a claim that its underlying device defect has been repaired.

Keep indexing, covered-paint removal, streaming, physics, world distance,
water, fog and other render settings unchanged. No save or asset reset is
needed. Install v243 over the existing app and launch normally.

Validation: compile the actual program factory with host, Android debug and
Android NDEBUG policy; verify exact original shader sources for absent, zero,
one and malformed previous properties; exercise compile/link failures and
subsequent creation. Run the retained GLES shader comparisons, geometry,
terrain upload and render-dispatch regressions. CI builds Debug and O2 APKs.
Headset confirmation of restored terrain remains necessary.
