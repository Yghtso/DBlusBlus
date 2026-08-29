# 1. Verdict

**CHAPTER 11 — TARGETED DOCUMENT FIXES RECOMMENDED**

Chapter 11 is technically coherent and semantically complete. It defines deterministic lock identities, strict release ordering, write-conflict handling, uniqueness coordination, deadlock detection, victim handling, and wake-up revalidation without requiring a frozen architecture decision.

Three localized documentation findings prevent a clean verdict:

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 0 |
| MINOR | 3 |
| EDITORIAL | 0 |

The findings concern:

1. DEVELOPMENT/implementation-roadmap leakage in §11.12.
2. Historical “existing/now applied” wording in §11.13.4.
3. A deadlock-summary wording mismatch between “cycle” and the canonical “cyclic component” rule.

# 2. Scope and documents read

Primary scope:

- [Chapter 11, lines 7897–8807](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:7897)
- Chapter 12 begins at line 8808.

Context consulted:

- `AGENTS.md`
- `ARCHITECTURE.md` front matter
- Chapter 3 controlled shutdown
- Chapter 5 tuple metadata, physical RID, UPDATE/DELETE boundaries
- Chapter 7 guard/latch lifetime
- Chapter 8 B+ physical keys, latching, duplicate/visibility boundary
- Chapter 9 transaction states, snapshots, status lookup, terminal publication
- Chapter 10 visibility and no-guessing errors
- Chapters 13–16 as required
- Chapter 21 CREATE INDEX/DDL gates
- Chapter 31 DML target materialization/revalidation
- §39.1 failure semantics
- §41 transaction verification obligations

Other live documents consulted:

- `docs/VERIFICATION.md`
- `docs/DEVELOPMENT.md`
- `docs/PROJECT_STATE.md`

No source audit was performed.

# 3. Actual Chapter-11 heading inventory

There are 39 headings: the chapter heading, 15 direct subsections, and 23 nested subsections.

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 11 | Logical Locking and Write Conflicts | Chapter ownership | ARCHITECTURE-APPROPRIATE |
| 11.1 | LockManager scope | Lock families and read-lock exclusion | ARCHITECTURE-APPROPRIATE |
| 11.2 | Tuple-write lock identity | `(TableId,RID)` identity | ARCHITECTURE-APPROPRIATE |
| 11.3 | Lock/latch separation rule | No transaction wait under physical latch | ARCHITECTURE-APPROPRIATE |
| 11.4 | UPDATE / DELETE write-conflict protocol | Target lock/revalidation algorithm | ARCHITECTURE-APPROPRIATE |
| 11.4.1 | No competing xmax | Uncontended write | ARCHITECTURE-APPROPRIATE |
| 11.4.2 | xmax belongs to self | Same-transaction handling | ARCHITECTURE-APPROPRIATE |
| 11.4.3 | xmax belongs to an aborted transaction | Ineffective aborted write | ARCHITECTURE-APPROPRIATE |
| 11.4.4 | xmax belongs to an in-progress transaction | Wait/recheck behavior | ARCHITECTURE-APPROPRIATE |
| 11.4.5 | xmax belongs to a committed competing updater | Isolation dispatch | ARCHITECTURE-APPROPRIATE |
| 11.5 | READ COMMITTED write conflict | Fresh-snapshot retry/abort boundary | ARCHITECTURE-APPROPRIATE |
| 11.6 | REPEATABLE READ write conflict | First-updater-wins abort | ARCHITECTURE-APPROPRIATE |
| 11.7 | Lost-update prevention | Combined correctness argument | ARCHITECTURE-APPROPRIATE |
| 11.8 | Unique-key lock identity | `(IndexId, encoded key)` identity | ARCHITECTURE-APPROPRIATE |
| 11.9 | Unique-key lock protocol | Ordering, acquisition, lifetime | ARCHITECTURE-APPROPRIATE |
| 11.10 | Unique-check semantics | Current-state ownership predicate | ARCHITECTURE-APPROPRIATE |
| 11.10.1 | Three distinct decisions | Visibility/candidate/ownership separation | ARCHITECTURE-APPROPRIATE |
| 11.10.2 | Key domain and NULL rule | UNIQUE/PK NULL semantics | ARCHITECTURE-APPROPRIATE |
| 11.10.3 | Predicate inputs and operation context | Runtime self-exclusion | ARCHITECTURE-APPROPRIATE |
| 11.10.4 | Canonical current-owner algorithm | Candidate classification | ARCHITECTURE-APPROPRIATE |
| 11.10.5 | INSERT/current-state truth table | INSERT and same-command cases | ARCHITECTURE-APPROPRIATE |
| 11.10.6 | UPDATE truth table and immediate behavior | UPDATE/self-exclusion/key swaps | ARCHITECTURE-APPROPRIATE |
| 11.10.7 | Competing transactions, waiting, and recheck | Key-lock serialization | ARCHITECTURE-APPROPRIATE |
| 11.10.8 | Physical candidate integrity, retry, recovery, and vacuum | Physical/current-state integration | ARCHITECTURE-APPROPRIATE |
| 11.10.9 | CREATE UNIQUE INDEX | Offline validation | ARCHITECTURE-APPROPRIATE |
| 11.10.10 | §39.1 and autocommit boundary | Constraint failure consequence | ARCHITECTURE-APPROPRIATE |
| 11.10.11 | Forbidden UNIQUE implementations | Negative invariants | ARCHITECTURE-APPROPRIATE |
| 11.11 | Lock duration | Strict terminal-duration locking | ARCHITECTURE-APPROPRIATE |
| 11.12 | Lock table | Queue semantics plus implementation advice | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 11.13 | Unified transaction-level gates and deadlock detection | Cross-domain coordination | ARCHITECTURE-APPROPRIATE |
| 11.13.1 | Transaction-level synchronization domains | Closed resource registry | ARCHITECTURE-APPROPRIATE |
| 11.13.2 | Per-statement order and cross-statement admission | Ordering and prohibited transitions | ARCHITECTURE-APPROPRIATE |
| 11.13.3 | Retained-ownership compatibility matrix | Exact later-request outcomes | ARCHITECTURE-APPROPRIATE |
| 11.13.4 | Unified wait-for graph and deterministic victim | Edges, detection, victim | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 11.13.5 | Nontransaction owners, retirement, and lifecycle | Graph/lifecycle boundary | ARCHITECTURE-APPROPRIATE |
| 11.13.6 | Operation gate-order table | Per-operation acquisition/lifetime | ARCHITECTURE-APPROPRIATE |
| 11.13.7 | Adversarial outcomes and completeness argument | Semantic cycle coverage | ARCHITECTURE-APPROPRIATE |
| 11.14 | Physical latches are not LockManager locks | Mechanism separation | ARCHITECTURE-APPROPRIATE |
| 11.15 | Logical-locking invariants | Chapter summary | ARCHITECTURE WITH LOCAL WORDING ISSUE |

# 4. Section-by-section review

Legend: `S` sufficient, `X` exact/clear, `—` not locally applicable.

| Section | Timeless | Owner | Depth | Terms | Identity | Modes | Wait | Release | Owner check | RC/RR | Deadlock | UNIQUE | Revalidation | Failure | X-ref | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 11 | Yes | Arch | S | X | — | — | — | — | — | — | — | — | — | — | — | X | CLEAN |
| 11.1 | Yes | Arch | S | X | X | X | X | — | — | — | X | X | — | — | X | X | CLEAN |
| 11.2 | Yes | Arch | S | X | X | X | X | — | — | — | — | — | X | — | — | X | CLEAN |
| 11.3 | Yes | Arch | S | X | — | — | X | — | — | — | X | — | X | — | X | X | CLEAN |
| 11.4 | Yes | Arch | S | X | X | X | X | X | X | X | — | — | X | X | X | X | CLEAN |
| 11.4.1 | Yes | Arch | S | X | — | — | — | — | X | — | — | — | X | — | — | X | CLEAN |
| 11.4.2 | Yes | Arch | S | X | — | — | — | — | X | — | — | — | X | X | X | X | CLEAN |
| 11.4.3 | Yes | Arch | S | X | — | — | — | — | X | — | — | — | X | X | X | X | CLEAN |
| 11.4.4 | Yes | Arch | S | X | X | X | X | — | X | — | X | — | X | X | X | X | CLEAN |
| 11.4.5 | Yes | Arch | S | X | — | — | — | — | X | X | — | — | X | X | X | X | CLEAN |
| 11.5 | Yes | Arch | S | X | — | — | X | X | X | X | — | — | X | X | X | X | CLEAN |
| 11.6 | Yes | Arch | S | X | — | — | — | X | X | X | — | — | X | X | X | X | CLEAN |
| 11.7 | Yes | Arch | S | X | X | X | X | X | X | X | — | — | X | — | X | X | CLEAN |
| 11.8 | Yes | Arch | S | X | X | X | — | — | — | — | — | X | — | X | X | X | CLEAN |
| 11.9 | Yes | Arch | S | X | X | X | X | X | X | — | X | X | X | X | X | X | CLEAN |
| 11.10 | Yes | Arch | S | X | X | — | X | X | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.1 | Yes | Arch | S | X | — | — | — | — | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.2 | Yes | Arch | S | X | X | — | — | — | X | — | — | X | — | X | X | X | CLEAN |
| 11.10.3 | Yes | Arch | S | X | X | — | — | — | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.4 | Yes | Arch | S | X | X | — | X | — | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.5 | Yes | Arch | S | X | — | — | X | — | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.6 | Yes | Arch | S | X | X | — | X | — | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.7 | Yes | Arch | S | X | X | X | X | X | X | — | X | X | X | X | X | X | CLEAN |
| 11.10.8 | Yes | Arch | S | X | X | — | — | — | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.9 | Yes | Arch | S | X | X | — | X | X | X | — | — | X | X | X | X | X | CLEAN |
| 11.10.10 | Yes | Arch | S | X | — | — | — | X | — | — | — | X | — | X | X | X | CLEAN |
| 11.10.11 | Yes | Arch | S | X | X | — | X | X | X | — | — | X | X | X | X | X | CLEAN |
| 11.11 | Yes | Arch | S | X | — | — | — | X | — | — | — | X | — | X | X | X | CLEAN |
| 11.12 | **No** | Mixed | Clear | X | X | X | X | X | — | — | X | — | — | — | — | X | **FINDING** |
| 11.13 | Yes | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | CLEAN |
| 11.13.1 | Yes | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | CLEAN |
| 11.13.2 | Yes | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | CLEAN |
| 11.13.3 | Yes | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | CLEAN |
| 11.13.4 | **Mostly** | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | **FINDING** |
| 11.13.5 | Yes | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | CLEAN |
| 11.13.6 | Yes | Arch | S | X | X | X | X | X | X | — | X | X | X | X | X | X | CLEAN |
| 11.13.7 | Yes | Arch | S | X | X | X | X | X | — | — | X | — | X | X | X | X | CLEAN |
| 11.14 | Yes | Arch | S | X | — | — | X | — | — | — | X | — | X | — | X | X | CLEAN |
| 11.15 | Yes | Arch | S | Local issue | X | X | X | X | X | X | Local issue | X | X | X | X | X | **FINDING** |

# 5. Ownership boundary

| Mechanism | Canonical owner | Protects | Lifetime | Persistent? | May block? | Role |
|---|---|---|---|---:|---:|---|
| Heap/B+ page latch | Chs. 7–8 | Physical bytes/structure | Guard/operation | No | Yes, short | Physical mutual exclusion |
| MVCC visibility | Ch. 10 | Snapshot read semantics | Statement/transaction snapshot | Metadata persisted; decision runtime | No logical-lock wait | Read correctness |
| `TUPLE_WRITE` | Ch. 11 | Competing UPDATE/DELETE of one physical target | Transaction terminal publication | No | Yes | Lost-update prevention |
| `UNIQUE_KEY` | Ch. 11 | Current logical ownership of one non-NULL unique key | Transaction terminal publication | No | Yes | SQL uniqueness |
| Transaction lifecycle | Ch. 9 | State/admission/status publication | Transaction | Terminal outcome persists | — | Governs legal operations/release |
| DML operation order | Chs. 15/31 | Integration and target materialization | Statement | Effects persist | May invoke waits | Publication sequencing |
| Reclamation/read epochs | Ch. 14 | RID reuse and status retirement | Guard/maintenance horizon | Some state persisted | Maintenance-specific | Physical identity safety |
| Failure continuation | §39.1 | Statement/transaction/database result | Failure boundary | No new lock state | — | Retry/MUST_ABORT/noncontinuable |

Assessment:

- Lock versus latch: exact and unambiguous.
- Lock identity: sufficient and stable.
- Chapter 11 does not redefine snapshot visibility, status lookup, DML publication, or reclamation.
- Ordinary visibility errors reached during write-target revalidation propagate; they do not become conflicts.

# 6. Lock resources and modes

## Lock-resource table

| Resource | Exact identity | Operations | Mode | Lifetime | Persistent? | Revalidate after wait? |
|---|---|---|---|---|---:|---:|
| `TUPLE_WRITE` | `(TableId, physical target RID)` | UPDATE, DELETE | Exclusive | Through terminal publication | No | Yes |
| `UNIQUE_KEY` | `(IndexId, canonical encoded non-NULL user key)` | Unique INSERT/UPDATE/DELETE | Exclusive | Through terminal publication | No | Yes, complete range |
| `SchemaLock` | Database-global schema domain | Schema-changing DDL | Exclusive | Attempt-owned before publication; then terminal | No | Yes |
| `TableWriterGate` | `TableId` | DML/shared; DDL/exclusive | `SHARED_WRITER`, `EXCLUSIVE_DDL` | Terminal once retained for published/persistent work | No | Yes |
| `STATS_PUBLISH` | Table/schema-version/immutable manifest identities | ANALYZE publication | Shared compatible claim | Terminal after first stats publication | No | Yes |
| `MANIFEST_CHANGE` | Table plus affected IndexIds | CREATE/DROP index/table | Exclusive overlapping claim | Terminal after catalog/RETIRING publication | No | Yes |

Identity stability:

- `TableId`, `IndexId`, and catalog object IDs are nonreused.
- Tuple identity includes the physical RID and table identity.
- Read epochs prevent a protected candidate RID from changing meaning.
- Unique-key equality uses the complete canonical Chapter-8 bytes, not a hash.
- Lock keys are database-local; unrelated databases have separate runtime managers.
- Constraint-backed and standalone unique indexes lock by their stable `IndexId`; each valid constraint backing index is unique in catalog metadata.

## Mode and compatibility table

| Domain | Request | Existing other holder | Compatible? |
|---|---|---|---:|
| `TUPLE_WRITE` same key | Exclusive | Exclusive | No |
| `UNIQUE_KEY` same key | Exclusive | Exclusive | No |
| Either logical key | Same owner reacquire | Same owner | Reentrant/idempotent handling |
| Different logical identities | Any v1 mode | Any | Yes |
| `SchemaLock` | Exclusive | Other `SchemaLock` owner | No |
| `TableWriterGate` | Shared | Shared | Yes |
| `TableWriterGate` | Shared | Exclusive | No |
| `TableWriterGate` | Exclusive | Shared or exclusive | No |
| `STATS_PUBLISH` | Shared claim | Compatible statistics claim | Yes |
| `STATS_PUBLISH` | Shared claim | Overlapping manifest change | No |
| `MANIFEST_CHANGE` | Exclusive scope | Overlapping statistics/manifest claim | No |
| Same-owner stronger/identical mode | Reentrant/subsumed | Same owner | Yes, subject to §11.13.2 exceptions |

No S/IS/IX/SIX tuple-lock hierarchy exists. Table/intention/schema/range families are not LockManager key families in v1.

Upgrade/downgrade assessment:

- `TUPLE_WRITE` and `UNIQUE_KEY` have no mode conversion.
- Same-table shared `TableWriterGate -> EXCLUSIVE_DDL` is rejected before waiting.
- Existing same-table exclusive ownership subsumes a later shared request.
- No write-lock downgrade before terminal publication is defined or permitted.
- Lock escalation is outside the v1 baseline.

# 7. Acquisition, release, and ordering

## Acquisition/release table

| Operation | Resources/order | May wait? | Forbidden while waiting | Required recheck | Release |
|---|---|---:|---|---|---|
| INSERT nonunique | Shared writer gate | Yes | Physical latches/epochs | Descriptor/manifest after gate wait | Terminal |
| INSERT unique | Shared gate → sorted unique keys | Yes | Physical latches/epochs | Complete key range/current ownership | Terminal |
| UPDATE | Shared gate → tuple lock → sorted old/new unique keys | Yes | Physical latches/epochs | Target after tuple grant and again after key grants | Terminal |
| DELETE | Shared gate → tuple lock → old unique keys | Yes | Physical latches/epochs | Target after each relevant grant | Terminal |
| CREATE INDEX | Schema → exclusive table → manifest | Yes | Short latches | Table/name/manifest/writer drain | Terminal after publication |
| ANALYZE publication | Stats table → IndexIds ascending | Yes | Page/B+/epoch ownership | Complete current manifest | Terminal after first row |
| COMMIT | No new ordinary gate | Terminal waits separately owned | — | Commit prerequisites | C5 after C4 |
| ABORT | No new ordinary gate | Cleanup only | — | Abort publication | A3 after A2 |

## Lock-order table

| First | Second | Allowed? | Reverse-needed behavior |
|---|---|---:|---|
| Shared `TableWriterGate(T)` | Tuple/unique locks | Yes | Use unified graph for retained cross-statement inversions |
| `TUPLE_WRITE` | Old/new `UNIQUE_KEY` | Yes | Release physical latches first |
| Multiple unique keys | `(IndexId, encoded bytes)` ascending | Required when finite set known | Incremental discovery may deadlock; graph resolves |
| SchemaLock | Table gates by ascending TableId | Required | No same-table shared→exclusive upgrade |
| Table gate | Manifest scopes by TableId/IndexId | Required | Use graph/revalidation |
| Physical latch/epoch | Transaction-lock wait | **Forbidden** | Release, wait, reacquire, revalidate |
| Transaction lock | Short physical latch for mutation | Allowed after grant | Re-fetch identity first |

Strictness assessment: v1 uses targeted strict transaction-duration write/key locks, not shared read locking or full rigorous 2PL over all database access.

# 8. Waiting, cancellation, and lifecycle

- Waiting policy: incompatible requests wait; synchronous cycle detection runs before sleep.
- Queue order: LockManager’s stated v1 queue is FIFO. Compatible gate requests need not be FIFO unless queue policy makes an earlier request a real represented dependency.
- Timeouts: diagnostic only; not victim selection.
- Cancellation:
  - deadlock victim’s outgoing wait is canceled;
  - disconnected transactions enter canonical abort;
  - DRAINING stops new requests and drives nonterminal transactions to terminal handling;
  - no ordinary grant can resume a `MUST_ABORT`, `COMMITTING`, `ABORTING`, or terminal transaction.
- Holder termination:
  - COMMIT: C4 terminal publication, C5 release/wake.
  - ABORT: A2 terminal publication, A3 release/wake.
- Wake-up always discards stale conclusions and performs resource-specific revalidation.
- LockManager/wait queues are runtime coordination state. Recovery reconstructs terminal outcomes, admits no traffic before READY, and does not replay SQL locks or uniqueness checks.
- Chapter 3 requires holders, waiters, snapshots, and gates to drain before subsystem teardown. No transaction can outlive LockManager destruction.

# 9. Current-owner and write-conflict semantics

## Current-owner table

| Owner relation | Status/command | Isolation relation | Wait/retry | MUST_ABORT | Error | Outcome |
|---|---|---|---|---:|---:|---|
| No `xmax` | Invalid sentinel | Any | No | No | No | Proceed |
| SELF earlier command | `cmax < C` | Old version not ordinarily visible | No competing wait | No | No | Revalidation rejects old target; use current version |
| SELF current command | `cmax == C` | Same-command old version visible | No competing wait | No | No | Same-transaction semantics; target spool prevents duplicate mutation |
| SELF future/causal invalid | `cmax > C` or invalid self order | Any | No | Per §39 | Yes | Corruption/internal invariant |
| Other active | `IN_PROGRESS` | Any | Wait, then full recheck | Not merely for waiting | No | Terminal result decides |
| Other committed | `COMMITTED` | RC | Fresh-snapshot retry before first write; otherwise abort | After publication | No | Never mutate stale version |
| Other committed | `COMMITTED` after RR snapshot | RR | No snapshot refresh | Yes | Serialization/write conflict | Abort |
| Other aborted | `ABORTED` | Any | No | No | No | Prior `xmax` ineffective; may overwrite metadata |
| `RETIRED`/`INVALID`/`RESERVED` | Outcome required | Any | No guessing | §39 classification | Yes | Propagate corruption/invariant result |
| Lookup failure | Lower-layer error | Any | No Boolean/conflict guess | Per §39 | Yes | Propagate exact lower error |

## UPDATE/DELETE conflict table

| State | UPDATE | DELETE | RC | RR |
|---|---|---|---|---|
| Uncontended | New version + old `xmax` | Install `xmax/cmax` | Proceed | Proceed |
| SELF current | Same-owner handling; no second competing writer | Same | No independent conflict | No independent conflict |
| SELF prior | Old target fails visibility/revalidation | Same | Search/revalidation owns current target | Same |
| Other active | Wait without physical latch; re-fetch | Same | Re-evaluate after terminal | Re-evaluate, then conflict if committed post-snapshot |
| Other committed | Never mutate stale RID | Same | Retry/FA prewrite; MA postwrite | Transaction-fatal serialization failure |
| Other aborted | Aborted `xmax` ineffective | Same | Proceed after revalidation | Proceed after revalidation |
| Stale/reused/wrong RID | Reject | Reject | Error/retry per owner | Error |
| Invalid metadata/status | Error, not conflict | Error, not conflict | §39 | §39 |

Lost-update prevention is complete: exclusive per-target locking, terminal-duration ownership, post-grant revalidation, RC restart rules, and RR first-updater-wins prevent two stale writers from silently committing.

# 10. Isolation and retry

| Mode | Competing commit after snapshot | Snapshot refresh | CommandId | Prewrite outcome | Postwrite outcome |
|---|---|---|---|---|---|
| READ COMMITTED | Original target may become stale | Fresh snapshot on permitted internal retry | Same logical CommandId | Retry or failed-active result | MUST_ABORT → ABORT |
| REPEATABLE READ | New version remains outside fixed snapshot | None | Current statement command boundary only | Serialization failure | Serialization failure |
| Unique current-state check | Snapshot age irrelevant | Full current-state rescan after wait | Current command used for self ownership | Constraint/wait/error | §39 boundary determines transaction result |

A canonical internal retry is not a new statement. It retains CommandId, discards the old RC snapshot and target/result state, captures a fresh snapshot, and is permitted only before any persistent statement write.

# 11. Uniqueness

## Uniqueness table

| Candidate state | Physical entry | Current-state result | Wait/recheck | Violation? | May proceed? |
|---|---:|---|---:|---:|---:|
| No candidate | No | `NO_CONFLICT` | No | No | Yes |
| Fully non-NULL committed/frozen live owner | Yes | `UNIQUE_CONFLICT` | No | Yes | No |
| Owner committed but invisible to caller snapshot | Yes | `UNIQUE_CONFLICT` | No | Yes | No |
| Other active creator/deleter | Yes | `WAIT_THEN_RECHECK` | Yes, complete rescan | Not yet | Only after terminal recheck |
| Aborted creator | Yes | `IGNORE_ABORTED_OR_STALE` | No | No | Yes if no other owner |
| Aborted deleter | Yes | Old owner remains | No | Yes | No |
| Committed effective deleter | Yes | Stale candidate ignored | No | No | Yes if no other owner |
| Exact current UPDATE old RID | Yes | `SELF_EXCLUDED` | No | No | Yes |
| Same transaction, another live row | Yes | `UNIQUE_CONFLICT` | No | Yes | No |
| Same-command other-row delete | Yes | Still conflicts | No | Yes | No |
| Earlier-command self-delete | Yes | Stale/ignored | No | No | Yes |
| RETIRED/INVALID/RESERVED required status | Yes | Corruption/invariant family | No | No | No |
| Lookup I/O/format/corruption failure | Unknown | Propagated lower error | No | No | No |
| Dangling/mismatched/reused RID | Yes | Corruption | No | No | No |
| NULL-containing UNIQUE key | May exist | Outside duplicate domain | No key lock | No | Yes |
| PRIMARY KEY NULL | N/A | NOT NULL violation first | No | Constraint error | No |

## Unique races

| Ordering | Holder terminal result | Waiter after lock grant | Legal survivor | Forbidden |
|---|---|---|---|---|
| T1 locks K first; T2 waits | T1 COMMITTED | Full rescan finds T1 | T1 | Both commit K |
| T1 locks K first; T2 waits | T1 ABORTED | Full rescan ignores T1 garbage | T2 may proceed | Treat aborted T1 as permanent owner |
| T2 locks first | Symmetric | Symmetric | T2 if committed | Both commit K |
| Delete K holds lock; insert waits | Delete COMMITTED | Old owner stale; insert may proceed | Inserter | Proceed before delete terminal |
| Delete K holds lock; insert waits | Delete ABORTED | Old owner still conflicts | Original owner | Treat aborted delete as freeing K |
| UPDATE row A→K; INSERT K | First key-lock owner serializes | Current-state recheck | At most one owner | Snapshot-based duplicate admission |
| Two UPDATEs to K | Same | Same | At most one | End-of-statement permutation acceptance |

Additional conclusions:

- Unique identity is per `IndexId`, not `ConstraintId`.
- Several unique constraints acquire keys in global `(IndexId, bytes)` order.
- Nonunique indexes do not use `UNIQUE_KEY`.
- CREATE UNIQUE INDEX uses writer exclusion and the same current-state semantics without per-row key locks.
- Physical duplicate key bytes are never semantic uniqueness authority.

# 12. Deadlock model

| Aspect | Canonical rule |
|---|---|
| Graph | One database-local transaction wait-for graph |
| Nodes | Normal TxnIds |
| Edges | Waiter → every owner or queue predecessor preventing grant |
| Covered domains | Schema, both table-gate modes, both publication modes, tuple, unique |
| Installation | Atomic with observing blockers, before sleep |
| Detection | Synchronous cycle detection on edge addition/replacement |
| Victim | Highest normal TxnId in each cyclic strongly connected component |
| Victim result | `DEADLOCK_DETECTED`, transaction enters `MUST_ABORT` |
| Release | Only after ABORTED publication, at A3 |
| Survivor | Wakes on actual release and fully revalidates |
| Timeout | Diagnostics only |
| Ordinary cycle | Transaction-fatal, not database-noncontinuable |
| Graph incoherence | Internal invariant/noncontinuable path |
| Retry | Victim transaction is aborted; no same-TxnId statement continuation |

The detailed rule is complete. Finding 3 concerns only §11.15’s less precise summary wording.

# 13. Transaction state and locks

| State | New ordinary locks? | Existing wait | Ownership | Release | Ordinary statement? |
|---|---:|---|---|---|---:|
| ACTIVE | Yes | May wait | Retained | No | Yes |
| MUST_ABORT | No | Canceled/abort-driven | Retained | A3 after A2 | No |
| COMMITTING | No | Terminal protocol only | Retained | C5 after C4 | No |
| ABORTING | No | Abort cleanup only | Retained | A3 after A2 | No |
| COMMITTED | No | None | Cleanup releases | C5 | No |
| ABORTED | No | None | Cleanup releases | A3 | No |

Read-only and SELECT behavior:

- Ordinary SELECT takes no tuple read lock and no Chapter-11 transaction gate.
- MVCC supplies isolation; absence of shared tuple locks does not imply dirty reads.
- Read-only transactions still own snapshots and lifecycle state.
- `SELECT FOR UPDATE` has no Chapter-11 v1 contract and is not part of the reviewed supported surface.
- Predicate/key-range locking and SERIALIZABLE are deferred from v1.
- Savepoints, prepared transactions/2PC, and lock escalation are outside the v1 baseline.

# 14. Failure matrix

| Condition | Statement/transaction result | Retry? | Ownership | Owner |
|---|---|---|---|---|
| Incompatible holder | Wait | After grant/recheck | Retained | Ch. 11 |
| Wait cancellation before write | Failed-active unless independently fatal | New statement, not late grant | Holder locks unaffected | §39 |
| Deadlock victim | MUST_ABORT → ABORTED | New transaction only | Retained through A3 | §11.13/§39 |
| Lock entry/waiter allocation OOM | FA prewrite; MA postwrite | Per §39 | Coherent prior ownership retained | §39 |
| Graph coherence cannot be maintained | Internal invariant/noncontinuable | No | Do not guess/release | §11.13.4/§39 |
| Status lookup failure | Propagate exact lower error | Only if owning error policy permits | No conflict guess | Chs. 9–10/§39 |
| Current-owner corruption | Corruption/internal invariant | No ordinary retry | Do not mutate | Chs. 10–11/§39 |
| Unique conflict | `UniqueViolation` | No internal duplicate retry | Locks terminal-held as applicable | §11.10/§39 |
| Unique probe I/O/corruption | Exact lower/corruption result | Per §39 | No “free key” | Chs. 8, 11, 39 |
| Shutdown while waiting | Cancellation/forced terminal handling | No ordinary continuation | Release only terminally | Ch. 3 |
| Database noncontinuable | No ordinary continuation | Recovery/controlled stop | Retain unsafe ownership | Ch. 3/§39 |

No new error aliases are invented by Chapter 11. `UNIQUE_CONFLICT`, `WAIT_THEN_RECHECK`, and `CORRUPTION_OR_INTERNAL_ERROR` are conceptual predicate outcomes; external transaction consequences remain §39-owned.

# 15. Concurrency/semantic matrix

| Interaction | Ordering point | Legal outcome | Forbidden outcome |
|---|---|---|---|
| UPDATE/UPDATE same RID | Tuple lock | One waits; loser revalidates | Both overwrite stale version |
| UPDATE/DELETE same RID | Tuple lock | Serialized, revalidated | Both publish against same stale owner |
| DELETE/DELETE same RID | Tuple lock | Serialized | Double independent delete |
| RC stale target | Post-grant revalidation | Fresh retry prewrite or abort postwrite | Blind stale mutation |
| RR stale target | Post-grant revalidation | Serialization abort | Follow invisible newer version |
| Unique INSERT/INSERT K | Key lock | One current owner | Two committed K owners |
| Unique UPDATE/INSERT K | Key lock/current-state scan | One succeeds | Snapshot-only decision |
| Delete K/insert K | Key lock until delete terminal | Commit frees; abort preserves | Freeing uncommitted delete |
| Waiter/holder COMMIT | C4 then C5 | Wake sees COMMITTED | Release while IN_PROGRESS |
| Waiter/holder ABORT | A2 then A3 | Wake sees ABORTED | Release before status publication |
| Waiter transaction abort | State transition | Wait canceled; no late grant | Ordinary statement resumes |
| Transaction wait/page latch | Before sleep | Release latch first | Cross-layer deadlock |
| Unique candidate/RID reuse | Read epoch | Stable identity or corruption | Mutate reused RID |
| Cross-gate cycle | Unified graph | Highest-TxnId victim | Separate invisible cycles |

# 16. Cross-chapter consistency

| Chapter | Comparison | Result |
|---|---|---|
| 3 | DRAINING, cancellation, teardown | CONSISTENT |
| 5 | Physical RID and new-RID UPDATE | CONSISTENT |
| 7 | Latch/lock separation | CONSISTENT |
| 8 | Physical duplicates and heap recheck | CONSISTENT |
| 9 | States, retry identity, terminal release | CONSISTENT |
| 10 | Visibility, SELF causality, no-guessing errors | CONSISTENT BUT SPECIALIZED |
| 12 | WAL publication and no physical abort undo | CONSISTENT |
| 13 | Recovery outcome reconstruction; no SQL unique replay | CONSISTENT |
| 14 | Read epochs, status retirement, reclamation | CONSISTENT |
| 15 | DML lock/publication sequence | CONSISTENT |
| 16 | Stable index/constraint identities | CONSISTENT |
| 21 | DDL gates and offline unique build | CONSISTENT |
| 31 | Target materialization and one-target-once | CONSISTENT |
| 39 | Retry/MUST_ABORT/deadlock/error effects | CONSISTENT |
| 41 | High-level verification obligations | CONSISTENT |

# 17. Explicit cross-reference audit

| Source | Target | Purpose | Exists/owner | Precision/status |
|---|---|---|---|---|
| 11.1 | §11.13 | Unified graph | Yes/11 | Precise |
| 11.4.5 | §§11.5–11.6 | Isolation dispatch | Yes/11 | Precise |
| 11.5 | §15.7 | RC retry boundary | Yes/15 | Precise |
| 11.8 | Chapter 8 | Key representation | Yes/8 | Correct, broad |
| 11.8 | Chapter-8 `IndexKeyCodec` | Canonical encoding | Yes/8 | Name-precise |
| 11.9 | §11.4 | Tuple lock first | Yes/11 | Precise |
| 11.9 | §11.10 | Unique recheck | Yes/11 | Precise |
| 11.9 | §11.13 | Cross-key deadlock | Yes/11 | Precise |
| 11.10.1 | §§15.2–15.3 | Heap/index publication interval | Yes/15 | Precise |
| 11.10.2 | §11.8 | Equality | Yes/11 | Precise |
| 11.10.4 | §14.6 | RID protection | Yes/14 | Precise |
| 11.10.4 | §12.12 | Provisional publication | Yes/12 | Precise |
| 11.10.6 | §11.9 | Key order | Yes/11 | Precise |
| 11.10.6 | §15.3 | UPDATE publication | Yes/15 | Precise |
| 11.10.8 | §14.6 | RID reuse | Yes/14 | Precise |
| 11.10.8 | Chapter 14 | Vacuum cleanup | Yes/14 | Broad but correct |
| 11.10.9 | §21.8 | CREATE INDEX | Yes/21 | Precise |
| 11.10.10 | §39.1 | Failure boundary | Yes/39 | Precise |
| 11.11 | §9.14 | Terminal publication | Yes/9 | Precise |
| 11.13.1 | §9.14 | Gate retention | Yes/9 | Precise |
| 11.13.1 | §§15.5–15.6 | Release points | Yes/15 | Precise |
| 11.13.1 | §39.1.4 | Clean prepublication release | Yes/39 | Precise |
| 11.13.1 | §§14.17.1/39.1.4 | Publication gates | Yes/14,39 | Precise |
| 11.13.1 | §§11.2/11.8/11.11 | Logical keys/lifetime | Yes/11 | Precise |
| 11.13.2 | §39.1 | Proactive rejection | Yes/39 | Precise |
| 11.13.3 | §11.13.4 | Wait behavior | Yes/11 | Precise |
| 11.13.4 | §14.17.1 | Manifest recheck | Yes/14 | Precise |
| 11.13.4 | §§11.3/11.10 | Target/key recheck | Yes/11 | Precise |
| 11.13.4 | §15.6 | Victim abort | Yes/15 | Precise |
| 11.13.4 | §39.1 | Victim classification | Yes/39 | Precise |
| 11.13.5 | §3.3.6 | DRAINING | Yes/3 | Precise |
| 11.13.5 | §39.1.7 | Disconnect | Yes/39 | Precise |
| 11.13.6 | §14.17.1 | Vacuum gate order | Yes/14 | Precise |
| 11.15 | §§11.10/11.13 | Summary | Yes/11 | One local wording issue |

No vague “later WAL/recovery chapter” navigation occurs in Chapter 11.

# 18. Terminology

| Term | Canonical meaning | Assessment |
|---|---|---|
| Transaction lock | Transaction-lifetime logical conflict protection | Precise |
| Page latch | Short-lived physical byte/structure protection | Precise |
| Resource | One exact logical lock/gate identity | Precise |
| Holder/owner | Transaction currently retaining a resource | Precise by context |
| Logical key owner | Tuple version that currently owns unique K | Distinct from lock owner |
| Waiter | Transaction request blocked by represented dependency | Precise |
| Grant | Permission after compatibility/queue checks | Precise |
| Write conflict | Competing current modification of target version | Precise |
| Unique conflict | Another current logical owner of K | Precise |
| Retry | Same logical RC statement attempt before publication | Precise |
| MUST_ABORT | Nonterminal transaction-fatal state | Precise |
| Deadlock victim | Highest TxnId in cyclic SCC | Precise in §11.13.4 |
| Unique key | Fully non-NULL canonical encoded key for one IndexId | Precise |
| Unique violation | Constraint result after current-owner check | Precise |
| Cancellation | Removes wait/admission; not early ownership release | Precise |
| Revalidation | Fresh physical/current-state check after wait | Precise |
| Stale | Physically present but no longer current logical owner | Precise |

Only §11.15’s “cycle” summary is locally less precise than “cyclic component.”

# 19. Normative-language assessment

| Requirement family | Language/result |
|---|---|
| No wait under page latch | `MUST NOT`; strong and exact |
| Re-fetch/revalidate | `MUST`; exact |
| RC stale mutation | `MUST NOT`; exact |
| RR conflict | Mandatory abort; exact |
| Key equality | `MUST` use complete bytes; exact |
| Unique post-lock scan | `MUST`; exact |
| Terminal-duration release | Declarative plus `MUST NOT`; exact |
| Deadlock edges/detection | Mandatory semantic requirements; exact |
| Victim policy | Exact highest-TxnId SCC rule |
| Failure propagation | Cross-owned by Chs. 9–10 and §39 |
| §11.12 container | `MAY`, but document-role inappropriate |
| Optional RC prewrite retry | Intentional `may`; §39 permits failed-active alternative |
| Queue fairness | Deliberate implementation freedom if dependencies are represented |

# 20. Temporality, ownership, and analytical depth

## Temporal-language classification

| Occurrence | Category | Assessment |
|---|---|---|
| “currently visible/current transaction/current command/current owner” | A/B runtime/MVCC | Valid |
| “earlier/later command/statement/waiter” | B transaction history | Valid |
| “after lock acquisition/after wake/through terminal publication” | A runtime ordering | Valid |
| “future self cmin/cmax” | B impossible transaction history | Valid |
| “v1/deferred lock families” | D durable scope | Valid |
| Cross-section references | E navigation | Valid |
| “The initial LockManager MAY use … hash map … mutex” | F project/implementation chronology | Finding |
| “The abstraction must allow later sharding/shared modes…” | F roadmap | Finding |
| “existing youngest-transaction policy, now applied…” | F historical wording | Finding |
| “initial victim” in invariant 20 | A deadlock sequence, but coupled to imprecise “cycle” | Local wording finding |

## Document ownership

| Material | Proper owner | Chapter-11 result |
|---|---|---|
| Lock identities/compatibility/lifetime | ARCHITECTURE | Correct |
| Wait graph/victim semantics | ARCHITECTURE | Correct |
| Unique current-owner predicate | ARCHITECTURE | Correct |
| Hash map/mutex implementation sequence | DEVELOPMENT | Leakage in §11.12 |
| Sharding/shared-mode future sequencing | DEVELOPMENT or timeless deferred scope | Leakage in §11.12 |
| Deterministic barrier procedures | VERIFICATION | Not present |
| Implementation availability | PROJECT_STATE | Not present |
| Historical test/milestone evidence | devlog | Not present |

## Analytical depth

| Mechanism | Rating |
|---|---|
| Lock/latch separation | ANALYTICALLY SUFFICIENT |
| Tuple identity/revalidation | ANALYTICALLY SUFFICIENT |
| RC/RR write conflict | ANALYTICALLY SUFFICIENT |
| Lost-update prevention | ANALYTICALLY SUFFICIENT |
| Unique identity/equality | ANALYTICALLY SUFFICIENT |
| Uniqueness vs visibility | ANALYTICALLY SUFFICIENT |
| Unique wait/recheck | ANALYTICALLY SUFFICIENT |
| Release/publication ordering | ANALYTICALLY SUFFICIENT |
| Cross-resource deadlock | ANALYTICALLY SUFFICIENT |
| Victim/cleanup semantics | ANALYTICALLY SUFFICIENT |
| Runtime lock-table recovery nature | SEMANTICALLY CLEAR BUT RATIONALE THIN |
| Resource-exhaustion handling | SEMANTICALLY CLEAR THROUGH §39 |
| §11.12 implementation mechanics | DOCUMENT-ROLE FINDING |

Source-layout coupling: no `.cpp`, `.h`, source path, TODO, test count, or milestone appears. The hash-map/mutex prescription is implementation-algorithm coupling rather than source-layout coupling.

# 21. Technical consistency matrix

| # | Item | Result |
|---:|---|---|
| 1 | Chapter-11 ownership boundary | CONSISTENT |
| 2 | Transaction lock vs page latch | CONSISTENT |
| 3 | Resource families | CONSISTENT |
| 4 | Resource identity | CONSISTENT |
| 5 | Identity stability | CONSISTENT |
| 6 | Lock modes | CONSISTENT |
| 7 | Compatibility | CONSISTENT |
| 8 | Acquisition semantics | CONSISTENT |
| 9 | Reentrant acquisition | CONSISTENT BUT SPECIALIZED |
| 10 | Lock ownership | CONSISTENT |
| 11 | Release point | CONSISTENT |
| 12 | Terminal-publication ordering | CONSISTENT |
| 13 | Upgrade semantics | CONSISTENT |
| 14 | Downgrade semantics | N/A |
| 15 | Wait policy | CONSISTENT |
| 16 | Queue ordering | CONSISTENT |
| 17 | Cancellation | CONSISTENT |
| 18 | Canceled waiter no-late-grant | CONSISTENT BUT SPECIALIZED |
| 19 | No wait under page latch | CONSISTENT |
| 20 | Acquire/release/revalidate | CONSISTENT |
| 21 | Current-owner inputs | CONSISTENT |
| 22 | SELF prior command | CONSISTENT BUT SPECIALIZED |
| 23 | SELF current command | CONSISTENT BUT SPECIALIZED |
| 24 | SELF future command | CONSISTENT |
| 25 | Same-owner causal corruption | CONSISTENT |
| 26 | Other IN_PROGRESS owner | CONSISTENT |
| 27 | Other COMMITTED owner | CONSISTENT |
| 28 | Other ABORTED owner | CONSISTENT |
| 29 | RETIRED owner | CONSISTENT BUT SPECIALIZED |
| 30 | INVALID owner | CONSISTENT BUT SPECIALIZED |
| 31 | RESERVED owner | CONSISTENT BUT SPECIALIZED |
| 32 | Lookup failure | CONSISTENT BUT SPECIALIZED |
| 33 | UPDATE conflict | CONSISTENT |
| 34 | DELETE conflict | CONSISTENT |
| 35 | Lost-update prevention | CONSISTENT |
| 36 | RC write conflict | CONSISTENT |
| 37 | RR write conflict | CONSISTENT |
| 38 | Canonical retry | CONSISTENT |
| 39 | MUST_ABORT transition | CONSISTENT |
| 40 | Statement-local conflict | CONSISTENT |
| 41 | Deadlock existence | CONSISTENT |
| 42 | Deadlock graph | CONSISTENT |
| 43 | Deadlock victim | CONSISTENT; summary wording finding |
| 44 | Victim transaction result | CONSISTENT |
| 45 | Lock-table runtime nature | CONSISTENT BUT SPECIALIZED |
| 46 | Recovery lock reset | CONSISTENT BUT SPECIALIZED |
| 47 | Shutdown wait cancellation | CONSISTENT |
| 48 | Lock-manager lifetime | CONSISTENT |
| 49 | Unique constraint owner | CONSISTENT |
| 50 | Unique key identity | CONSISTENT |
| 51 | Unique NULL semantics | CONSISTENT |
| 52 | Concurrent unique INSERT | CONSISTENT |
| 53 | Concurrent unique UPDATE | CONSISTENT |
| 54 | Unique delete/insert race | CONSISTENT |
| 55 | Aborted unique candidate | CONSISTENT |
| 56 | Active unique candidate | CONSISTENT |
| 57 | Snapshot-invisible committed candidate | CONSISTENT |
| 58 | RETIRED/INVALID/RESERVED candidate | CONSISTENT |
| 59 | Uniqueness lookup failure | CONSISTENT BUT SPECIALIZED |
| 60 | Unique lock release | CONSISTENT |
| 61 | Nonunique indexes | CONSISTENT |
| 62 | Constraint identity | CONSISTENT |
| 63 | Multi-unique lock order | CONSISTENT |
| 64 | Row-vs-key lock order | CONSISTENT |
| 65 | Multirow DML | CONSISTENT |
| 66 | Lock escalation | N/A/deferred |
| 67 | Resource exhaustion | CONSISTENT BUT SPECIALIZED |
| 68 | Waiter termination | CONSISTENT |
| 69 | Holder termination | CONSISTENT |
| 70 | Wake-up revalidation | CONSISTENT |
| 71 | Stale RID after wait | CONSISTENT |
| 72 | Read-only transaction locks | CONSISTENT |
| 73 | Ordinary SELECT lock behavior | CONSISTENT |
| 74 | SELECT FOR UPDATE | N/A |
| 75 | Predicate/range-lock scope | CONSISTENT/deferred |
| 76 | Parallel-ready compatibility | CONSISTENT |
| 77 | Implementation-container freedom | **FINDING** |
| 78 | Conflict vs corruption | CONSISTENT |
| 79 | Conflict vs visibility | CONSISTENT |
| 80 | Implementer invention | CONSISTENT; none required |

# 22. Documentation-model matrix

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | Runtime concurrency language preserved | CONSISTENT |
| 3 | No current implementation status | CONSISTENT |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No implementation sequencing | FINDING |
| 6 | No VERIFICATION procedure leakage | CONSISTENT |
| 7 | No PROJECT_STATE leakage | CONSISTENT |
| 8 | No devlog/history | CONSISTENT, except one history-shaped phrase |
| 9 | No source-layout coupling | CONSISTENT |
| 10 | Lock/latch terminology | CONSISTENT |
| 11 | Conflict/visibility distinction | CONSISTENT |
| 12 | Conflict/corruption distinction | CONSISTENT |
| 13 | Uniqueness/physical duplicate distinction | CONSISTENT |
| 14 | Retry/abort distinction | CONSISTENT |
| 15 | Holder/waiter terminology | CONSISTENT |
| 16 | Release/publication rationale | CONSISTENT |
| 17 | Current-owner rationale | CONSISTENT |
| 18 | Deadlock rationale | CONSISTENT |
| 19 | Uniqueness-race rationale | CONSISTENT |
| 20 | Readable without implementation-status knowledge | CONSISTENT |

# 23. Findings

## BLOCKING findings

None.

## MAJOR findings

None.

## MINOR findings

### F11-1 — §11.12 implementation sequencing and roadmap leakage

- Exact section: §11.12, lines 8420–8449.
- Evidence: “The initial LockManager MAY use … `hash map<LockKey, LockQueue>` … protected by a conventional mutex” and “The abstraction must allow later: sharding, shared lock modes…”
- Severity: MINOR
- Type: DOCUMENT OWNERSHIP
- Scope: Local
- Explanation: This fixes an initial implementation mechanism and narrates later evolution. The semantic queue/compatibility contract is architectural; hash-map/mutex sequencing is DEVELOPMENT-owned.
- Canonical comparison: `DEVELOPMENT.md` already owns transaction/durability implementation sequencing and explicitly places LockManager/deadlock work in its development phases.
- Consequence: The architecture becomes progress-shaped and unnecessarily constrains implementation mechanics.
- Correct owner: `docs/DEVELOPMENT.md` for implementation guidance; durable v1 scope may remain in architecture.
- Future action: **D. DEVELOPMENT-OWNERSHIP FIX**

### F11-2 — §11.13.4 historical transition wording

- Exact section: §11.13.4, lines 8637–8638.
- Evidence: “This is the existing youngest-transaction policy, now applied across every registered resource family.”
- Severity: MINOR
- Type: TEMPORALITY
- Scope: Local
- Explanation: “existing” and “now applied” describe a document/design transition rather than the resulting timeless rule.
- Canonical comparison: The canonical rule is simply that each cyclic SCC selects its highest normal TxnId across all registered resource families.
- Consequence: Live architecture contains unnecessary historical narration.
- Correct owner: Timeless `ARCHITECTURE.md` rewrite; provenance, if retained, belongs in devlog/review history.
- Future action: **B. TIMELESSNESS REWRITE**

### F11-3 — §11.15 deadlock summary is less precise than §11.13.4

- Exact section: §11.15 invariant 20, line 8800.
- Evidence: “the initial victim is the highest TxnId in the cycle.”
- Severity: MINOR
- Type: DEADLOCK
- Scope: Cross-section, §11.15 versus §11.13.4
- Explanation: The canonical algorithm selects the highest normal TxnId in each cyclic strongly connected component. “The cycle” is ambiguous for one SCC containing multiple overlapping cycles.
- Canonical comparison: §11.13.4: “for every cyclic strongly connected component … the highest normal TxnId in that cyclic component.”
- Consequence: An implementer reading only the invariant summary could choose a cycle-local victim inconsistent with the canonical SCC rule.
- Correct owner: `docs/ARCHITECTURE.md`
- Future action: **O. DEADLOCK CLARIFICATION**

## EDITORIAL findings

None.

# 24. Frozen semantic questions

**None.**

No undefined choice was found for:

- resource identity;
- compatibility;
- release timing;
- current-owner conflict;
- RC/RR conflict;
- retry/MUST_ABORT;
- deadlock victim handling;
- uniqueness races;
- cancellation;
- wake-up revalidation.

# 25. Follow-up verification gaps

Existing verification is strong for unified graph edges, adversarial deadlocks, RC/RR conflict behavior, terminal release, unique races, wait/recheck, and current-command uniqueness.

The following should receive more explicit procedural ownership:

1. Direct `TUPLE_WRITE` current-owner matrix for `RETIRED`, `INVALID`, `RESERVED`, future SELF metadata, and exact status-lookup failure propagation—not only ordinary visibility coverage.
2. Unique current-state fixtures explicitly naming `RETIRED`, `INVALID`, `RESERVED`, and unsupported/I/O lookup failures rather than relying on the generic “impossible status” row.
3. Lock-entry, waiter, and wait-graph allocation/resource-exhaustion fixtures on both sides of the first-published-write boundary.
4. Recovery fixture proving pre-crash holders/waiters/edges do not survive and no SQL lock/unique predicate is replayed before READY.
5. Explicit FIFO/reentrant/same-owner LockManager queue checks and a no-late-grant assertion after cancellation.

These are **FOLLOW-UP VERIFICATION GAPS**, not architecture defects.

# 26. Out-of-scope observations

| Source | Observation | Future owner |
|---|---|---|
| §14.17 | “Background scheduling may later react…” is roadmap-shaped wording | Chapter-14 review |
| §15.7.2–15.7.3 | Future opt-in retry/savepoint language is evolution-shaped | Chapter-15 review |
| §31.7 | Later-architecture wording around parallel writes is outside Chapter 11 | Chapter-31 review |

None directly contradicts Chapter 11 and none is counted here.

# 27. Direct review answers

| Question | Answer |
|---|---|
| Lock/latch ownership ambiguity? | No |
| Lock-resource identity ambiguity? | No |
| Compatibility ambiguity? | No |
| Release-before-terminal path? | No |
| Wait-under-page-latch ambiguity? | No |
| Current-owner conflict ambiguity? | No architecture-wide ambiguity |
| SELF/future contradiction with Ch. 10? | No |
| RC conflict ambiguity? | No |
| RR conflict ambiguity? | No |
| Retry/MUST_ABORT ambiguity? | No |
| Deadlock semantic ambiguity? | Only §11.15’s localized cycle/SCC summary wording |
| Unique-key identity ambiguity? | No |
| Concurrent unique-commit hole? | No |
| Unique visibility/recheck ambiguity? | No |
| Waiter cancellation/lifetime ambiguity? | No |
| Wake-up revalidation ambiguity? | No |
| Correctness policy invention required? | No |
| Project-time wording? | Yes, localized §§11.12 and 11.13.4 |
| DEVELOPMENT-owned material? | Yes, §11.12 |
| VERIFICATION procedure leakage? | No |
| PROJECT_STATE material? | No |
| Devlog/history material? | No substantive history; one history-shaped sentence |
| Ambiguous terminology? | Only “cycle” versus cyclic SCC in §11.15 |
| Analytically underexplained boundary? | No finding-level correctness boundary |
| Can it stand as timeless v1 contract? | Semantically yes; not fully document-clean until the targeted wording fixes |

# 28. Global documentation-model assessment

- Analytical rather than chronological: **Yes overall**, with two localized chronology/roadmap phrases.
- Current-state narration: **No**.
- DEVELOPMENT sequencing leakage: **Yes, §11.12**.
- VERIFICATION procedure leakage: **No**. The §11.13.7 timelines analyze required outcomes rather than prescribe test barriers.
- PROJECT_STATE leakage: **No**.
- Devlog/history leakage: **No substantive history**, though §11.13.4 uses history-shaped wording.
- Lock/conflict terminology: **Precise**, except the deadlock summary.
- Rationale: **Sufficient** at conflict, wait, release, deadlock, and uniqueness boundaries.
- Readable without knowing implementation status: **Yes**.
- Timeless canonical result: **Technically yes, document-clean only after targeted cleanup**.

# 29. Previous-chapter regression and Chapter-10 compatibility

- Chapter 5 physical RID/new-version identity: preserved.
- Chapter 7 page latch versus transaction lock: preserved.
- Chapter 8 physical duplicate keys and heap recheck: preserved.
- Chapter 9 state/retry/terminal-release rules: preserved.
- Chapter 10 visibility, SELF causality, status no-guessing, and error propagation: preserved.
- No invalid status or impossible SELF state is converted into a normal conflict or invisibility result.
- No physical candidate or index presence becomes semantic authority.

# 30. Recommended next actions

Recommended immediate action:

**targeted documentation edit**

Scope:

1. Make §11.12 timeless and implementation-free while retaining exact v1 exclusive compatibility and queue semantics.
2. Remove historical “existing/now applied” wording from §11.13.4.
3. Align §11.15 invariant 20 with the cyclic-SCC victim rule.
4. Then synchronize the narrow verification gaps listed above.

No frozen semantic review is required.

Recommended Chapter-12 review scope, based on the actual boundary:

- WAL stream and transaction ownership;
- terminal commit/abort records;
- status-page mutation protocol;
- provisional mutation/publication atomicity;
- heap-before-index WAL order;
- no ordinary user-DML undo;
- abort treatment of B+ structural shape;
- CommitCoordinator/durable LSN;
- WAL-before-data;
- failure classification around append, durability, and terminal publication;
- consistency with Chapter 11’s rule that logical locks release only after C4/A2 terminal publication and never merely at WAL append, page mutation, or data-page flush;
- recovery redo must not rerun SQL UNIQUE enforcement.

# 31. Repository-state and read-only guarantee

Initial state:

- `git status --short`: clean
- `git diff --cached --name-only`: empty
- HEAD: `5f220508bf51d8ab622483a03d7072cbaca49324`

Final state:

- `git status --short`: clean
- `git diff --cached --name-only`: empty
- HEAD: `5f220508bf51d8ab622483a03d7072cbaca49324`
- `git diff --check`: passed, no output
- Scoped documentation diff: empty

Final confirmations:

- Files modified by audit: **NONE**
- Repository state changed by audit: **NO**
- Pre-existing material modified or staged: **NO**
- Build/tests/benchmarks run: **NO**
- Implementation work performed: **NO**
- Chapter 12 review started: **NO**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**