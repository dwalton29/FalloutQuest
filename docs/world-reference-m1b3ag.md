# M1B3ag — original INFO owning-quest identity

Continues M1B3af (`a6509edfed4e2c5229fe06cfa32e388cf3a989fb`), whose Android
build passed at https://github.com/dwalton29/FalloutQuest/actions/runs/39054754217.

Master-matched INFO evidence now requires its original QSTI to be nonzero and
resolve to exactly one nondeleted QUST. The parent payload must pass strict
framing and decode without quest diagnostics. A missing/wrong-type/deleted or
duplicate identity, malformed payload, missing/short DATA, orphan item or
duplicate stage cannot authenticate this owning quest. This is a conservative
definition check: a diagnostic elsewhere in the parent quest also rejects.

The first scan collects the owning IDs from original decoded INFOs after
program matching. The existing second complete header scan resolves them,
including every duplicate occurrence, independently of INFO/QUST record order.
Only requested quest payloads are decoded under the existing 64 MiB record
bound. Shared quests are deduplicated; distinct quests must all be found before
the verified report is published. No third pass or whole-master quest index is
introduced. A quest also used by stage evidence can be decoded in both passes.

The worker retains its prior value-only inputs. Candidate capture, byte/count
limits, exact report comparison, commit/settlement and independent dependency
admission remain unchanged. The parent is resolved from the original INFO,
not from a caller-supplied owning-quest claim.

Tests cover normal and extended QSTI, either record order, compressed parents,
null/missing/self/wrong-type links, duplicate/deleted/missing quests, strict
payload and decoder diagnostics, shared parents and multiple distinct parents.
Failed matching preserves the previous report. Valid quests with either initial
running flag are accepted: runtime state can start a quest that was not authored
to run at startup. Original MS11 stage/bomb/INFO and the independently discovered
restricted INFO retain their successful metadata admission.

This proves original owning-quest identity and the current decoder's structural
acceptance. It does not check current running/completion state, replay INFO or
quest conditions, authenticate their complete execution/read context, or grant
dispatch authority. It adds targeted quest decoding and associated CPU/memory
cost; existing evidence limits do not bound all decoder allocations. General
resident Enable/Disable still awaits remaining ownership, bounded dispatch and
device validation. There is no Quest timing or version claim.
