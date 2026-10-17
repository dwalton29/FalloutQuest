# M1B3ac — inherited script context and parent evidence

Continues M1B3ab (`c9857bc76aafda9ac6a0711ec35cc4180af71173`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39048620929.

Captured program entries now retain the implicit script reference and the index
of their parent program, with an explicit NoParent sentinel for roots. An event
can call SetStage, which changes the numeric-local owner to the quest while
distance, button and message operations continue using the event's implicit
reference. The previous owner/activator/local-script fields did not preserve
that distinction. The interpreter records the actual inherited reference and
parent in the same private collector as the exact entered program.

A scope restores the parent after every entered program, including failed and
exceptional exits. The preparation scope clears both the collector and parent.
Neither is copied from the live Player or exchanged during commit/rollback.
Capture remains optional and off on ordinary execution paths. Captured plans
are limited to 4,096 entries, matching the existing master evidence limit;
empty matching event blocks cannot grow provenance without consuming commands.

Before reading the master, the worker validates an entry-order tree of depth at
most 32. An event root uses its own owner as its implicit reference. Direct
stage/INFO roots have no inherited reference or activator. Nested entries must
be quest-stage fragments with an earlier, still-active parent, and inherit that
parent's reference and activator. A returned subtree cannot be reopened. These
fields are also included in exact report comparison at commit and settlement.

Matching an original SCPT block now checks its authored Begin filter using the
same predicate as execution: an empty filter or the supported player filter
with the Player activator. Unsupported filters and unsatisfied player filters
reject even when body/container bytes match. Existing original owner/SCRI
authentication and independent resident dependency rejection remain required.

Tests cover actual nested quest/event/dialogue capture, sibling/root restoration,
context tampering, self/forward/returned-subtree links, depth and entry limits,
and supported/unsupported/satisfied/unsatisfied filters. Original combined
MS11 stage/bomb/INFO test evidence rebases parent indexes when concatenating
independently captured plans; production passes one plan's trace unchanged.

This validates captured context structure and the supported block filter. It
does not independently reconstruct every SetStage call, replay conditions,
authenticate an arbitrary activator's runtime identity or INFO eligibility,
establish inherited/dynamic script binding, or prove complete dependency/read
ownership. The complete original body still has its existing admission checks.
General resident Enable/Disable awaits remaining ownership, bounded dispatch
and device validation. There is no Quest performance claim or version change.
