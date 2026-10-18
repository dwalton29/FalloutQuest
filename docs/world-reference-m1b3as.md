# M1B3as — indexed condition-variable dependencies

Continues M1B3ar (`9fefba24a82f4054c5ebda2acb3bb15556ab5740`), whose
Android build passed (run 39083636186).

Entered-program capture now retains conditionVariables and
questConditionVariables: one binding for every function 53 (GetScriptVariable)
or 79 (GetQuestVariable) in the entry's retained condition list or INFO parent's
quest condition list, in authored order. Each value contains the indexed owner,
direct script, numeric slot, quest/actor kind, actor base when applicable, and
normalized decoded variable name. Its ownerName is empty because CTDA operands
are FormIDs and indexes; no editor-name lookup is invented.

This collection describes potential gate dependencies. It does not rerun gates,
read variable values, or assert that an INFO is eligible. INFO result preparation
has always been separate from the dialogue gate evaluator. Both begin/end result
entries carry their own gate dependencies. Stage entries retain their item gates;
the quest-root gates are retained only for INFO parents, as before. Unentered or
skipped stage items are outside this evidence.

The indexed VariableKey resolver accepts an optional bounded sink. Ordinary
condition evaluation and activation consumers omit it, preserving their existing
resolution and value semantics. During provenance preparation, the sink charges
the binding object plus both string terminators and variable-name bytes before
copying/lowercasing the retained name. Conditions collect after existing entry,
compiler/preflight and local-table accounting; the two gate lists share the
remaining 16 MiB retained-value allowance. Missing or oversized metadata rejects
the private candidate without publishing state/effects. Fixed vector objects are
included in sizeof(ScriptExecution); allocator capacity and decoded worker
indexes are separate preparation costs.

Before file access, master matching requires exact coverage against every
retained CTDA 53/79: owner, slot, quest/actor kind and order. Missing, extra,
reordered or cross-list bindings reject. Every supplied indexed binding then
joins the existing original QUST->SCRI->SCPT or ACHR/ACRE->NAME->NPC_/CREA->SCRI
chain proof and numeric declaration/slot validation. No additional master pass
is needed. Original owner records need not have EDIDs for indexed access; named
bindings continue to require the separate live-name namespace proof.

Deleted, duplicate, absent, malformed and wrong-type records, association/type
mismatches, changed slot names/indexes and ref/unclassified/ambiguous declarations
reject. Slot zero and complete repeated raw slot pairs remain legal. The old
verified report survives all failures. Exact allocation/file-free report
comparison includes both new vectors and every binding field.

Coverage is complete for these two functions within the retained lists, not for
all condition operands or all gate evaluation. Runtime reads/outcomes, activation
variable overlays, condition/global values, implicit receivers, failed source
lookups, source lookup completeness, call/event causality and full catalog
equivalence remain open. No authored dependency exemption or resident gameplay
caller is added. Version 212 remains unchanged; complete ownership, bounded
dispatch and device validation still gate general resident Enable/Disable.

Validation results are recorded in SCRIPTING_COMPLETION_STATUS.md.
