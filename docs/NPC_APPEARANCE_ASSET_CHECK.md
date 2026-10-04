NPC appearance follow-up (0.32.2)

Verified against the user's original Fallout 3 meshes/textures and Fallout3.esm;
no Bethesda asset bytes are included in this repository.

- Lucas Simms's worn hat is Armor/HeadGear/LucasSimmsHat/M/LucasSimmsHat.NIF.
  Its ARMO BMDT mask is 0x600 (headband and hat), not the head/hair mask 0x3.
  Recognise head accessories for rigid attachment and FaceGen fitting.
- The worn hat and hair geometry are already oriented in actor axes, around a
  head-relative origin. Translate hair/headwear to the Head bind origin without
  reapplying that bone's bind rotation; animation still uses the full Head pose
  delta. RACE slots 2..7 (mouth, teeth, tongue, eyes) instead use the full Head
  bind transform: their original root rotation expresses bone-local coordinates.
  Confirmed with EyeLeftHuman/EyeRightHuman and TeethUpper/LowerHuman originals.
  In the originals, the hat spans approximately Z 8.48..19.49 before attachment;
  HeadHuman's Head origin is Z 112.84.
- HairBase.NIF contains authored Hat and NoHat shapes. Render only the variant
  selected by the hat slot and use HairBasehat.egm / HairBasenohat.egm respectively.
  The previous generic HairBase.egm lookup did not find either original.
- Preserve the geometry transform's linear chain for EGM deltas, including
  authored scale and ancestor rotations; translations do not affect deltas.
- HeadHuman.egt colour features are vertically reversed relative to the supplied
  HeadHuman.dds. Reverse the EGT row during sampling, preserving DDS alpha and
  bilinear resampling. The previous composite put mouth colour across the eyes
  and eye colour across the mouth. FaceGen's format defines planar signed RGB
  mode images: https://facegen.com/dl/sdk/doc/manual/fileformats.html

Host regressions cover attachment under actor rotation/scale, the 0x600 hat
mask, Hat/NoHat selection and morph filenames, scaled ancestor morph vectors,
and EGT row/channel order with alpha preservation. On-headset visual validation
is still required for exact likeness and animated appearance.
