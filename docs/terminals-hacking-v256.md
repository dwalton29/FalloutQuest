# v256: world terminal screens and hacking

Terminals are activated with right A. Right stick moves the hacking cursor or
menu selection; A selects and B backs out. Horizontal stick scrolls long menu
pages; vertical stick scrolls result text. The interface is rendered on the
actual CRT mesh, depth-tested in the world, without relocating the player or
spawning a floating screen. Left-stick walking and physical head movement remain available, so the player
can approach the CRT without resetting the puzzle. Right-stick snap turning
is suppressed while that stick controls the terminal.

## Original sources inspected

- Fallout3.esm: 378 TERM definitions, 1,339 menu items, 357 compiled result
  bodies, DNAM difficulties/flags, PNAM password notes, CTDA conditions,
  INAM notes, TNAM submenu links, and placed REFR XLKR associations.
- Original hacking_menu.xml, computers_menu.xml, Menus/FalloutDict.txt and
  FALLOUT.INI font 5 (Fixedsys_Comp_uniform_width). The dictionary and font
  are loaded from the user's installation/Misc and Textures archives.
- All ten model paths used by model-bearing TERM records, including desk,
  ruined, wall, mainframe, Vault control, Overseer and Planetarium consoles.
  Largest connected screen-material surfaces exclude keyboard lamps/labels;
  glyph corners are mapped barycentrically onto the original curved geometry.
- Fallout3.exe 1.7.0.4: 005B3E50 word length (4 + 2*tier + base FormID parity), 005B3EF0 difficulty thresholds
  and leveled adjustment; 00925781..0092586A Science-based word count;
  00928A50 dictionary loading. Defaults iHackingMinWords=5,
  iHackingMaxWords=20, fHackLevelMult=.25 and original hacking strings.
- Cross-checked TERM layout with
  [xEdit's FO3 definitions](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsFO3.pas).
  The supplied master also contains three-byte DNAM variants; both three and
  four bytes are decoded.

No proprietary assets are checked into this repository or embedded in APKs.

## Implemented state and gameplay

Hacking uses the existing Science skill, not a separate invented skill. Gates
are 0/25/50/75/100. Real password notes bypass hacking, including Requires Key
terminals. Difficulty and candidate counts use original data/defaults.

The minigame supports four guesses, positional likeness, selectable whole words,
repeated guesses costing another attempt, matched punctuation bonuses, dud removal that preserves the password, one
allowance reset, warning on the final attempt, cancellation and persistent
lockout. Computer Whiz is selectable and permits one additional failed hack.
Success unlocks the real reference and awards the original tier's perk-modified
XP once. Authored ownership feeds the shared crime/karma feedback boundary.

Menus retain original text, result scripts, conditions, notes and submenus.
Result text scrolls; original image notes and audio notes are displayed/played
on the terminal. Input is latched across entry/exit so held A/B cannot select
the next page or operate a weapon. Loading, focus loss, disabled references,
cell changes and leaving the terminal's area close the interface.

Terminal placements were previously removed by Pip-Boy quest-target pruning.
They are retained with their XLKR links. Linked-reference CTDA run-on contexts
and preamble `ref`/`GetLinkedRef` locals now resolve from the original placement.
Scripted non-actor numeric locals are indexed separately from the NPC census.

Result programs prepare and commit through the shared detached transaction
system. Terminal program identity, menu ownership/reachability, original
conditions and linked references are captured and checked against the original
master before resident Enable/Disable exchanges. Unsupported commands fail the
whole action and display their diagnostic; they do not partially execute.

Additional script operations include AddNote/RemoveNote, ShowMap,
ForceTerminalBack, optional Enable/Disable fade flags and door SetOpenState.
Door targets use original NIF animations and the existing collision update path.
Sparse open targets persist in the optional v20 save extension; v1-v19 remain
readable and unchanged saves retain their earlier encoding.

## VR adaptations and remaining engine limits

The 60x28 display grid, two-dimensional stick navigation/repeat, procedural
memory placement, screen insets and 1-2.5 mm surface offsets are implementation
choices for Quest. This is a native reconstruction of the original menus, not
execution of Bethesda's XML renderer or bit-identical random puzzle generation.

Specialized menu scripts still depend on engine systems outside this terminal
implementation. Examples include creature/turret DamageAV/SetUnconscious,
SetFaction/SetEnemy, Activate chains, PlaceAtMe and special scripted activator
animations. These produce an explicit unsupported-action diagnostic. Displaying
all original menu content does not imply that every quest/creature command has
been implemented. Ordinary terminal reading, hacking, locks/doors, notes,
map reveals and supported quest/global/local scripts are connected to gameplay.

## Validation

Host tests cover skill thresholds, word lengths/counts, likeness, repeated
selections, four-failure lockout, punctuation consumption, password preservation,
input routing/release latches, password bypass, Computer Whiz, XP once, linked
unlock transactions and save/reload. Optional original-asset checks enumerate
all TERM/menu definitions and match a captured original Tenpenny terminal action
and XLKR back to the master. Original mesh tests check every model family and
surface coordinate orientation. Both native Vulkan and GLES renderer headers
are syntax-checked. Full Android and Vulkan CI remain the release gate.

Headset readability, exact perceived CRT offset, controller comfort and audio
mix still require device testing.
