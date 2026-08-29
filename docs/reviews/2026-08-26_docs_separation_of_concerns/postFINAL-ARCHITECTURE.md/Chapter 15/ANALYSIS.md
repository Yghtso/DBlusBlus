# Chapter 15 review verdict

**CHAPTER 15 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 15’s central concurrency, MVCC, retry, uniqueness, WAL, abort, and recovery model is coherent. No path permits retry after a published statement write, committed partial DML, stale-RID rebinding, or physical user-DML undo.

Two architecture-owned results remain undefined:

1. the canonical persisted `cmax` value for a newly created tuple whose `xmax = INVALID_TXN_ID`;
2. affected-row counting and publication semantics.

There is also one major cross-chapter latch-order wording conflict and three minor documentation findings.

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 3 |
| MINOR | 3 |
| EDITORIAL | 0 |

## Scope and repository state

Primary scope: [Chapter 15](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12027), through the line preceding Chapter 16.

Context consulted:

- Chapters 3–14 as closed semantic owners, particularly Chapters 5, 7–14;
- [§39.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24050);
- [§41.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24900);
- §21.2.1 and §21.5 for writer-gate/DDL ownership;
- §§31.1–31.6 and §31.9 where needed for target-spool and `RETURNING` delegation;
- `docs/VERIFICATION.md`, `docs/DEVELOPMENT.md`, and `docs/PROJECT_STATE.md` only for role and coverage classification.

§31.7 was not reviewed because Chapter 15 does not explicitly reference it. Review artifacts were not inspected.

Initial repository state:

- Working tree: clean
- Index: clean
- HEAD: `a3baa64b964212beca8a9912acc5cb214c272508`

## Actual Chapter-15 structure

| Section | Exact heading | Responsibility | Status |
|---|---|---|---|
| 15 | Transactional Write Protocols | Cross-subsystem DML and terminal protocol integration | CLEAN WITH NOTE |
| 15.1 | Scope and ownership | Defines integration boundary and preserves subsystem ownership | CLEAN |
| 15.1.1 | DDL writer-gate integration | Shared writer gate, DDL order, status-dependency precondition | CLEAN |
| 15.2 | INSERT | Unique admission, heap publication, index publication, abort residue | FINDING |
| 15.3 | UPDATE | RID handoff, conflict handling, uniqueness, old/new versions | FINDING |
| 15.4 | DELETE | RID handoff, logical deletion, retained indexes and key lock | CLEAN |
| 15.5 | COMMIT | C0–C6 terminal protocol | CLEAN |
| 15.6 | ABORT | A0–A4 semantic abort/no-undo protocol | CLEAN |
| 15.7 | READ COMMITTED retry boundary | First-published-write boundary | CLEAN |
| 15.7.1 | Retry before the first persistent write | Clean RC internal retry | CLEAN |
| 15.7.2 | Conflict after a persistent statement write | Mandatory abort and new-transaction external retry | FINDING |
| 15.7.3 | External output | `RETURNING` publication safety | FINDING |
| 15.8 | Cross-layer contract | Prevents upper-layer semantic redefinition | CLEAN |
| 15.9 | End-to-end invariants | Cross-layer normative summary | FINDING |

All sections are architecture-owned. No verification procedure, implementation-status report, source layout, or historical material appears in Chapter 15.

# Core semantic assessment

## Statement, attempt, snapshot, and CommandId

One admitted SQL statement receives one CommandId. It may have several internal READ COMMITTED attempts.

| Attempt/outcome | Snapshot | CommandId | Published write? | Transaction result | Locks |
|---|---|---|---:|---|---|
| First RC attempt | Fresh statement snapshot | Assigned once | Initially no | `ACTIVE` | Acquired locks become terminal-duration |
| Clean RC retry | Discard old; capture fresh snapshot | Retained, not consumed | No | `ACTIVE` | Existing transaction locks remain |
| RR attempt | Fixed transaction snapshot, current command boundary | Assigned once | Initially no | `ACTIVE` | Terminal-duration |
| Successful statement | Snapshot released as applicable | Consumed | Maybe | Explicit transaction remains `ACTIVE`; autocommit enters COMMIT | Retained |
| Recoverable failed statement | Snapshot released | Consumed | No | `FAILED_TRANSACTION_REMAINS_ACTIVE` | Transaction locks remain |
| Transaction-fatal attempt | Released by abort protocol | Consumed | Either | `MUST_ABORT -> ABORTED` | Retained through A3 |
| Post-publication non-success | No retry | Consumed | Yes | Mandatory ABORT, or noncontinuable if storage ownership is uncertain | Retained through A3/noncontinuable ownership |

Attempt start/end is recoverable from §§9.6, 9.9, 15.7, 31.5, and 39.1:

- start: statement snapshot/attempt-local state established;
- clean end: internal retry discards attempt-local state without consuming CommandId;
- final end: statement succeeds or returns a final failure and consumes CommandId.

The phrase “CommandIds … are never reused” in §15.9 is consistent when “reuse” means assignment to another statement. A clean internal retry retains the same assignment; it does not consume and reassign it.

### CommandId matrix

| Case | Current C | Next C | Reused? | Consumed? | State |
|---|---:|---:|---:|---:|---|
| First statement | `0` | `1` after completion | No | On final completion | `ACTIVE` or terminal path |
| Successful statement | C | checked C+1 | No | Yes | `ACTIVE` |
| Clean internal retry | C | unchanged | Same assignment retained | No | `ACTIVE` |
| Recoverable failure | C | checked C+1 | No | Yes | `ACTIVE` |
| MUST_ABORT failure | C | No ordinary next statement | No | Yes | `MUST_ABORT -> ABORTED` |
| `UINT32_MAX` statement | max | none | No | Yes | Existing work may COMMIT/ROLLBACK |
| Admission after max | none | none | No | No | `COMMAND_ID_EXHAUSTED` |

No CommandId ambiguity requires a semantic decision.

## Lock and protection ownership

| Protection | Protects | Acquisition/lifetime | May block? | Crash persistence | Substitute? |
|---|---|---|---:|---:|---|
| `TableWriterGate` | Stable table/index manifest against DDL | Before tuple/key locks; through C5/A3 | Yes | No | Not a tuple/key lock |
| `TUPLE_WRITE` | Exact writable physical `(TableId,RID)` and RID retention | Request registered under epoch; through C5/A3 | Yes | No | Not visibility or page protection |
| `UNIQUE_KEY` | Current-state ownership admission for one full key | Sorted acquisition; through C5/A3 | Yes | No | Not B+ latching |
| Page latch | Page bytes/structure | Short physical operation | Yes, short | No | Not transaction protection |
| BufferPool pin | Frame binding/residency | Guard lifetime | Resource wait/failure | No | Not RID retention |
| Read epoch | Retained physical RID identity | Discovery through live TUPLE_WRITE registration | No transaction wait | No | Not visibility |
| Write-status dependency | Future publication of transaction’s own TxnId | Registration through C5/A3 | No DML lock wait | No | Not snapshot/status guard |

The writer handoff is gap-free:

```text
candidate under read epoch
-> release page/index latches
-> register queued or granted TUPLE_WRITE claim
-> release epoch
-> block if necessary
-> grant
-> fresh pin/latch and full revalidation
```

No blocking transaction wait retains a page latch or read epoch. TUPLE_WRITE and UNIQUE_KEY ownership remains terminal-duration, including through `MUST_ABORT`.

## Current-owner matrix

| Current `xmax` state | READ COMMITTED | REPEATABLE READ | Wait/retry | Write allowed? |
|---|---|---|---|---:|
| Invalid/no deleter | Proceed after revalidation | Proceed | No | Yes |
| SELF | Apply exact cmin/cmax and operation-context rules | Same | No generic retry | Only if exact SELF rules authorize |
| Other IN_PROGRESS | Release physical protection, wait, re-fetch/revalidate | Same | Wait/recheck | Not before terminal result |
| Other COMMITTED | Fresh-snapshot retry if still pre-write; otherwise abort | Serialization failure and abort | RC only, pre-write | No stale-target write |
| Other ABORTED | Revalidate; aborted mark is ineffective | Same | No | Yes |
| RETIRED | Invariant/corruption path | Same | No guessing | No |
| INVALID/RESERVED status | Invariant/corruption path | Same | No guessing | No |
| Lookup/I/O/format failure | Propagate exact lower-layer result | Same | Only where lower layer permits | No guessed outcome |

## Uniqueness matrix

| Candidate | Conflict? | Wait? | Heap recheck? | Result |
|---|---:|---:|---:|---|
| Committed/frozen live creator | Yes | No | Yes | `UniqueViolation` |
| Other creator nonterminal | Undecided | Yes | Yes, complete rescan | Terminal result decides |
| Aborted creator | No | No | Yes | Ignore physical garbage |
| Effective creator, committed deleter | No | No | Yes | Key is currently free |
| Effective creator, aborted deleter | Yes | No | Yes | Delete ineffective |
| Effective creator, nonterminal deleter | Undecided | Yes | Yes, complete rescan | Commit frees; abort preserves |
| Exact UPDATE old RID | No | No | Yes | `SELF_EXCLUDED` |
| Same transaction, another live row | Yes | No | Yes | `UniqueViolation` |
| Another row deleted in current command | Yes | No | Yes | Immediate constraint remains |
| Row self-deleted in earlier command | No | No | Yes | Key may be reused |
| NULL-containing ordinary UNIQUE key | No | No lock | Physical entry still installed | `NO_CONFLICT` |
| PRIMARY KEY NULL | Rejected before unique admission | No | No unique scan | NOT NULL/PK violation |
| Dangling, reused, malformed, mismatched candidate | Not a duplicate decision | No | Mandatory | Corruption/invariant error |

The exact-key scan:

- starts from `(K, MIN_RID)`;
- scans the complete equal-key run, including across leaves;
- heap-rechecks every distinct physical candidate;
- does not stop early after finding a conflict;
- uses current transaction state, not the caller snapshot;
- gives corruption precedence over a valid conflict;
- repeats completely after every wait.

# DML protocols

## INSERT

| Stage | May wait? | Physical protection | Failure result |
|---|---:|---|---|
| Input/type/NOT NULL/PK validation | No | None | FA before publication |
| Shared writer gate | Yes | No page latch | Structured gate failure/deadlock policy |
| Sorted UNIQUE_KEY acquisition | Yes | No page latch/epoch | Constraint/deadlock result |
| Complete current-state exact-key scan | May wait for status | Fresh read epochs per scan | Retry/recheck; no publication |
| Heap page/RID allocation | Resource failure possible | Heap pin/latch | FA until a logical row publishes |
| Heap WAL and tuple publication | No logical wait | Heap mutation protocol | Post-authorizing publication must complete or database becomes noncontinuable |
| B+ entry MTRs for every index | Short physical latch waits | B+ MTR ownership | Later known failure after prior logical publication means MA |
| `RETURNING` spool and completion | No external exposure yet | Query-owned spool | Failure after publication means MA |
| Final result | No | None | Explicit success; autocommit proceeds to COMMIT |

RID allocation uses the advisory FSM but actual heap geometry is authoritative. A legal `UNUSED` slot or newly published slot supplies the RID; a `DEAD` slot cannot be reused early.

The heap redo record precedes every index MTR that references the RID. Heap plus indexes are not one cross-file physical atomic write; transaction status, terminal key locks, per-page WAL/MTR atomicity, and mandatory abort after partial publication provide logical atomicity.

### INSERT tuple header

Defined:

```text
xmin = current TxnId
xmax = INVALID_TXN_ID
cmin = current CommandId
```

Also derivable from existing owners:

- no predecessor uses §5.7.4’s invalid RID pair;
- schema version comes from the resolved v1 schema;
- tuple flags/null/body follow the Chapter-5 codec.

Not defined: the canonical persisted `cmax` value while `xmax` is invalid. This is frozen question F15-Q1.

### INSERT failures

- Duplicate/constraint error before first publication: failed statement may leave explicit transaction `ACTIVE`; CommandId is consumed.
- After heap/index logical publication: same-TxnId retry is forbidden; transaction automatically aborts.
- A known later index failure leaves aborted heap/index garbage.
- Failure after a publication-authorizing WAL append but before required page publication must complete/retry that primitive or enter `DATABASE_NONCONTINUABLE`.
- Recovery replays physical heap/index WAL; it does not rerun INSERT or uniqueness.

## UPDATE

```text
finalize unique target spool
-> candidate RID under read epoch
-> live TUPLE_WRITE request
-> epoch release
-> grant and fresh revalidation
-> conflict classification
-> derive old/new unique keys
-> sorted UNIQUE_KEY acquisition
-> second target revalidation
-> current-state uniqueness scan
-> publish new version
-> publish old xmax/cmax
-> insert new RID into every index
-> buffer result
```

New version:

```text
xmin = current TxnId
cmin = current CommandId
prev = exact old RID
new physical RID
```

The new version logically starts with no deleter, so `xmax` uses the Chapter-5 invalid sentinel. The generic canonical `cmax` bytes remain unspecified, as with INSERT.

Old version:

```text
xmax = current TxnId
cmax = current CommandId
```

Even when indexed values are unchanged, a new physical `(key,new RID)` entry is created and the old `(key,old RID)` remains until vacuum. Changed keys create the appropriate new entries; old entries remain.

For a retaining unique key, only the exact revalidated old RID is excluded. Other equal owners—including another row from the same transaction or current command—still conflict.

### UPDATE failure/crash composition

- Failure after only the new version publishes: automatic abort makes it invisible; old row remains effective.
- Failure after old `xmax` also publishes: automatic abort makes that `xmax` ineffective and the new creator invisible.
- Failure after one or more index entries: all current-statement physical effects remain aborted garbage.
- COMMIT cannot begin until statement success, so no partial UPDATE prefix can commit.
- Recovery repeats physical WAL only; no SQL UPDATE replay or lock replay occurs.

## DELETE

```text
finalize unique target spool
-> candidate under read epoch
-> live TUPLE_WRITE request
-> epoch release
-> grant and revalidation
-> current-owner conflict rules
-> old-key UNIQUE_KEY acquisition
-> second revalidation
-> publish xmax/cmax
-> retain all physical index entries
```

The retained old-key lock prevents another transaction from treating the key as free before terminal publication.

- Commit makes the version deleted to qualifying newer snapshots; physical removal remains vacuum-owned.
- Abort makes the `xmax` ineffective.
- A failure after `xmax/cmax` publication mandates automatic abort.
- DELETE never synchronously removes ordinary index entries.

## Multirow and statement atomicity

| Operation | One CommandId/snapshot attempt? | Target/input handling | Failure after row N | Duplicate target |
|---|---:|---|---|---|
| Multirow INSERT | Yes | Sequential or batch with pending-key set | If prior row published: mandatory transaction abort | Duplicate unique input conflicts |
| Multirow UPDATE | Yes | Finalized target spool | Partial physical versions become aborted garbage | Target RID deduplicated by §31.2 |
| Multirow DELETE | Yes | Finalized target spool | Published xmax values become ineffective on abort | Target RID deduplicated by §31.2 |

V1 has no statement-level physical undo, savepoints, or subtransactions. Therefore:

- a pre-write recoverable failure may leave an explicit transaction active;
- any non-success after this statement’s first published logical mutation aborts the whole transaction;
- earlier successful statements in that transaction also become aborted because the transaction cannot commit;
- a later statement never observes partial effects from the failed DML because no later ordinary statement is admitted.

The architecture defines duplicate-target behavior through §31.2: one physical target RID appears at most once in the finalized UPDATE/DELETE target spool.

## Result publication

`RETURNING` is clear:

- all rows are buffered;
- no failed or internally retried attempt exposes a prefix;
- explicit-transaction output may be consumed after statement success but before COMMIT;
- autocommit output remains hidden through COMMIT C4–C5;
- a transport failure after commit cannot undo COMMITTED.

Statement success is not transaction COMMIT. Dirty pages and WAL-authorized mutations likewise do not imply COMMIT; MVCC status remains authoritative.

Affected-row count is not defined. The architecture does not state whether counting linearizes at target-spool membership, successful per-row publication, final-attempt completion, or another point. This is frozen question F15-Q2.

# Failure and error matrices

## Failure-authorization matrix

| Failure point | Internal statement retry? | Transaction state | Physical rollback? | Completion/recovery |
|---|---:|---|---:|---|
| Before any logical publication | RC conflict only, optionally | `ACTIVE`/FA unless independently fatal | Exact provisional primitive restoration only | Attempt may restart or return failure |
| After heap logical publication | No | MA -> automatic ABORT | No user-DML undo | Garbage remains/recovery replays |
| After first index MTR | No | MA -> automatic ABORT | No user-DML undo | Physical entry may remain |
| After several mutations | No | MA -> automatic ABORT | No | Status makes entire transaction aborted |
| Known pre-append failure with exact restoration | Only through normal statement policy | FA if no earlier current-statement write; otherwise MA | Exact local restoration | No published effect from failed primitive |
| Post-authorizing append publication failure | No | `DATABASE_NONCONTINUABLE` if completion cannot be established | No rollback-and-continue | Controlled stop/recovery |
| Known WAL write/durability failure | No post-write retry | FA or MA by current-statement boundary | Dirty state retained | Lower-layer retry or structured error |
| Append/ownership uncertainty | No | `DATABASE_NONCONTINUABLE` | No guessing | Recovery from valid prefix |
| Corruption | No | `DATABASE_NONCONTINUABLE`/canonical corruption path | No | Repair/recovery owner |
| Deadlock victim | No | MA regardless of write boundary | No | Automatic ABORT |
| RR serialization failure | No | MA | No | Automatic ABORT |
| Unique/NOT NULL/type error | No internal retry | FA before boundary; MA after | No statement undo | Exact original diagnostic preserved |
| Resource exhaustion | Only if owning operation explicitly permits | FA before; MA after | Exact provisional restoration only | No unsafe fallback |

## Error/result summary

| Condition | Canonical result |
|---|---|
| Unique owner found | `UniqueViolation`, then §39 boundary classification |
| NOT NULL/PK NULL | Constraint error, normally pre-write |
| Expression/cast/overflow | FA before first write; MA afterward |
| Deadlock | `DEADLOCK_DETECTED`, mandatory abort |
| RR write conflict | Serialization/write-conflict, mandatory abort |
| CommandId exhausted | Reject next ordinary statement; COMMIT/ROLLBACK existing work remains legal |
| TxnId exhausted | New transaction admission fails; no wrap |
| PageNo/FileId/SlotId exhausted | Owning checked-domain error; no wrap/rebinding |
| WAL position exhausted | Mutation cannot authorize; terminal credit protects already-admitted writer closure |
| `NO_REPLACEABLE_FRAME` | Resource failure; classification uses statement boundary |
| FSM stale high | Heap recheck rejects candidate |
| FSM stale low | May miss space/search or extend; correctness unaffected |
| Heap/index corruption | Canonical corruption, never skipped/treated as duplicate |
| Status lookup failure | Exact lower-layer result; no outcome guessing |
| Cancellation | FA before write, MA afterward, unless another fatal cause dominates |
| Shutdown | No new statement after DRAINING; existing transaction follows normal terminal protocol |
| Storage uncertainty | `DATABASE_NONCONTINUABLE` |

# Update version-state matrix

| State | Old version | New version | Visible result | Index behavior |
|---|---|---|---|---|
| Before UPDATE | Live/no updating xmax | Absent | Old | Old entries |
| UPDATE in progress | Own/in-progress xmax | Own/in-progress creator | Other transactions use snapshot rules; operation context owns progress | Old plus published new candidates |
| Updating transaction aborts | Aborted xmax ineffective | Aborted creator invisible | Old remains logical row | New entries become garbage |
| Updating transaction commits | Committed xmax | Committed creator | Snapshot-dependent old/new choice | Both physical generations may remain |
| Old RR snapshot | May continue seeing old | Too new/in active | Old | Heap recheck chooses old |
| New qualifying snapshot | Old deleted | New committed | New | New candidate resolves |
| Same transaction, later command | Old self-deleted with earlier cmax | New self-created with earlier cmin | New | Current-command rules apply |

# Retry and retained-lock matrix

| Retry case | Existing locks | New requests | Snapshot | Result |
|---|---|---|---|---|
| Clean RC retry | Retained | May add locks for newly found RIDs/keys | Fresh | Same CommandId |
| Prior UNIQUE_KEY retained | Remains held | Fresh exact-key scan still required | Fresh RC snapshot | Old scan cannot authorize |
| Prior TUPLE_WRITE retained | Old RID remains protected | Retry may acquire a different target lock | Fresh RC snapshot | Unified graph handles cycles |
| Continuous conflicts | No fixed retry count | Implementation may retry, return FA, or honor cancellation | Fresh per attempt | Never retry after publication |
| Deadlock/cancellation after publication | Retained until A3 | No ordinary acquisition after MUST_ABORT | No new statement | Automatic abort |

# Cross-chapter consistency

| Owner | Chapter-15 result |
|---|---|
| Chapter 3 READY/shutdown | CONSISTENT |
| Chapter 4 identity/exhaustion/errors | CONSISTENT |
| Chapter 5 tuple/RID/versioning | **FINDING:** fresh-version `cmax`; INSERT latch-release list conflicts with §5.14 |
| Chapter 6 FSM | CONSISTENT |
| Chapter 7 pins/latches/publication | CONSISTENT, except §15.2 wording could imply overlong heap-latch lifetime |
| Chapter 8 index candidates/MTR | CONSISTENT |
| Chapter 9 transaction/CommandId/snapshots | CONSISTENT BUT SPECIALIZED |
| Chapter 10 MVCC/no physical undo | CONSISTENT |
| Chapter 11 locks/current owner/uniqueness | CONSISTENT BUT SPECIALIZED |
| Chapter 12 WAL authorization | CONSISTENT BUT SPECIALIZED |
| Chapter 13 recovery/no SQL replay | CONSISTENT |
| Chapter 14 status dependency/read epoch/RID reuse | CONSISTENT |
| §39 failure semantics | CONSISTENT BUT SPECIALIZED |
| §41 verification | Existing substantial ownership; follow-up gaps remain |

Chapter-14 compatibility is clean:

- the write-status dependency remains held through DML publication and C5/A3;
- the epoch-to-TUPLE_WRITE handoff is gap-free;
- queued/granted requests retain old RID identity;
- no stale waiter can survive same-RID reuse;
- DEAD/UNUSED and index/predecessor barriers are unchanged.

# Explicit cross-references

| Chapter-15 source | Target | Purpose | Result |
|---|---|---|---|
| §15.1.1 | §21.2.1 | Writer gate | Exists; correct owner |
| §15.1.1 | §11.13 | Retained-gate graph/order | Exists; precise |
| §15.1.1 | §9.4 | Write-status dependency | Exists; precise |
| §15.2–15.4 | §§11.9–11.10 | Key order/current ownership | Exists; precise |
| §15.4 | Chapter 14 | Full reclamation barrier set | Exists; broad but appropriate |
| §15.5 | §21.5 | Durable final names | Exists; precise |
| §15.5–15.6 | §4.3.2.4 | Terminal WAL credit | Exists; precise |
| §15.5–15.6 | §12.10.5 | Terminal status-page mutation | Exists; precise |
| §15.5–15.6 | §9.14 | Runtime terminal publication | Exists; precise |
| §15.5 | §13.13.2 | Terminal status redo | Exists; precise |
| §15.5 | §39.1.5 | COMMIT failures | Exists; precise |
| §15.6 | §12.17 | Abort-status WAL-before-data | Exists; precise |
| §15.6 | §39.1.6 | ABORT failures | Exists; precise |
| §15.7 | §39.1.2 | First-published-write boundary | Exists; precise |
| §15.7.2 | §15.6 | Automatic abort | Exists; precise |
| §15.7.3 | §31.9 | `RETURNING` publication | Exists; precise |

No broken cross-reference was found.

# Documentation-model assessment

## Temporality

| Evidence | Classification | Result |
|---|---|---|
| “later transaction-gate wait” | Runtime ordering | Valid |
| “later unrelated DDL” | Transaction/statement history | Valid |
| “vacuum removes the garbage later” | Runtime maintenance ordering | Valid |
| “no later runtime…may change” | Runtime/durability ordering | Valid |
| “later ROLLBACK” | Transaction history | Valid |
| “A future explicit opt-in retry policy…” | Project/evolution roadmap | Finding |
| “Future statement savepoints/subtransactions could relax…” | Project/evolution roadmap | Finding |
| `current` transaction/statement/owner uses | Runtime/transaction meaning | Valid |

No current implementation status, Phase-2 narration, dates, milestones, test results, or history appears in Chapter 15.

## Terminology

| Term | Canonical meaning | Assessment |
|---|---|---|
| SQL statement | One admitted command and CommandId | Clear |
| Statement attempt | One execution under one stable attempt snapshot | Clear across §§9.9, 15.7, 39.1 |
| Clean retry | RC fresh-snapshot retry before publication | Clear |
| CommandId | Statement-level identity retained across attempts | Clear |
| Candidate RID | Snapshot-derived physical candidate under epoch | Clear |
| Target RID | Revalidated physical version under TUPLE_WRITE | Clear |
| Current owner | §11.10 status/command-based ownership, not snapshot visibility | Clear |
| `SELF_EXCLUDED` | Exact current UPDATE old/replacement RID only | Clear |
| WAL authorization | Valid append of publication-authorizing record | Clear |
| Published mutation | §12.12 step-8 ordinary dirty state | Clear |
| “installed persistent write” | Used once in §15.9 instead of “published transaction-owned mutation” | Finding |
| Affected row | No canonical definition | Finding |

## Normative-language assessment

MUST-level requirements are correctly used for:

- gate order;
- status-dependency lifetime;
- no post-publication same-TxnId retry;
- no premature external output;
- no semantic redefinition by upper layers.

MAY is correctly used for:

- holding compatible shared gates across tables;
- optional clean RC retry;
- physical aborted garbage retention.

No SHOULD-level requirement weakens a correctness rule.

## Analytical depth

| Mechanism | Assessment |
|---|---|
| Statement vs attempt | Analytically sufficient through cross-references |
| CommandId/retry | Sufficient |
| Post-wait revalidation | Sufficient |
| Current-state uniqueness | Sufficient and well-rationalized |
| UPDATE old/new versions | Sufficient |
| First-published-write boundary | Sufficient |
| ACTIVE vs MUST_ABORT | Sufficient |
| No physical user-DML undo | Sufficient |
| RETURNING publication | Sufficient |
| Affected-row reporting | **Analytical/semantic finding** |
| Fresh tuple no-deleter header | **Semantic completeness finding** |

Chapter 15 is understandable without knowing whether DML, HeapFile, BufferPool, or LockManager currently exists. Its defects do not depend on repository implementation status.

# 120-item technical consistency matrix

Legend: `C` = consistent, `S` = consistent but owned/specialized elsewhere, `F` = finding, `N/A` = outside v1.

| # | Topic | Status |
|---:|---|:---:|
| 1 | Statement vs attempt | S |
| 2 | CommandId ownership | C |
| 3 | Clean retry boundary | C |
| 4 | Retry after write | C |
| 5 | RC fresh snapshot retry | C |
| 6 | RR serialization behavior | C |
| 7 | Snapshot registration | C |
| 8 | TableWriterGate order | C |
| 9 | TUPLE_WRITE handoff | C |
| 10 | No blocking under epoch | C |
| 11 | INSERT lock order | C |
| 12 | UPDATE lock order | C |
| 13 | DELETE lock order | C |
| 14 | UNIQUE_KEY sort | C |
| 15 | NULL UNIQUE | C |
| 16 | PK NULL | C |
| 17 | Current-state uniqueness | C |
| 18 | Complete equal-key scan | C |
| 19 | Committed creator conflict | C |
| 20 | In-progress creator | C |
| 21 | Aborted creator | C |
| 22 | Committed deleter | C |
| 23 | Aborted deleter | C |
| 24 | In-progress deleter | C |
| 25 | SELF old-target exclusion | C |
| 26 | Same-txn other-row conflict | C |
| 27 | Same-command other-row delete | C |
| 28 | Earlier self delete | C |
| 29 | Exact physical duplicate | C |
| 30 | INSERT RID allocation | S |
| 31 | INSERT xmin/cmin | C |
| 32 | Write-status dependency | C |
| 33 | INSERT heap/index ordering | **F** |
| 34 | INSERT pre-auth failure | C |
| 35 | INSERT post-auth failure | S |
| 36 | INSERT index failure | S |
| 37 | UniqueViolation state | S |
| 38 | UPDATE discovery | C |
| 39 | UPDATE handoff | C |
| 40 | UPDATE revalidation | C |
| 41 | Current-owner matrix | S |
| 42 | Old xmax/cmax | C |
| 43 | New xmin/cmin | C |
| 44 | New RID | C |
| 45 | prev_RID | C |
| 46 | Update old/new atomicity | S |
| 47 | Unchanged indexed key | C |
| 48 | Changed indexed key | C |
| 49 | Unchanged unique key | C |
| 50 | Changed unique key | C |
| 51 | Failure after old-xmax authorization | S |
| 52 | Failure after new-version authorization | S |
| 53 | Index-maintenance failure | S |
| 54 | DELETE discovery | C |
| 55 | DELETE revalidation | C |
| 56 | DELETE xmax/cmax | C |
| 57 | Delete index policy | C |
| 58 | Delete UNIQUE_KEY | C |
| 59 | Aborted delete | C |
| 60 | Committed delete | C |
| 61 | Delete post-auth failure | S |
| 62 | Multirow INSERT | S |
| 63 | Multirow UPDATE/DELETE | S |
| 64 | Statement atomicity | S |
| 65 | Failed-statement self visibility | C |
| 66 | ACTIVE/MUST_ABORT threshold | S |
| 67 | Recoverable failure | S |
| 68 | Deadlock | C |
| 69 | Serialization failure | C |
| 70 | NOT NULL | S |
| 71 | CHECK constraint | N/A |
| 72 | Foreign key | N/A |
| 73 | Expression error | S |
| 74 | Tuple size | C |
| 75 | CommandId exhaustion | C |
| 76 | Numeric exhaustion | S |
| 77 | FSM stale-high | C |
| 78 | FSM stale-low | C |
| 79 | No replaceable frame | S |
| 80 | WAL-position exhaustion | C |
| 81 | WAL ENOSPC | S |
| 82 | Page corruption | S |
| 83 | Index corruption | C |
| 84 | Status lookup failure | C |
| 85 | Row count | **F** |
| 86 | RETURNING | C |
| 87 | Statement success vs COMMIT | C |
| 88 | Dirty vs committed | C |
| 89 | WAL-authorized vs committed | C |
| 90 | Abort/no physical undo | C |
| 91 | Aborted INSERT index | C |
| 92 | Aborted UPDATE | C |
| 93 | Committed UPDATE | C |
| 94 | Unique INSERT race | C |
| 95 | Delete/insert key race | C |
| 96 | Update key-move race | C |
| 97 | Two-row swap | C |
| 98 | SELF command semantics | C |
| 99 | Multiple unique indexes | C |
| 100 | Nonunique indexes | C |
| 101 | Stable IndexId | C |
| 102 | Stable TableId | C |
| 103 | Schema version | C |
| 104 | DDL race | C |
| 105 | Index-set stability | C |
| 106 | Catalog lookup failure | C |
| 107 | Stale plan boundary | S |
| 108 | Exact 8135 boundary | C |
| 109 | Invalid RID sentinel | C |
| 110 | DEAD/UNUSED candidate | C |
| 111 | Terminal lock release | C |
| 112 | Retry locks | C |
| 113 | Retry UNIQUE_KEY | C |
| 114 | Retry TUPLE_WRITE | C |
| 115 | Cancellation | S |
| 116 | Shutdown | C |
| 117 | Noncontinuable failure | C |
| 118 | Duplicate target in statement | S |
| 119 | Same-row multi-update | S |
| 120 | Implementer invention required | **F** |

# Documentation-model matrix

| # | Question | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | Runtime temporal language preserved | CONSISTENT |
| 3 | No implementation status | CONSISTENT |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No DEVELOPMENT sequencing | CONSISTENT |
| 6 | No VERIFICATION procedure leakage | CONSISTENT |
| 7 | No PROJECT_STATE leakage | CONSISTENT |
| 8 | No devlog/history | CONSISTENT |
| 9 | No source-layout coupling | CONSISTENT |
| 10 | Statement/attempt terminology | CONSISTENT |
| 11 | Retry terminology | CONSISTENT |
| 12 | Current-owner terminology | CONSISTENT |
| 13 | Uniqueness terminology | CONSISTENT |
| 14 | Authorization terminology | FINDING |
| 15 | Failure/state terminology | CONSISTENT |
| 16 | Revalidation rationale | CONSISTENT |
| 17 | Current-state uniqueness rationale | CONSISTENT |
| 18 | Retry-boundary rationale | CONSISTENT |
| 19 | §§15.7.2–15.7.3 timelessness | FINDING |
| 20 | Readable without implementation-status knowledge | CONSISTENT |

# Complete findings

## F15-1 — Fresh-version `cmax` is not canonical

- Section: §§15.2–15.3
- Evidence: INSERT lists `xmin`, `xmax`, and `cmin`; UPDATE’s new version lists `xmin`, `cmin`, and `prev`, but neither defines fresh no-deleter `cmax`.
- Severity: **MAJOR**
- Type: **SEMANTIC COMPLETENESS**
- Scope: Cross-section/persistent-format integration
- Arithmetic: N/A
- Canonical comparison: §5.7 defines a persisted 48-byte header; §5.7.2 defines `xmax=0` as no deleter. Bootstrap happens to use `cmax=0`, but it is not the generic user-DML authority.
- Consequence: conforming writers can produce different persistent bytes, and verification cannot derive one canonical fresh tuple header without choosing a value.
- Correct owner: Architecture, Chapter 5/15 integration.
- Future action: **U. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED**

Frozen question:

> What exact `cmax` value MUST a newly created normal tuple persist while `xmax = INVALID_TXN_ID`?

## F15-2 — Affected-row semantics have no canonical owner

- Section: §§15.2–15.4, §15.7.3, §15.9; cross-reference §20.17.10
- Evidence: §20.17.10 requires no-op lowering to preserve “affected-row reporting,” but Chapter 15 defines only physical row operations and `RETURNING`, not when an affected-row count increments or publishes.
- Severity: **MAJOR**
- Type: **RESULT PUBLICATION**
- Scope: Cross-section
- Arithmetic: N/A
- Consequence: implementations may count initial candidates, finalized targets, successful physical row mutations, or rows from abandoned RC attempts differently. Retries could double count, and failure/result behavior is not deterministic.
- Correct owner: Architecture, Chapter 15 with result-interface integration.
- Future action: **U. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED**

Frozen question:

> What exact successful-attempt event contributes one affected row for INSERT, UPDATE, and DELETE, and how are abandoned retries, failed multirow statements, no-op assignments, and stale-target revalidation reflected?

## F15-3 — INSERT latch-release order conflicts with the heap owner

- Section: §15.2
- Evidence: step 8 performs all B+ MTRs and step 9 then says to “release short-lived heap/B+ page latches/pins.”
- Severity: **MAJOR**
- Type: **LOCK ORDER**
- Scope: Cross-section
- Canonical comparison: §5.14 releases the heap latch/page guard before index creation; §15.1 says owning subsystem latch contracts remain authoritative.
- Consequence: literal implementation could retain a heap latch while entering B+ MTR latching, creating avoidable cross-subsystem latch coupling and a different order from Chapter 5.
- Correct owner: Architecture, Chapter 15 integration wording; Chapters 5, 7, and 8 remain semantic owners.
- Future action: **A. LOCAL WORDING FIX**

The architectural decision is already recoverable from Chapter 5; this does not require a new semantic choice.

## F15-4 — Whole-request retry policy is roadmap-shaped

- Section: §15.7.2
- Evidence: “A future explicit opt-in retry policy may do the same…”
- Severity: **MINOR**
- Type: **TEMPORALITY**
- Scope: Local
- Consequence: the same invariant can be expressed timelessly without predicting a future policy.
- Correct owner: Architecture should state that any whole-request retry uses a new transaction identity; implementation sequencing belongs in DEVELOPMENT.
- Future action: **B. TIMELESSNESS REWRITE**

## F15-5 — Savepoint wording is roadmap-shaped

- Section: §15.7.3
- Evidence: “Future statement savepoints/subtransactions could relax this v1 restriction…”
- Severity: **MINOR**
- Type: **TEMPORALITY**
- Scope: Local
- Consequence: current v1 scope is clear, but the sentence narrates a possible future feature.
- Correct owner: Architecture may timelessly state that savepoints/subtransactions are outside v1 and that the rule applies while they are absent.
- Future action: **B. TIMELESSNESS REWRITE**

## F15-6 — “Installed” drifts from the exact publication term

- Section: §15.9 invariant 7
- Evidence: “installed its first persistent write”
- Severity: **MINOR**
- Type: **TERMINOLOGY**
- Scope: Local/cross-reference
- Canonical comparison: §§12.12 and 39.1.2 distinguish reservation, provisional installation, valid append, and published mutation.
- Consequence: “installed” can be read as provisional resident-byte installation rather than the transaction-owned publication boundary.
- Correct owner: Architecture terminology.
- Future action: **H. TERMINOLOGY NORMALIZATION**

# Direct yes/no conclusions

| Question | Result |
|---|---|
| Statement-attempt ambiguity? | No |
| CommandId reuse/consumption ambiguity? | No |
| Retry after an authorized/published statement write? | No |
| RC retry with stale snapshot? | No |
| RR fresh-snapshot retry? | No |
| Transaction-lock order contradiction? | No |
| Read-epoch-to-TUPLE_WRITE gap? | No |
| Missing post-wait revalidation? | No |
| Current-owner ambiguity? | No |
| Uniqueness based on caller snapshot? | No |
| Incomplete exact-key scan? | No |
| `SELF_EXCLUDED` ambiguity? | No |
| INSERT heap/index partial-publication ambiguity? | No; §39 resolves transaction outcome |
| UPDATE old/new-version atomicity ambiguity? | No |
| DELETE index policy ambiguity? | No |
| Post-publication failure incorrectly leaves ACTIVE? | No |
| Failed statement’s partial effects may commit? | No |
| Multirow statement atomicity ambiguity? | No |
| Affected-row/result ambiguity? | **Yes—affected count only; RETURNING is clear** |
| Physical user-DML undo required? | No |
| Aborted version/index inconsistency? | No |
| Stale RID/reuse alias? | No |
| Write-status-dependency violation? | No |
| Implementer invention required? | **Yes—fresh `cmax` and affected-row semantics** |
| Project-time wording? | Yes, two localized phrases |
| DEVELOPMENT-owned roadmap material? | Yes, those two future-policy phrases |
| VERIFICATION procedure leakage? | No |
| PROJECT_STATE leakage? | No |
| Devlog/history leakage? | No |
| Ambiguous terminology? | Yes, localized “installed persistent write” |
| Analytically underexplained boundary? | Yes, affected rows and fresh `cmax` |
| Timeless canonical contract as written? | Not fully |

# §§15.7.2–15.7.3 dedicated assessment

## §15.7.2 — “Conflict after a persistent statement write”

- Architectural purpose: forbid same-TxnId retry after the first published statement mutation.
- Runtime ordering: clear.
- Transaction consequence: exact `ACTIVE -> MUST_ABORT -> ABORTED`.
- Autocommit: no transparent whole-transaction rerun.
- Explicit transaction: terminally aborted.
- CommandId/TxnId consistency: correct.
- Analytical rationale: sufficient.
- Document issue: “A future explicit opt-in retry policy…” is roadmap-shaped.
- Semantic status: clean.
- Overall: **FINDING**, document-only.

## §15.7.3 — “External output”

- Architectural purpose: prevent irreversible output from an attempt that may retry/fail.
- `RETURNING`: correctly delegated to §31.9.
- Explicit transaction vs autocommit: exact.
- Verification leakage: none.
- Analytical rationale: sufficient.
- Document issue: “Future statement savepoints/subtransactions…” is roadmap-shaped.
- Semantic status: clean.
- Overall: **FINDING**, document-only.

# Verification cross-check

Existing verification already owns:

- CommandId and RC retry;
- current-owner RC/RR matrices;
- TUPLE_WRITE handoff and terminal claims;
- complete uniqueness/current-state races;
- self-exclusion and same-command collisions;
- partial DML before/after publication;
- mandatory abort/no physical undo;
- target-spool deduplication/Halloween protection;
- `RETURNING` buffering and autocommit COMMIT publication;
- WAL/MTR and recovery prefixes.

Follow-up verification gaps:

1. **Blocked by F15-Q1:** exact fresh INSERT/UPDATE tuple-header byte oracle, especially `cmax`.
2. **Blocked by F15-Q2:** affected-row count across clean retries, failed multirow attempts, zero-target statements, and autocommit result publication.
3. Explicit end-to-end DML prefix matrix at every heap/new-version/old-header/per-index publication boundary; current coverage is broad but not enumerated per prefix.
4. Direct assertion that INSERT releases its heap latch before entering B+ MTR latching once §15.2 wording is synchronized.
5. End-to-end assertions for unchanged-key UPDATE creating `(key,new RID)`, DELETE retaining entries, and all-index partial failure/reopen outcomes.

These are `FOLLOW-UP VERIFICATION GAP`s, not additional architecture findings.

# Out-of-scope observations

- §31.12 contains “may later expose progress/debug result chunks,” which is roadmap-shaped wording for a future Chapter-31 review.
- §32.1 contains “The first production executor may…,” which is implementation-stage wording for a future Chapter-32 review.
- §31.7, §31.7-related protected material, §31.7’s direct review, §31.7 edits, §31.7 verification, §31.7 chronology, §31.7 source coupling, §31.7 semantics, §31.7 tests, §31.7 status, §31.7 ownership, §31.7 implementation, §31.7 future action, §31.7 artifact creation, §31.7 staging, §31.7 commits, §31.7 builds, §31.7 benchmarks, §31.7 scaffolding, §31.7 Phase-2 work, §31.7 review decisions, §31.7 cleanup, §31.7 synchronization, and §31.7 revision were not performed.
- §31.7 itself, §31.7’s protected wording, §31.7 direct review, §31.7 review artifacts, §31.7 edits, §31.7 implementation, §31.7 verification, §31.7 history, §31.7 staging, §31.7 commits, §31.7 builds, §31.7 tests, §31.7 benchmarks, §31.7 scaffolding, and §31.7 Phase-2 work remain untouched.
- Appendix C was not consulted because Chapter 15 does not reference it.

# Recommended next action

**Frozen semantic review required** for:

1. fresh no-deleter `cmax`;
2. affected-row counting/publication.

After those decisions, perform one targeted Chapter-15 architecture integration that also:

- synchronizes INSERT latch-release wording with §5.14;
- removes the two roadmap phrases;
- normalizes “installed persistent write” to the exact publication term.

Then synchronize the affected verification procedures.

# Recommended Chapter-16 review scope

The next direct read-only scope should be actual Chapter 16, **“Catalog and Schema Metadata,”** from [§16.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:12366) through the line before Chapter 17.

Its actual principal areas are:

- role/dependency boundary;
- v1 namespace model;
- stable catalog identities;
- built-in TypeId registry;
- schema-v1 system relations and physical catalog indexes;
- bootstrap self-description and validation;
- immutable descriptors;
- historical schema interpretation;
- ColumnId versus position;
- catalog bootstrap;
- cache/descriptor lifetime;
- catalog invariants.

That review was not started here.

# Final repository checks

- Files modified by audit: **none**
- Initial status: clean
- Final status: clean
- Initial index: clean
- Final index: clean
- Initial HEAD: `a3baa64b964212beca8a9912acc5cb214c272508`
- Final HEAD: `a3baa64b964212beca8a9912acc5cb214c272508`
- `git diff --check`: passed
- Audit-created repository changes: **none**
- Review artifacts modified/staged: **none**
- Build/tests/benchmarks: **not run**
- Implementation work: **none**
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
