# M1B3ad — retained script-evidence byte budget

Continues M1B3ac (`e97ebe7a6962d4ba19ee4d4dbcc5325cfca818cb`).

Captured evidence now has a 16 MiB accounted-value budget in addition to the
existing 4,096-entry bound. Repeated event blocks retain separate copies of the
full SCPT container; the entry limit alone allowed hundreds of megabytes from a
relatively small source script. This is an explicit platform admission policy,
not a limit inferred from original game data.

One shared, allocation-free charge function counts fixed ScriptExecution and
raw-record objects, event/source/tag strings with terminators, compiled/header
bytes, reference FormIDs and raw-record payloads, including the full container.
It checks lengths by subtraction/division before arithmetic and leaves the
running total unchanged on failure. An absent container and an explicit empty
container receive the same charge.

The private candidate charges original values before copying program/container
evidence and publishes its new total only after the entry is retained. The
preparation scope clears this counter alongside the collector and parent; it
is not copied from the live Player, saved, or swapped during commit/rollback.
Ordinary execution without evidence capture does not charge or impose this
byte budget. Master matching applies the same policy before opening the master
or copying a verified report. Failed attempts preserve prior reports.

Tests cover exact fits, one-entry overflow, invalid running totals, every
retained field, container parity, cumulative repeated-container rejection,
oversized metadata, and separate 4,096/4,097 small-fragment count boundaries.
Capture failures preserve live state/effects and uncaptured execution remains
available under its existing policy. Original MS11 provenance remains covered.

The budget counts retained value sizes, not allocator bookkeeping, spare
capacity, the original catalog, parser/decoder allocations, other prepared
resources, or simultaneous plan/worker/report copies. Object-size charges use
the current platform ABI. It does not claim a 16 MiB process-memory ceiling or
Quest timing validation. Remaining context/dependency ownership, bounded
dispatch and device validation still gate general resident Enable/Disable.
