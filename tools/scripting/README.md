# Fallout 3 script requirements inventory

[Summary](../../docs/SCRIPTING_SCDA_INVENTORY.json) · [Per-fragment index (gzip JSON Lines)](../../docs/SCRIPTING_FRAGMENT_REQUIREMENTS.jsonl.gz)

The index covers each nonempty SCDA fragment in the supplied base master. A row ID identifies the record type, FormID and authored slot. For example, `QUST:00014E9E:10/0` means stage 10, item 0. For INFO the slot is begin/end, for TERM it is the menu entry, and for PACK it is the begin/end/change slot (0/1/2). Counts in the summary are distinct fragments. One fragment can contribute to several gaps.

### Look up affected fragments

From the repository root, this lists every fragment for which the host compiler diagnosed an unsupported form of OpenTeammateContainer.

```sh
gzip -dc docs/SCRIPTING_FRAGMENT_REQUIREMENTS.jsonl.gz |
  jq -r 'select(.observed_gaps | index("unsupported command form/OpenTeammateContainer")) | .id'
```

To find all uses, including ones for which the probe has no diagnostic, query the dependency fields. For example, use `select(.expression_calls | index("GetDead"))` to find nested calls to GetDead. The data can also be read with Python's standard `gzip.open(path, "rt")` and `json.loads(line)`.

The host tests also exercise the original Taft Tunnel follower package and a compiled Jericho dialogue result. They check teammate membership, combat-style precedence and weapon selection. Cell transitions and in-combat companion activation require separate gameplay coverage.

The linked-reference tests run the original planetarium ambush and Jefferson Memorial controller scripts. They check the enabled actor, the two successive reference lookups, and transaction rollback.

The ownership tests exercise an original reward dialogue, a faction-owned cabinet, a quest-stage cell transfer, an owner-specific clue and a trigger that releases 33 placed items. They also check direct versus inherited ownership, container theft, transaction rollback and save migration.

The barter tests run original stock-switching dialogue and check purchases, saved links, stale quotes and restocking after the player leaves and returns.

### Rebuild the inventory

```sh
cmake -S tests/player -B build/script-inventory -DCMAKE_BUILD_TYPE=Release
cmake --build build/script-inventory --target script_fragment_probe -j 3
python3 tools/scripting/inventory_scda.py /path/to/Fallout3.esm docs/SCRIPTING_NATIVE_COMMANDS.json \
  --probe build/script-inventory/script_fragment_probe \
  --fragments docs/SCRIPTING_FRAGMENT_REQUIREMENTS.jsonl.gz \
  --output docs/SCRIPTING_SCDA_INVENTORY.json
python3 tests/player/test_scripting_audit.py
```

Omitting both `--probe` and `--fragments` runs the original envelope-only audit. The extended report records SHA-256 hashes of the master and the generated index. It contains metadata and diagnostics. The game files are read locally.

### Reading a row

- **Bytecode requirements.** `commands` includes control instructions. `expression_calls` identifies nested opcodes from length-delimited expressions. `operators` lists the operators used. Each reference-table entry records its type (SCRO form or SCRV local), target and operand roles. Variable accesses are triples of scope slot (zero means the owner), local index and bytecode type. `event_headers` contains the compiled event types and filters. `declared_events` independently lists named source events. An unrecognised operand leaves a diagnostic on its own fragment.
- **Conditions.** Condition lists retain their original order, flags, parameters, run-on reference and comparison. INFO includes the owning quest's dialogue gates. QUST results carry only the matching stage item's gates. TERM, PACK and PERK keep their entry, package or effect scopes. Each probed condition is evaluated individually so a false predicate or OR group cannot hide a later unsupported function. The single-condition results do not imply that the whole gate passed.
- **Compilation and expressions.** The host uses the production catalog and command compiler. A compiled program can still contain an unavailable instruction in a branch. When a program fails early, the audit also checks source lines for explicit unsupported-command diagnostics. Independent numeric expression probes use the runtime's binding evaluator on the initial sample state. An expression can need a value set earlier in the script. Bound native names are cross-referenced with compiled query opcodes in the summary. Form IDs observed in successful expression binding are retained separately from the original reference table.
- **Execution evidence.** The host prepares disposable production transactions and checks the identity of the entered top-level script. A successful transaction that skipped its target is recorded as skipped. The INFO probe exercises the selected result phase independently of dialogue eligibility. For each event script the index retains the result for each source block. `mixed_event_blocks` means the results differed across that script's blocks. `unprobed` includes contexts for which this tool has no delivery adapter. PERK and the placed-reference fragment still have complete dependency rows.
- **Missing owners.** A block marked `needs_owner` needs a calling instance for a declared local assignment or an implicit command. Its diagnostic stays under `context_required`. The summary counts these fragments under `probe_context_requirements` and keeps their execution unverified.
- **Sampling.** Probes share a fresh baseline, a fixed script RNG seed, and registered original actors. Owners are deterministic samples of authored placements or quest owners. Zero means an owner could not be established. INFO uses its speaker base or an unambiguous mandatory speaker-ID predicate where available. For a result gated only by speaker faction, the probe can sample a living, enabled NPC in that faction. These rows carry `runtime.owner_basis` equal to `sampled faction member`. Some terminals require a parent terminal, and package-change callbacks require a prior package. Pending world changes are never committed. A manually delivered event does not establish that the headset naturally generates it. The summary lists literal event dispatch call sites located in production code. Indirect and dynamic producers may require inspection.
- **Prioritisation.** `handler_diagnostics` groups explicit unsupported command forms, expression operands and condition functions. The counts can overlap. `observed_gaps` also includes argument, binding and context diagnostics. `preview_rejections` are separate because an entire stage can fail in another item or nested result. A successful sample leaves alternate branches, save states, animation and headset presentation unverified.

The operand reader follows the Fallout 3 reference-prefix, argument and numeric-value routines at 0x005400F0, 0x00517950 and 0x00517660. [Instruction reader](https://github.com/momusdigital/f3_decomp/blob/f5bac1b54fc8fde16294c1d6779f27407d0a117b/src/f3staging/f3blk_00540000_o2.cpp#L27540) · [Argument reader](https://github.com/momusdigital/f3_decomp/blob/f5bac1b54fc8fde16294c1d6779f27407d0a117b/src/f3staging/f3blk_00500000_o2.cpp#L33085)
