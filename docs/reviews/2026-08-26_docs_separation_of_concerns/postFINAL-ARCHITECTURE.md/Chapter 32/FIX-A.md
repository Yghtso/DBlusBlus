## 1. Initial HEAD/status

```text
HEAD:
    ae9a635d71d652a7b8bcf99913c5e7ad55904d0a

Commit:
    ae9a635 chapter 32 ARCHITECTURE analysis

Working tree:
    clean

Index:
    clean
```

## 2. Final HEAD/status

```text
HEAD:
    ae9a635d71d652a7b8bcf99913c5e7ad55904d0a

Working tree:
    M docs/ARCHITECTURE.md

Index:
    clean
```

## 3. Files modified

Only:

```text
docs/ARCHITECTURE.md
```

## 4. Exact Chapter-32 sections changed

- §32.1 Worker model
- §32.2 Morsels
- §32.4 Parallel sequential scan
- §32.5 Parallel hash join
- §32.6 Parallel hash aggregate
- §32.8 Task scheduler and dependencies
- §32.9 Fairness
- §32.10 NUMA policy
- §32.11 SIMD and hot-loop policy
- §32.12 Prefetch
- §32.13 Parallel-runtime invariants

Chapter 33 begins at line 24121 and was not modified.

## 5. N32-1 repair summary

Added a source-independent morsel contract requiring:

- complete source-domain coverage;
- pairwise-disjoint morsel ownership;
- one exclusive claim owner;
- exactly-once required source-occurrence acceptance;
- explicit claim/completion/failure states;
- correct splitting/coalescing;
- no false success after cancellation or failure;
- no generic unsafe morsel replay.

## 6. Final source-domain definition

The source domain is the demanded work for a specific:

```text
source/operator instance
execution attempt
required algorithmic pass
```

Canonical demand, pruning, and early-stop rules may exclude work that is legally unnecessary.

## 7. Complete/disjoint partition rule

When complete consumption is required:

```text
union of morsel domains = required source domain
distinct morsel domains do not overlap
no required unit is omitted
```

This governs ownership, not SQL row order.

## 8. Exclusive claim/linearization rule

A morsel has conceptual states:

```text
available/unclaimed
exclusively claimed
successfully completed
canceled/failed
```

Concurrent workers cannot both successfully claim one morsel. A morsel cannot complete successfully twice.

No queue, mutex, counter, compare-exchange, or work-stealing structure is prescribed.

## 9. Exactly-once source-occurrence rule

Within the defined source instance, attempt, and pass, every required source occurrence is accepted exactly once.

This does not mean every input produces one output row.

Filters, joins, aggregates, and LEFT JOIN retain their existing output multiplicity.

## 10. Source-instance/attempt/pass scope

Legitimate separate algorithmic passes may reread physical input. Each pass has its own source domain and ownership accounting.

## 11. Splitting/coalescing behavior

Splitting preserves the complete parent domain as disjoint child domains.

Coalescing covers every child domain exactly once.

Claimed or accepted work cannot be silently reassigned in a way that duplicates occurrences.

## 12. Empty-source and empty-morsel behavior

- Empty source: empty morsel partition may complete successfully.
- Zero-length morsel: owns no occurrence and cannot create duplicates.
- Nonempty morsel yielding zero output: valid after visibility/filtering.
- Coverage is determined from domains and claim/completion state, not output cardinality.

## 13. Cancellation handling

Canceled morsels do not count as successful completion.

Chapter 26 and Chapter 39 own cancellation, quiescence, cleanup, and transaction consequences.

## 14. Worker failure handling

Failed morsels cannot produce successful complete execution. Query cleanup and worker quiescence follow existing owners.

No generic mid-attempt replay protocol was introduced.

## 15. Unsafe replay prevention

Failed or abandoned morsels are not automatically requeued.

Reassignment is permitted only where an existing canonical owner provides exact attempt-isolated retry semantics.

No physical undo or automatic post-W DML retry was introduced.

## 16. Successful completion condition

A source or blocking stage requiring complete consumption cannot complete successfully while a required morsel is:

```text
unclaimed
active
failed
canceled
```

Canonical early stop/pruning remains the only exception.

Chapter 26 remains the sole Finalize/readiness owner.

## 17. Heap page-range application

§32.4 now requires page-range morsels to cover the required page domain without gaps or overlap.

Each range must resolve its required source occurrences before successful scan completion.

Separate statements or valid algorithmic passes may revisit the same physical page.

## 18. VALUES range application

VALUES row ranges inherit the general partition rule and preserve distinct equal-valued occurrences.

No occurrence may be duplicated or omitted.

## 19. Spill partition/run application

Spill partition/run ranges inherit the same ownership rule while preserving Chapter 24 and operator-specific repeated-pass behavior.

## 20. Snapshot/RID/read-epoch preservation

Existing Chapter 9, 11, 14, 26, and 27 rules remain authoritative:

- same transaction/snapshot/CommandId;
- read-epoch protection;
- exact RID identity;
- no unsafe physical rebinding;
- cleanup only after worker quiescence.

## 21. Worker-count independence

One worker, multiple workers, morsel-size changes, claim order, and uneven worker speeds cannot change required source membership, multiplicity, visibility, or ordinary error semantics.

Chapter 29’s permitted FLOAT64 reduction-tree variation remains unchanged.

## 22. SQL bag and join-multiplicity preservation

The repair explicitly separates source ownership from output cardinality.

Chapter 20 and Chapters 27–30 remain authoritative for:

- bags;
- ordering;
- filtering;
- joins;
- LEFT JOIN multiplicity;
- aggregation;
- sort output.

## 23. Aggregate/FLOAT64 preservation

Exact integer/count state, MIN/MAX, AVG counts, flags, and canonical representations remain unchanged.

Legal FLOAT64 binary64 reduction-tree variation remains permitted.

## 24. Sort/repeated-pass preservation

Sort runs and merge passes retain their existing Chapter 30 ownership.

Separate valid reads are not incorrectly prohibited, and required run/partition work cannot be omitted or duplicated.

## 25. DML single-writer preservation

DML evaluation and scan work may use permitted parallelism.

Persistent DML mutation/write publication remains single-worker under Chapter 31.

Ordinary errors still close before W, and W/C/R semantics are unchanged.

## 26. DDL regression

DDL coordinator ownership remains unchanged.

## 27. VACUUM regression

VACUUM maintenance ownership, same-table serialization, and permitted different-table concurrency remain unchanged.

## 28. ANALYZE regression

ANALYZE retains one publication coordinator under Chapters 31 and 34.

Collection helpers do not create parallel statistics publication.

## 29. §32.2 changes

Added:

- source-domain definition;
- complete/disjoint partition rule;
- exactly-once occurrence acceptance;
- linearized claim ownership;
- split/coalesce rules;
- empty-source/morsel behavior;
- cancellation/failure and replay restrictions;
- timeless morsel-size wording.

## 30. §32.4 changes

Parallel sequential scan now explicitly delegates to §32.2’s:

- complete coverage;
- non-overlap;
- exclusive claim;
- occurrence acceptance;
- successful-completion requirements.

## 31. §32.13 changes

Invariant 2 now requires:

- complete/disjoint required source coverage;
- one claim owner;
- exactly-once occurrence acceptance;
- successful completion only after complete coverage or canonical early stop;
- cancellation/failure never counting as successful coverage.

## 32. N32-2 phrase-by-phrase repair table

| Old wording | Final wording |
|---|---|
| “first production executor” | Worker/task model supports one- and multi-worker execution |
| “initial heap-scan target” | Configurable heap-scan morsel target |
| “preferred build architecture” | A parallel hash-join build may use the structure |
| “preferred first parallel design” | A parallel hash aggregate may use the structure |
| “first parallel scheduler” | A conforming parallel scheduler may use the structure |
| “future optimization” | Compatible optional optimization |
| “future multi-query scheduler” | A multi-query scheduler may cap resources |
| “deferred until measured” | Optional NUMA policies |
| “future NUMA policy” | A NUMA policy can be inserted |
| “profiling-driven later work” | Optional architecture-specific optimizations |
| “initial execution relies” | BufferPool/OS/cache behavior is a valid default |
| “added only when profiles show benefit” | Explicit prefetch is optional when profiling supports it |

## 33. Final optionality/baseline-scope assessment

The repaired chapter remains:

- timeless;
- implementation-independent;
- precise about semantic obligations;
- free of mandated scheduler primitives;
- explicit that NUMA, SIMD, prefetch, work stealing, and multi-query controls are optional.

## 34. Mandatory thought-experiment matrix

| Case | Required outcome | Resolved |
|---|---|---|
| A. Two workers claim one morsel | Only one claim succeeds | Yes |
| B. Overlapping page ranges | Invalid partition; no duplicate ownership | Yes |
| C. Required page range absent | No successful complete scan | Yes |
| D. VALUES occurrence in two morsels | Invalid overlap; no duplicate acceptance | Yes |
| E. Equal VALUES deduplicated | Forbidden; occurrences remain distinct | Yes |
| F. Spill segment omitted | No successful complete pass | Yes |
| G. Empty source | Empty partition may succeed | Yes |
| H. Zero-length morsel | Owns no occurrence | Yes |
| I. Morsel produces no output | Valid if assigned work completed | Yes |
| J. Worker fails before acceptance | Morsel not successful; query failure/cleanup | Yes |
| K. Worker fails after acceptance | No unsafe replay; existing failure owner | Yes |
| L. Cancellation after output chunk | Existing query/result owner applies; no false success | Yes |
| M. Canceled morsel marked complete | Forbidden | Yes |
| N. Finalize with unclaimed work | Successful completion forbidden | Yes |
| O. Successor after partial completion | Dependency readiness forbidden | Yes |
| P. One versus several workers | Same required source coverage/results | Yes |
| Q. Separate algorithmic reread | Permitted under separate pass ownership | Yes |
| R. DML worker attempts mutation | Violates Chapter 31 single-writer ownership | Yes |

## 35. Global contradiction-search results

No contradiction was found with:

- Chapter 26 occurrence acceptance;
- Chapter 20 bags and demand;
- Chapters 11/14 RID and epoch ownership;
- Chapters 27–30 operator-specific partitioning;
- cancellation and quiescence;
- early stop;
- hash join build/probe;
- aggregate Combine/Finalize;
- sort repeated passes;
- DML single-worker mutation.

No earlier chapter required modification.

## 36. Document-role audit

The modified text remains timeless Architecture.

It contains no:

- review chronology;
- commit references;
- implementation status;
- benchmark result;
- concrete queue or atomic primitive;
- new retry mechanism;
- new memory subsystem;
- Verification procedure.

## 37. New semantic question

```text
NONE
```

## 38. `git diff --check`

```text
PASS
```

## 39. Repair-only diff summary

```text
docs/ARCHITECTURE.md | 111 lines changed
96 insertions
15 deletions
```

All changes are within Chapter 32.

## 40. Final Git status

```text
 M docs/ARCHITECTURE.md
```

The index remains clean.

## 41. Confirmations

```text
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
CHAPTER 31 NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
HISTORICAL REVIEW ARTIFACTS UNTOUCHED
CHAPTER 33 REVIEW NOT STARTED
```

```text
N32-1:
    CLOSED

N32-2:
    CLOSED
```

CHAPTER 32 ARCHITECTURE FIX A COMPLETE —
READY FOR FOCUSED READ-ONLY CLOSURE AUDIT