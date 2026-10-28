# v261: interior cave spawn and live Megaton gate repair

The v259 source-script and NIF tests passed, but the live Wasteland selector still
contained an older unconditional suppression of REFRs 0001D55A (the scripted
gatehouse), 00003B24 (the entrance door), and 0006D4AD (the alternative entrance
reference). That removed the gatehouse before CPU geometry/animation staging,
so the reference could not appear in the resident GameMode/PlayGroup registry.
The suppression is removed. Original enable-parent state still decides which
variants exist. The old model-name-dependent 6.5-metre entrance aiming padding
is also removed; normal door bounds and interaction occlusion apply.

Startup now uses the opposite side of the original cave-door link: exterior
REFR 00018612's XTEL arrives at interior REFR 00024720 in Vault101a (CELL 00024512,
no exterior WRLD), at authored position (10800.414, 7531.120, 2818.680). Position
and facing are read from the installed master, not copied as runtime coordinates.
v260 selected the interior door's outgoing link and therefore spawned outside.

To allow leaving the cave in the current development save, the original script's
`Activate` fallback now queues a transactional default door action. The script,
not an object-ID exception, authorizes the delegation. Ordinary scripted-door
permissions still block automatic default actions. Delegation retains enable,
lock and key checks; delivery rechecks them and uses the original XTEL transition
or existing local swing-door controller, without recursively invoking OnActivate.
The current original CG04 quest is not start-game-enabled. When it is not running,
Vault101ExitScript selects its normal Activate fallback. No quest stage is forced.

Two known but unimplemented commands, `player.scaonactor` and `ForceSave`, are
recognized as unavailable instructions: an unentered branch does not execute
them, while an entered branch rejects the entire transaction. Unknown commands
still fail compilation; no command is silently treated as successful. Full active
character-generation menus/quest flow and Activate event re-entry remain unsupported.

Validation now starts with the actual world placement selection before invoking
the original Megaton trigger and resident GameMode scheduler. It asserts the gate
and entrance survive selection, Forward remains active through its source duration,
Left follows at completion and OnLoad restores the idle. Original ESM tests also
execute the real cave OnActivate fallback. Synthetic tests verify candidate-only
queued activation, retained scripted-door blocking, lock checks, rejection of an
executed unavailable operation, and restoration of previously suppressed entrance
references through the ordinary selector. An extracted production startup resolver
was checked against the supplied ESM for the interior CELL, exact arrival and
cancellation. Original source assets remain external. Headset verification is
still needed for the complete physical playthrough.
