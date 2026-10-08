# FalloutQuest v193 — LVLI-sourced NPC outfit assembly

The NPC scene appearance loader now indexes **LVLI** records from the user's
original \`Fallout3.esm\` and resolves eligible, nested LVLO entries into
source-backed ARMO worn-model/slot records. This fixes the unrepresented
\`CondLeatherArmor\` armour on Moira's guard and \`NPCTownClothes\` outfits on
Common House residents. The existing ARMO BMDT slot suppression, male/female
MODL/MOD3 model selection, NIF mesh upload, FaceGen and skinning paths remain
unchanged.

Resolution is *appearance-only*. The original CNTO LVLI remains in the actor's
source list; no fictional permanent ammo/items or new Bethesda assets enter
inventory or corpse loot. Stable reference/slot/list-derived choices avoid
rerolling outfits when a cell is reopened, even after a process restart.
The original LVLD chance-none and LVLF level selection flags are respected for
the level-1 entries; complex actor-level scaling and LVLG dynamic global
chance-none remain unsupported (fail closed). Recursion and cycles are bounded.
An absent/higher-level entry never invents fallback equipment.

Expected log: \`NPC LVLI OUTFIT actor=... source=... selected=... mask=... model=...\`
followed by \`Q23.6 NPC ARMOR\` and \`Q23.7 NPC ASSEMBLY READY\`.
Original LVLI and ARMO records can be used to verify selected models; player and
NPC inventory equivalence is separate future work.

Host tests cover original 12-byte LVLO layout, nested lists, female and male
model choice, chance-none, unavailable higher-level entries, cyclic references,
and deterministic cell reloading. In-headset clothing correctness is not proven
by a successful build; verify Moira's guard and the Common House settlers.
