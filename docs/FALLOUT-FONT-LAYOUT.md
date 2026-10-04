# Fallout 3 bitmap font layout, version 149

## Audit of canonical main

Started from `9572d69bdca6911c51ac5cba1e3184c98da15a11` on main.
`fo3-runtime-loop.inc` calls `RenderFo3InteractionHud`, previously the layout
header's tallest-glyph baseline renderer. It included the base renderer but did
not call its alternate entry point. Loot called `fo3huddynamic::EnsureResources`
and its own glyph loop, so the dynamic header was live for resources and loot,
not its prompt Render entry point. The assets header's static Open/Door renderer
had no live caller. The runtime's procedural OPEN/DOOR debug geometry is behind
`if (false)` and does not use FNT. No other live bitmap glyph consumer was found.

The parser's record sizes were correct, but it called offsets 44/48/52
xOffset/yOffset/advance. Consequently horizontal spacing affected Y and vertical
top bearing affected pen X. The prompt override inferred a baseline from the
current string's tallest glyph and treated spacing as vertical bearing. Loot
still top-anchored glyphs using spacing and measured width using topEdge.

There is now one CPU parser/layout header, one prompt renderer/resource state,
and the loot renderer calls that same layout with its existing world transform.
Legacy layout/dynamic headers delegate; the unused static renderer was removed.
No prompt wording, font/atlas choice, HUD colour, button, padding, VR mounting,
scale, opacity, blend functions, interaction logic or world renderer was changed.
Geometry rebuild conditions remain prompt equality and loot-panel identity.
Shutdown now releases the shared live resources instead of the previous separate
loot font instance. Loading/world/actor rendering remains outside this change.

## Evidence from the supplied original executable

Primary evidence is the supplied Fallout3(2).exe, disassembled as 32-bit x86 at
its PE virtual addresses. Function names below describe their recovered roles;
they are not exported symbols. No game bytes are committed.

| Location | Verified operation |
|---|---|
| B57F28 | Allocate/read 0x3928 bytes for font data |
| B57F73..B57FCD | 256 records, stride 0x38; first topEdge at data+0x15C |
| B58130..B58176 | Texture count at +4, maximum 8, filename at +12 |
| B58CB2..B58CC9 | Glyph table data+0x128+unsigned-byte*0x38 |
| B555FD..B55616 | Add glyph+0x2C (leadingEdge) to pen X |
| B55635..B556EF | Quad Z=lineZ+glyph+0x34; second edge subtracts +0x28 |
| B556F3..B55751 | UV corners read +4,+20,+12,+28 for TL,BL,TR,BR |
| B55792..B557AC | Add width + spacing only when width>0 to the pen |
| B56529..B56535 | Measurement sums leadingEdge + width + spacing |
| B58C87..B58C9F, B56500..B56518 | CP1252 91/92 map to apostrophe, 93/94 to double quote |

Disk layout is baseline float at 0, texture count int at 4, eight 36-byte texture
entries starting at 8 (filename begins four bytes into an entry), and 256 glyphs
at 0x128. Glyph texture index at 0; four UV pairs at 4..35; width at 36, height at
40, leadingEdge at 44, spacing at 48, topEdge at 52. The portable logical Glyph
is statically checked to be 0x38 bytes. Parsing rejects truncated/extra files,
non-finite metrics, invalid counts and unterminated texture filenames.

### Loader substitutions, including whitespace

B57F90 computes the whole font's maximum bitmap height and minimum(topEdge-height),
not the current string's tallest glyph. It also computes the maximum
baseline-topEdge+height, confirming the baseline-relative bottom extent.
B57FCF..B5800C swaps space's width and spacing, then sets its height to maximum
height and topEdge to maxHeight+min(topEdge-height). B5800C..B58042 copies those
four metrics to NBSP (A0), leaving its own UVs/leading edge intact. B58048..B5808D
copies the five scalar metrics of pipe (7C) to DEL (7F), leaving UVs intact.
B58093..B58126 gives NUL zero width/spacing/UVs and the same height/top extent.
These verified engine substitutions run once after the disk parse. There is no
magic 13-pixel rule or Quest-invented character correction.

### Horizontal and vertical rules

After those substitutions:

```
renderAdvance = leadingEdge + width + (width > 0 ? spacing : 0)
left          = penX + leadingEdge
right         = left + width
```

Original FontManager measurement unconditionally adds all three fields. That
can differ from AddChar for zero-width records with nonzero spacing. Current
single-line UI fitting/centering deliberately measures the emitted pen advance,
so measured widths and geometry agree. Both rules are named in the shared header;
none of the current text requires original wrapping, tab stops or inline controls.

AddChar's native upward quad is Z=lineZ+topEdge and Z-height. Expressed in downward
UI pixels with `lineTop` and the font's stored baseline:

```
top           = lineTop + baseLine - topEdge
bottom        = top + height
```

The baseline is independent of the string. Spacing never affects Y. Ascenders,
descenders and punctuation retain their authored topEdge/height distinctions.
UVs retain their original corner order and row convention. A zero-area UV
rectangle emits no glyph quad, while its authored advance still applies; this
avoids sampling an atlas border texel for an invisible glyph. Invalid texture
indices also emit no quad. The existing single-atlas UI explicitly rejects fonts
with multiple atlases rather than silently sampling the wrong texture.

## Super-Duper Mart trace

The supplied original master was scanned for CELL/DOOR FULL. CELL 00017F37 has:

```
Super-Duper Mart
53 75 70 65 72 2D 44 75 70 65 72 20 4D 61 72 74
```

Door prompt composition inserts ASCII spaces and copies authored FULL names;
GetFo3DoorPrompt copies bytes with memcpy. QueryFo3Interaction and the host's
fixed doorPrompt buffer preserve them. Neither the original nor corrected renderer
remaps ASCII 20/2D/2E: their indices are respectively 32/45/46. No source-code
substitution from space to period was found. The confirmed bugs are metric misuse
and inconsistent layout; the precise cause of a reported dot-like mark on the
headset is **not conclusively observed** without that original FNT/device capture.
The corrected layout does not patch the string or change prompt wording.

Debug-only, opt-in, capped diagnostics trace ESM FULL, interaction target, host
buffer and emitted glyph selection. They only run on source/geometry changes,
not each eye/frame, and report bytes, index, advance and quad/no-quad separately.
The font loader reports structural sizes and representative metrics once.

```sh
adb shell setprop debug.falloutquest.font 1
adb shell am force-stop com.falloutquest.app
# Launch the app, then aim at the door and open a loot panel.
adb logcat -v time FalloutQuest:I '*:S' | grep -E 'FNT (METRICS|GLYPH)|HUD TEXT'
adb shell setprop debug.falloutquest.font 0
```

Restart after enabling to capture ESM cache construction. Android property tracing
is absent in release builds. Diagnostics are limited to 96 byte-trace records per
process; glyph details accompany geometry records only.

## Tests and missing original input

Synthetic parser tests generate their own records, with deliberately different
leading/spacing/top metrics. They cover all requested diagnostic strings, negative
spacing, whitespace/NBSP, smart quotes, punctuation indices, authored descender
positions, pen/measurement agreement, fitting, malformed sizes/counts/filenames,
NaNs and baseline independence. Integration checks require prompt and loot to use
the canonical helpers/resources, guards, caches and bounded diagnostics.

Optional original validation prints raw metrics for space, period, hyphen, 0,1,
A,M,a,e,g,t,y and checks structural and drawable/whitespace relationships:

```sh
cmake -S tests/assets -B build/host-assets
cmake --build build/host-assets
ctest --test-dir build/host-assets --output-on-failure
build/host-assets/font_layout_tests /path/to/Data/textures/fonts/baked-in_monofonto_large.fnt
python3 tests/render/test_font_consumers.py
```

The supplied Misc BSA and both Megaton ZIPs were inspected; they do **not** contain
this FNT. Exact missing input: the user's
`Data/textures/fonts/baked-in_monofonto_large.fnt`, or `Data/Fallout - Textures.bsa`
containing it. A matching `.tex` atlas is additionally needed for a visual font
preview. No third-party font was substituted. The executable's layout/arithmetic
was verified, but original representative numeric glyph metrics, atlas appearance
and headset punctuation/alignment cannot be claimed verified in this session.
Menus/prefabs/text_box.xml is available in Misc BSA; its presentation remains as
already recovered in the project. Original multiline wrapping/tab/control-code
behaviour is outside the current single-line UI scope.
