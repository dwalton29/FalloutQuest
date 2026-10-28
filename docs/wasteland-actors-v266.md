# Authored Wasteland actors — v266

The original Wasteland contains 2,316 ACHR/ACRE placements. The renderer previously rejected every exterior world except Megaton, discarded all parent-controlled actor references, and could not assemble creatures or resolve raider LVLN templates.

This change tracks original actor identities independently of visibility. Interiors use their CELL; original small worlds remain fully resident; large worlds use a three-by-three grid around the tracked player. CPU preparation and NAVM/furniture scope follow that window. Arrivals retain the existing worker, epoch validation, image staging bounds and one-part-per-frame GPU upload. Leaving the window retires GPU resources while retaining canonical actor state. Original Enable/Disable/XESP remains authoritative; tracking an initially disabled actor does not activate it. Unsupported/missing assets receive a retry backoff.

## Source examples

| Reference | Source behavior now supported |
| --- | --- |
| `0001FB3A`, Deputy Weld | ACRE/CREA, Protectron skeleton and NIFZ model parts, template categories, voice, GameMode speech, conversation INFO selection/results, Say Once recording, and topic-filtered SayToDone. The original INFO result sets the flag watched by the gate script. |
| `000C6E91`, Micky | Original NPC/race/outfit and dialogue, persistent Variable01–Variable10 actor values, original OnLoad/GetDead/GetAV/Kill path. |
| Super-Duper Mart raiders | Nested LVLN selection, source level eligibility/flags/chance, stable per-reference choices, shared appearance/stats/inventory/AI/voice, and the original intercom trigger's linked-reference enable-parent wave. |

Selections freeze at first enabled residency, using the current player level and a stable reference/list seed. They are stored in an optional FQPS v21 extension alongside sparse actor variables; older supported saves remain readable. This is a deterministic standalone adaptation of random selection, not the PC engine's random stream or encounter-zone reset service. Count/multi-spawn list modes outside the supported single-actor path fail closed.

Source schemas were checked against the supplied Fallout3.esm and xEdit FO3 definitions. `tools/npc/audit_original_actors.py /path/to/Fallout3.esm` reproduces reference/template metadata. No Bethesda assets or extracted scripts are included in the repository.

## Verification and remaining scope

Host regression coverage includes physical residency bounds, original Weld INFO results and gate flag, Micky's saved variables and OnLoad death, the original mart trigger/parent chain, original creature/raider record assembly, and bounded NAVM reads. The normal weapon, dialogue, script transaction, package, combat, census and save tests remain part of CI.

The full original meshes/voice archives and a connected Quest are not available in this workspace, so original creature skinning, animation paths and audio playback still require device verification. Missing voice audio does not falsely execute successful speech end results. This is not complete vanilla AI/script parity: creature attacks, general combat-event choreography, encounter-zone respawn/reset, multi-spawn lists, and unsupported source commands remain separate work. Story-disabled actors are never globally forced on.
