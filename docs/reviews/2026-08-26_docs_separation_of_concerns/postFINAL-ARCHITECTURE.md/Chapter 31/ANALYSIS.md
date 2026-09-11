# Chapter 31 Architecture Review

## 1. Verdict

**BLOCKED ON SEMANTIC QUESTIONS**

Chapter 31 is broadly coherent, but it cannot stand unchanged as the canonical v1 contract because two observable transaction/result questions remain unresolved:

- `N31-1` — ordinary multi-row DML error discovery versus the first-persistent-write boundary.
- `N31-2` — spilled `RETURNING` read failure after successful explicit-transaction statement completion.

A separate **CHAPTER-31 FROZEN SEMANTIC / DESIGN DECISION PACKAGE** is required before documentation fixes.

## 2. Finding counts

| Class | Count |
|---|---:|
| BLOCKING | 2 |
| MAJOR | 0 |
| MINOR | 1 |
| EDITORIAL | 0 |
| DESIGN-SCOPE QUESTION | 0 |
| FROZEN SEMANTIC QUESTION | 2 |

## 3. Initial repository state

```text
HEAD:
    e8ffc8435431aa5604c7dedb02f95d386d2126b3

commit:
    e8ffc84 synced VERIFICATION after chapter 30 ARCHITECTURE fix

tracked modifications:
    none

staged modifications:
    none

untracked paths:
    none
```

## 4. Final repository state

```text
HEAD:
    e8ffc8435431aa5604c7dedb02f95d386d2126b3

working tree:
    clean

index:
    clean

git diff --check:
    clean
```

## 5. Audit-created changes

```text
NONE
```

## 6. Exact live Chapter-31 boundary

[docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:22794):

```text
start:
    line 22794 — # 31. DML, DDL, VACUUM, and Result Interface

end:
    line 23133 — final Chapter-31 invariant

next chapter:
    line 23135 — # 32. Parallel Execution and Scheduling
```

## 7. Complete subsection matrix

| Section | Live responsibility | Assessment |
|---|---|---|
| 31.1 | UPDATE/DELETE target materialization | Coherent |
| 31.2 | One physical target once | Coherent |
| 31.3 | Halloween protection | Coherent |
| 31.4 | Target-spool memory and spill | Coherent |
| 31.5 | Revalidation and READ COMMITTED retry | Coherent |
| 31.6 | Physical INSERT execution | Blocked by N31-1 composition |
| 31.7 | Physical UPDATE execution | Blocked by N31-1; minor chronology |
| 31.8 | Physical DELETE execution | Coherent |
| 31.9 | RETURNING spool/publication | Blocked by N31-2 |
| 31.10 | Query result cursor and lifetime | Coherent except N31-2 handoff |
| 31.11 | Physical DDL | Coherent |
| 31.12 | Physical VACUUM | Coherent; minor future-result wording |
| 31.12.1 | Physical ANALYZE | Coherent; minor status-style wording |
| 31.13 | DML/result invariants | Coherent but does not resolve N31-1/N31-2 |

## 8. Context sections inspected

The audit inspected the front matter and relevant contracts in Chapters:

```text
3, 4, 5, 8, 9, 11, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24,
25, 26, 27, 29, 30, 31, 32, 34, 37, 39, and 41.
```

Read-only role/coverage checks were also made in:

```text
docs/DEVELOPMENT.md
docs/PROJECT_STATE.md
docs/VERIFICATION.md
```

No historical review artifact was read.

## 9. Canonical owner matrix

| Contract | Canonical owner | Chapter-31 role |
|---|---|---|
| UPDATE/DELETE SQL target semantics | Chs. 18–21 | Physical realization |
| Physical target identity | Chs. 4–5, 11, 14 | Retain exact RID |
| Target deduplication | §§31.1–31.2 | Direct owner |
| Halloween protection | §§31.1–31.3 | Direct owner |
| Statement snapshot | Ch. 9 | Consume |
| Target revalidation | Chs. 11, 15, 21 | Invoke |
| Target write lock | Ch. 11 | Invoke |
| RID reuse safety | Chs. 11, 14 | Compose |
| Assignment row image | §21.13 | Execute |
| RETURNING image/bag | §21.15 | Buffer and publish |
| Affected-row count | §15.1.2 | Integrate into result envelope |
| Ordinary DML error order | §21.16.1 | Preserve; N31-1 |
| First persistent write | §39.1.2 | Preserve; N31-1 |
| Immediate uniqueness | §§11.9–11.10 | Invoke |
| Write publication order | §§12.12, 15.2–15.4 | Delegate |
| Query memory/spill | Ch. 24 | Account spools |
| Pipeline readiness | Ch. 26 | Compose |
| Result publication/lifetime | §§31.9–31.10 | Direct owner |
| Explicit/autocommit outcomes | Chs. 9, 15, 39 | Expose correctly |
| DDL publication | Ch. 21 | Control-operator delegation |
| VACUUM correctness | Ch. 14 | Control-operator delegation |
| ANALYZE collection/publication | Chs. 14, 21, 34 | Control-operator delegation |
| Cancellation/retry | Chs. 15, 24, 26, 39 | Compose |
| Parallel capability | Ch. 32 | Restrict DML/control roles |

No duplicate semantic owner was found outside N31-1/N31-2.

## 10. DML/result/control operator inventory

```text
PhysicalInsert
PhysicalUpdate
PhysicalDelete
PhysicalResultSink
PhysicalCreateTable
PhysicalCreateIndex
PhysicalDrop
PhysicalVacuum
PhysicalAnalyze
```

`PhysicalLimit`, DISTINCT, and Sort are reused only as supporting mechanisms; they do not own DML target semantics.

## 11. Target-spool lifecycle

The lifecycle is complete:

```text
read target relation
evaluate target qualification
materialize exact RID and required old values
deduplicate RID
Finalize spool
begin write phase
lock/re-fetch/revalidate
mutate
```

No target mutation is permitted before successful spool finalization.

## 12. “Supported target-producing joins”

This does not authorize `UPDATE ... FROM` or `DELETE ... USING`; Chapter 18 explicitly excludes both.

The phrase can validly describe architecture-authorized, semantics-preserving internal rewrites of supported uncorrelated subqueries, including semi/anti/marker-join forms under Chapter 20. Such rewrites cannot expand the SQL surface.

The wording is locally broad but causes no competing conforming SQL implementation because the grammar and logical-plan registries remain closed.

## 13. RID uniqueness/deduplication

The finalized spool is keyed by exact physical RID identity.

It must not use:

```text
payload equality
hash equality alone
TxnId identity
version-chain family identity
SQL DISTINCT representative semantics.
```

A hash implementation must resolve collisions using exact RID equality. One RID contributes at most one mutation, affected-row occurrence, and `RETURNING` occurrence.

## 14. Halloween protection

The materialization barrier prevents rediscovery caused by:

```text
updated index keys
new replacement versions
changed predicate values
current-command physical candidates.
```

Current-command MVCC remains supplementary rather than the sole defense.

## 15. RID lifetime and reuse protection

A spooled target cannot legally be reclaimed and rebound before write revalidation.

The owner chain is:

```text
active statement/transaction snapshot
    prevents reclamation of versions still needed by that snapshot

short discovery ReadEpochGuard
    protects physical RID dereference during candidate acquisition

live TUPLE_WRITE request
    becomes the exact RID-retention claim before release into a blocking wait

post-grant re-fetch/revalidation
    rejects stale, DEAD, UNUSED, malformed, or mismatched identity.
```

A writer may reacquire a short read epoch when taking a spooled RID into its write protocol. The active snapshot prevents the old version from becoming reclaimable in the intervening spool lifetime. No separate “spool is a retention claim” rule is needed.

## 16. Target-spool payload and RequiredSlotSet

The spool retains all values required for:

```text
assignment expressions
complete replacement tuple construction
affected unique keys
RETURNING
required slot/provenance identity.
```

Irrelevant columns may be pruned. Required hidden RID and old-value slots may not be pruned.

## 17. Spooled image versus revalidated image

The semantic authority is the exact revalidated physical target version.

Spooled values may be used only after proving they belong to that same version and remain exact. They cannot be applied to a replacement or rebound RID merely because the numeric RID matches.

`SET` and UPDATE `RETURNING` use the complete old/new image defined by Chapter 21, not an unconfirmed stale spool image.

## 18. Target revalidation

After `TUPLE_WRITE` acquisition or wake, execution must re-fetch and revalidate:

```text
RID identity
slot/version state
creator/deleter state
visibility
predicate/target eligibility
current object/descriptor state
applicable isolation outcome.
```

Chapter 31 introduces no new skip, retry, or conflict category.

## 19. READ COMMITTED retry

A legal same-`TxnId` retry requires:

```text
current_statement_has_published_write == false
```

It discards:

```text
target spool
dedup state
old-value state
RETURNING spool
provisional affected count
ordinary diagnostic candidates
execution cursors/scratch
attempt snapshot and attempt-local spill state.
```

The replacement attempt uses a fresh statement snapshot and the same logical statement `CommandId`.

No same-`TxnId` retry is legal after publication.

## 20. Target-spool memory and spill

Both in-memory and spilled forms must preserve exact:

```text
RID
NULL state
old typed values
VARCHAR bytes
LogicalSlotIds/provenance
deduplication state.
```

Spill ordering is nonsemantic and spill files are attempt-local.

## 21. Target-spool ordering

Target order is not SQL-visible.

These may vary without changing semantics:

```text
scan order
RID order
hash order
sort order
spill partition order
mutation order.
```

They cannot change affected count, `RETURNING` bag, selected ordinary error, or uniqueness semantics.

## 22. Affected-row count

For the final successful attempt:

| Operation | Count |
|---|---|
| INSERT | Successful logical input occurrences |
| UPDATE | Distinct finalized targets actually acted upon |
| DELETE | Distinct finalized targets actually acted upon |

It excludes discovery duplicates, stale skipped targets, abandoned attempts, physical versions, index entries, and WAL records.

The count belongs to the same successful statement-result envelope as `RETURNING`. Its wire/API representation is intentionally not fixed.

## 23. Ordinary DML error precedence

The public ordinary error is selected by §21.16.1:

```text
SourceSpan start
shorter/more-specific span
row-semantic phase
existing within-phase owner.
```

RID, row number, spool order, batch order, worker order, and physical visitation do not select among candidates.

However, the architecture does not coherently determine when a valid earlier row may publish relative to an ordinary candidate in a later row. That changes the §39 transaction consequence and creates N31-1.

## 24. Can physical visitation publish before all required higher-priority candidates are ruled out?

**Not coherently defined.**

Section 21.16.1 and invariant 21.20(29) prohibit physical visitation from changing the ordinary error’s transaction consequence. But §39.1.4 explicitly requires a valid scenario where rows 1–4 publish and row 5 then fails conversion/arithmetic/constraint evaluation.

A prevalidating implementation would discover the same row-5 error before any write and return `FA`; a row-at-a-time implementation returns `MA`. Both execution shapes are otherwise contemplated.

## 25. INSERT execution

Typed logical occurrences are preserved one-for-one. Conversions, NOT NULL, unique locking/checking, heap publication, index publication, and `RETURNING` image are correctly delegated.

The only unresolved part is the cross-row first-write/error-candidate boundary in N31-1.

## 26. INSERT uniqueness and current-command owners

Current-command rows already published by the same statement participate as live uniqueness owners.

Thus:

```sql
INSERT INTO t VALUES (1), (1);
```

cannot insert two successful owners merely because ordinary snapshot visibility hides the first current-command row.

## 27. INSERT batching

Batching is performance guidance. It may amortize encoding, FSM search, latching, and index-key construction.

It may not alter:

```text
multiplicity
conversion/constraint errors
canonical lock order
current-owner state
WAL-before-data
publication boundary
RETURNING image
affected count.
```

## 28. UPDATE old-row image

Every `SET` RHS reads one immutable complete old-row image. Unmentioned columns are copied from it.

Consequently:

```sql
SET a = b, b = a
```

is a swap, not sequential assignment.

## 29. UPDATE vectorization and lock waits

“Safely grouped” targets must already have exact validated old-row states appropriate for evaluation.

A wait breaks any vector batch whose retained row state may have become stale. After wake, revalidation must occur before evaluating or publishing from that target.

## 30. UPDATE uniqueness and excluded RID

The only self-exclusion is the exact revalidated old target RID when retaining the same key.

It does not exclude:

```text
all versions owned by the transaction
all rows from the same command
another target
another replacement version.
```

## 31. UPDATE key swaps

Uniqueness is immediate, not deferred final-state uniqueness.

For:

```text
A key=1
B key=2
A -> 2
B -> 1
```

either mutation order finds an existing current owner and rejects the swap. Spool order does not make the swap legal.

## 32. UPDATE no-op assignment

A qualifying `SET x=x` target counts as acted upon and produces the correct final-new `RETURNING` image.

An implementation may avoid redundant physical work only if it preserves all required locks, constraints, conflicts, count, and result semantics.

## 33. DELETE execution

DELETE:

```text
acquires/revalidates exact TUPLE_WRITE
publishes xmax/cmax
buffers the retained old-row RETURNING image
counts the target once.
```

It does not physically delete secondary-index entries. VACUUM owns cleanup.

## 34. DML single-worker execution

The single-worker write phase is a proportionate v1 capability restriction.

Its semantic purpose is to avoid undefined parallel lock/publication/error coordination. The phrase “unless a later architecture” is temporal and should be rewritten timelessly.

## 35. RETURNING row image

| Operation | Image |
|---|---|
| INSERT | Complete final new row |
| UPDATE | Complete final new row based on exact revalidated old target |
| DELETE | Retained exact old row |

No stale, partial, or unrelated physical version may supply the image.

## 36. RETURNING bag and order

All DML `RETURNING` results are unordered bags.

Order may vary by source, RID, spool, spill, mutation, or worker schedule. Multiplicity and typed values may not vary.

## 37. RETURNING spool memory and spill

The spool is statement/request-owned, query-memory accounted, spill-capable, and occurrence preserving.

It must own retained VARCHAR/vector backing. A borrowed producer chunk cannot escape.

## 38. RETURNING prepublication barrier

No `RETURNING` row may escape while:

```text
retry remains possible
writes remain incomplete
a required row-semantic failure remains possible
statement completion is unsuccessful.
```

Internally computed rows have no publication authority.

## 39. Explicit-transaction RETURNING

After successful statement completion:

```text
transaction remains ACTIVE
complete affected count is authoritative
RETURNING cursor may be consumed before COMMIT
later ROLLBACK does not retract already observed statement results.
```

The rows are not durability or COMMIT acknowledgements.

## 40. Autocommit RETURNING

No successful count or row is exposed before implicit COMMIT completes C4–C5.

A prepublication commit failure exposes no successful DML result. A postcommit transport failure cannot reverse COMMITTED.

## 41. Post-success RETURNING spill-read failure

**Unresolved — N31-2.**

For an explicit transaction, Architecture does not say whether a spill read failing after statement success and after one returned chunk is:

- result-delivery failure after a successful statement, leaving the transaction `ACTIVE`; or
- retroactive statement failure after persistent writes, forcing `MUST_ABORT`.

Returned rows cannot be retracted, making the second interpretation particularly problematic.

For autocommit, the transaction necessarily remains `COMMITTED`; result delivery may fail after a prefix. The client may not receive the complete result or affected count.

## 42. Transport failure

A transport/session loss after explicit-statement success does not rewrite that statement’s historical result. Under §39.1.7, an `ACTIVE` transaction whose session is lost is automatically aborted and cleaned up.

After autocommit’s durable/terminal commit, transport failure leaves the transaction `COMMITTED`.

## 43. Affected-row/result envelope

The count is exposed as metadata in the successful statement-result envelope:

- with or without `RETURNING`;
- at statement success for explicit transactions;
- after C4–C5 for autocommit.

No fake relational row is required. Wire protocol and concrete API structure remain implementation choices.

## 44. Query cursor contract

The public contract is a synchronous cursor:

```text
Next() -> chunk or FINISHED
```

Terminal errors propagate through the structured Chapter-26/39 error channel rather than becoming `FINISHED`.

Concurrent or reentrant `Next()` calls are not promised.

## 45. Result-chunk lifetime

A returned chunk remains valid until:

```text
the next Next() call
or
cursor destruction,
```

whichever occurs first.

Longer retention requires copying/materialization. All fixed, NULL, dictionary, constant, and VARCHAR backing must remain valid through that lifetime.

## 46. SELECT prefix followed by error

A SELECT may return a completed prefix before a later demanded non-DML expression or source error.

The prefix is not retracted, but the query’s complete result is failure. A normal read-only user error ordinarily leaves an explicit transaction `ACTIVE`.

## 47. FINISHED/EOS

Internal operator `FINISHED` is not client EOS.

Client `FINISHED` requires the root/result owner to establish successful exhaustion of all required pipelines, finalizers, and result state.

## 48. Cursor terminal state

No rewindability or repeated-`FINISHED` idempotence is promised. Calls after terminal error or destruction are protocol misuse unless a concrete API supplies a stronger convenience contract.

This is not a SQL-semantic gap.

## 49. DML result-envelope matrix

| Statement | Explicit transaction | Autocommit |
|---|---|---|
| INSERT no RETURNING | Count at statement success; transaction ACTIVE | Count only after C4–C5 |
| INSERT RETURNING | Count + row bag at statement success | Count + row bag after C4–C5 |
| UPDATE no RETURNING | Distinct acted-target count; ACTIVE | Count after C4–C5 |
| UPDATE RETURNING | Count + final-new bag; ACTIVE | Count + bag after C4–C5 |
| DELETE no RETURNING | Distinct deleted-target count; ACTIVE | Count after C4–C5 |
| DELETE RETURNING | Count + retained-old bag; ACTIVE | Count + bag after C4–C5 |

Failed statements expose neither successful count nor a DML row prefix.

## 50. Failure after internal result computation

Computed rows and provisional counts remain internal.

On failure before successful publication:

```text
discard spool
discard count
publish canonical error
apply §39 first-write consequence.
```

No partial SQL success exists.

## 51. DML resource-failure matrix

| Failure | Before first write | After first write |
|---|---|---|
| Target-spool OOM/SpillIO | FA | Normally unreachable after target Finalize unless later state fails |
| Assignment staging OOM | FA | MA if earlier target already published |
| RETURNING construction OOM/SpillIO | FA | MA |
| Representability failure | FA | MA |
| Cancellation | FA | MA |
| Persistent corruption | NC as lower owner requires | NC |

N31-1 affects whether an ordinary later-row error is established before or after publication; dynamic resource failures remain occurrence-owned.

## 52. DML cancellation

Cancellation follows the actual first-write boundary. It does not create an internal retry.

After publication, automatic abort is required. During client-side `RETURNING` consumption, ordinary cancellation is entangled with N31-2 unless it is a session loss, which §39.1.7 owns.

## 53. Retry freshness

No abandoned-attempt spool, result row, error candidate, affected count, cursor, or spill file may enter the replacement attempt.

Transaction-duration locks may survive only where Chapters 11 and 15 explicitly permit them.

## 54. Physical DDL

DDL operators are management/control operators that delegate SchemaLock, writer-gate, catalog MVCC, namespace, file, WAL, and terminal publication to Chapter 21.

They need not use relational hot-path interfaces internally.

## 55. CREATE INDEX publication

The index build remains private/offline until the transaction-owned catalog publication.

No committed catalog visibility precedes terminal commit. An aborted build remains unpublished and follows orphan/private-file cleanup ownership.

## 56. DDL single-coordinator wording

“DDL execution remains single-coordinator in v1” is a durable capability statement, not current project state. It is acceptably timeless, though “is single-coordinator in v1” would be slightly cleaner.

## 57. DDL result interface

DDL returns command completion/status, not relational rows or DML affected count.

Explicit-transaction statement success is distinct from committed global catalog visibility. Autocommit completion waits for commit publication.

## 58. Physical VACUUM ownership

`PhysicalVacuum` invokes Chapter 14. It does not redefine reclamation predicates, horizons, RID reuse, index cleanup, status reclamation, or publication units.

## 59. VACUUM concurrency

Chapter 31 does not globally serialize VACUUM because it explicitly delegates to maintenance/storage coordination rules.

Chapter 14 remains authoritative:

```text
same-table VACUUM: serialized
different-table VACUUM: concurrent
VACUUM with DML/SELECT: concurrent
VACUUM with ANALYZE: concurrent
```

Generic query-scheduler parallelism does not override these owners.

## 60. VACUUM progress/debug wording

“It may later expose progress/debug result chunks” is inappropriate roadmap language in canonical Architecture and creates an unnecessary hypothetical result surface.

This is part of MINOR-1.

The v1 contract currently has no VACUUM relational row result.

## 61. VACUUM cancellation/partial work

VACUUM is not all-or-nothing transactional DML.

Cancellation may leave already completed WAL-backed maintenance units in place. The current unit must complete or restore according to Chapter 14; prior units are not rolled back.

## 62. Physical ANALYZE ownership

`PhysicalAnalyze` delegates data/statistics semantics to Chapters 14, 21, and 34 for one resolved table and immutable manifest.

## 63. ANALYZE execution context

It uses:

```text
normal transaction snapshot
ReadEpochGuard where needed
QueryMemoryManager
optional bounded spill helper
cancellation
profiling.
```

These do not change statistics publication semantics.

## 64. ANALYZE scan

The required visible heap scan remains direct and complete. Required physical index-statistics scans are additional; heap scanning alone does not fabricate index statistics.

## 65. ANALYZE snapshot and concurrent DML

ANALYZE uses a stable MVCC snapshot. Concurrent DML is legal and may make statistics stale relative to later commits without making the descriptor internally inconsistent.

No schema-changing exclusivity is required merely for row-set stability.

## 66. ANALYZE manifest revalidation

Before the first statistics-row publication, the operation must revalidate:

```text
TableId liveness
SchemaVer
ColumnIds
complete IndexId set
index key schema/fingerprint
candidate manifest equality.
```

A stale candidate is discarded whole.

## 67. ANALYZE partial publication

If any statistics row has published and later work fails:

```text
transaction -> MUST_ABORT
partial StatsVersion remains globally unusable
previous committed descriptor remains authoritative.
```

No descriptor is updated in place.

## 68. ANALYZE transaction-local/global publication

A successful explicit-transaction ANALYZE may supply a transaction-local descriptor to later statements in the same transaction.

Global cache/descriptor publication occurs only after terminal commit and applicability revalidation.

## 69. ANALYZE failure/cancellation

Before any statistics-row publication, a recoverable failure may leave the explicit transaction `ACTIVE`.

After publication, failure or cancellation mandates abort.

## 70. ANALYZE single-coordinator wording

“The baseline implementation is single-coordinator” is implementation-status phrasing. If single-coordinator is the intended v1 capability, it should be stated directly and timelessly.

This is part of MINOR-1.

## 71. DDL/VACUUM/ANALYZE result matrix

| Statement | Relational rows | Count | Completion/publication |
|---|---:|---:|---|
| CREATE TABLE | No | No DML count | Explicit statement success; global visibility after commit |
| CREATE INDEX | No | No DML count | Private build, then transactional publication |
| DROP | No | No DML count | Logical transactional deletion; physical retirement may continue |
| VACUUM | No in v1 | No DML count | Maintenance-command completion; completed units may survive failure |
| ANALYZE | No | No DML count | Transactional statement; global descriptor after commit |

## 72. Non-row result interface

Non-row statements complete through command-result metadata plus terminal cursor/result status. They need not fabricate an empty row or hidden `LogicalSlotId`.

## 73. Result schema and LogicalSlotIds

SELECT and DML `RETURNING` chunks use the declared physical/logical result schema.

Affected count and command completion metadata are not relational slots. Runtime state may not invent output `LogicalSlotIds`.

## 74. Result memory and ownership

Query/request-owned buffering remains accounted until release or valid ownership transfer.

Client-owned copies need not remain charged to the query after transfer. No borrowed page, chunk, vector, or temporary block may expire during the guaranteed returned-chunk lifetime.

## 75. Connection loss

- SELECT: cancel/clean query.
- Explicit transaction in `ACTIVE`: automatic abort under §39.1.7.
- Autocommit after irreversible commit: transaction remains `COMMITTED`.
- COMMITTING after publication-authorizing append: commit is uncancellable.

Client knowledge and database outcome remain distinct.

## 76. Complete result-prefix matrix

| Scenario | Statement | Transaction | Prefix | Count/retry |
|---|---|---|---|---|
| SELECT prefix, later expression error | Failed query | Usually ACTIVE | Retained | No DML count; no automatic replay |
| Explicit DML success, later transport/session loss | Statement success stands | ACTIVE transaction auto-aborts due session loss | Retained | Count was authoritative; no same-session retry |
| Explicit DML success, later spool-read failure | **N31-2 unresolved** | ACTIVE or MUST_ABORT unclear | Retained | Count authority/retry unclear |
| Autocommit precommit failure | No successful DML response | ABORTED/failed | None | No count |
| Autocommit postcommit transport failure | Successful committed DML; delivery uncertain | COMMITTED | Possible prefix | Count may be unobserved; no transaction retry |
| Autocommit postcommit spool-read failure | Execution committed; delivery fails | COMMITTED | Possible prefix | Count may be unobserved; no rollback |

## 77. Result error channel

The cursor pseudocode omits concrete C++ error syntax, but Chapters 26 and 39 distinguish terminal error from `FINISHED`. A conforming API must preserve that distinction.

No architectural wire/API type is required.

## 78. Statement success versus transaction success

The distinction is correctly maintained:

```text
explicit:
    statement success may precede COMMIT and global visibility

autocommit:
    successful response waits for implicit COMMIT publication.
```

DDL and ANALYZE use the same distinction.

## 79. First-persistent-write terminology

Chapter 31’s “persistent WAL-visible write” refers to the §39.1.2 transaction-owned publication event, not:

```text
LSN reservation
private bytes
WAL durability
page flush
temporary spill writes.
```

Terminology is consistent.

## 80. WAL/index ordering

Chapter 31 delegates exact heap/index/MTR ordering to Chapter 15. Nothing locally reverses WAL-before-data or the required INSERT/UPDATE publication order.

## 81. UNIQUE semantics

Chapter 31 preserves:

```text
canonical key-lock order
full current-state recheck
post-wait revalidation
current-command owners
exact old-RID exclusion
immediate key-swap rejection.
```

## 82. Writer-gate/DDL coordination

DML obtains shared `TableWriterGate(T)` before its first persistent target write and before tuple/key locks.

A wake requires current descriptor, liveness, transaction-local schema/index set, and DML candidate revalidation. Newly authoritative indexes must be incorporated before heap/index mutation.

## 83. Target schema/descriptor stability

The immutable bound descriptor remains a semantic input, but current publication state is revalidated after writer-gate waits.

V1 has no general ALTER-column surface. DROP/CREATE INDEX races are governed by the gate and manifest recheck, preventing stale physical maintenance sets.

## 84. INSERT SELECT

`INSERT SELECT` is supported. Source errors cannot create partial SQL success. If prior DML writes published, a later source failure produces the §39 mandatory-abort outcome.

N31-1 still affects whether an ordinary source expression failure must be established before the first write.

## 85. INSERT SELECT self-read

The source uses the statement snapshot and current command boundary. Newly inserted rows from the same command are not recursively visible to the source scan, so self-feeding does not occur.

No target spool is required for this case.

## 86. UPDATE/DELETE subqueries

Only Chapter-20-supported uncorrelated expression subqueries are admitted. They are independent side plans, evaluated at most once per bound occurrence under demand rules.

They cannot capture the target row as an outer reference.

## 87. DML RequiredSlotSet

Required inputs include:

```text
exact target RID
assignment inputs
complete replacement values
unique-key inputs
RETURNING inputs
necessary provenance.
```

Pruning cannot force a later semantic reread of a different old-row version.

## 88. DML varlen/value ownership

Retained old/new/RETURNING VARCHAR data must survive:

```text
chunk reuse
lock waits
spill
retry boundaries
statement-result consumption.
```

Borrowed producer pointers cannot cross those lifetimes.

## 89. Target-dedup implementation freedom

Legal exact methods include:

```text
uniqueness proof
exact hash dedup
exact sort dedup
another exact physical-RID set.
```

No SQL DISTINCT canonical representative or ordering property is imported.

## 90. Target-dedup/error precedence

Duplicate candidate occurrences removed by canonical target dedup are not ordinary error candidates.

The implementation must establish final target membership before using a duplicate discovery’s later row-dependent work as a public DML candidate.

## 91. Lock-wait/vector-batch behavior

No page/index latch or read epoch crosses a blocking logical-lock wait.

Vector state dependent on target bytes must be discarded or revalidated after wake. A wait cannot cause lost, duplicate, or stale-row mutation.

## 92. Partial publication and abort

After any transaction-owned DML write, later statement failure means:

```text
no successful count
no RETURNING prefix
automatic transaction abort
physical aborted garbage may remain
no user-DML physical undo.
```

N31-2 concerns only failure after Chapter 31 has already declared statement success.

## 93. Transaction-owned lock lifetime

Query cleanup must not prematurely release:

```text
TUPLE_WRITE
UNIQUE_KEY
TableWriterGate
SchemaLock/publication gates.
```

They survive through terminal publication according to their owners.

## 94. Resource ownership matrix

| Lifetime | Resources |
|---|---|
| Attempt-local | Target spool, dedup state, result spool before valid transfer, vector scratch, spill files, cursors, ordinary candidates, provisional count |
| Transaction-owned | Logical locks, shared writer gate, write-status dependency, published tuple/index/catalog versions |
| Immutable/shared | Bound plan, IDs, descriptors subject to current-state validation |
| Request/result-owned | Successfully transferred explicit result or postcommit autocommit result |

## 95. DDL partial failure

Private files and consumed IDs may survive as cleanup-owned artifacts. Catalog publication crosses the write boundary; later failure mandates abort.

No incomplete catalog object becomes committed-visible.

## 96. VACUUM transactional model

VACUUM uses independently durable maintenance units and is not ordinary transactional DML. Command failure does not roll back completed units or expose a partial user-DML result.

## 97. ANALYZE transactional model

ANALYZE is transactional system DML. Statistics rows are MVCC catalog rows; first publication crosses §39.1’s write boundary.

Completeness filtering prevents partial use but does not erase published physical rows.

## 98. Cursor threading/reentrancy

Only a simple synchronous client contract is promised. Concurrent `Next()` calls, asynchronous delivery, and reentrant callbacks are not architecture guarantees.

## 99. Chunk-boundary semantics

Chunk size and partitioning are physical choices. They cannot change:

```text
result bag
row order where ordered
multiplicity
error ownership
transaction outcome.
```

Nonterminal fake empty chunks are unnecessary and cannot substitute for progress or EOS.

## 100. Control operators and vectorization

Classifying DDL/VACUUM/ANALYZE as control operators does not prohibit vectorized scans, batched collection, or efficient inner loops permitted by their owners.

## 101. Chapter-32 handoff

Chapter 32 owns scheduling and worker execution.

Chapter 31’s DML single-worker write phase and control-operator coordinator restrictions are capability limits. General worker scheduling cannot implicitly parallelize transactional publication or maintenance ownership.

## 102. Temporal/document-role audit

| Live phrase | Classification |
|---|---|
| “Read phase” / “Write phase” | Runtime lifecycle; valid |
| “current-state/current-command” | Semantic state; valid |
| “later transaction COMMIT/ROLLBACK” | Runtime transaction sequence; valid |
| “unless a later architecture explicitly defines…” | Project-change chronology; stale |
| “may later expose progress/debug result chunks” | Roadmap/hypothetical surface; stale |
| “baseline implementation is single-coordinator” | Implementation-status phrasing; stale |
| “v1 execution baseline” | Durable capability summary, but “baseline” can be simplified |
| “later cursor operation” | Runtime API sequence; valid |

## 103. Target-spool complexity

**CORE.**

It directly teaches and enforces Halloween protection, one-target-once semantics, revalidation, spill, and clean retry boundaries.

## 104. RETURNING-spool complexity

**JUSTIFIED ADVANCED / effectively CORE for this transaction model.**

Whole-statement buffering is justified by internal retry and the absence of user-DML physical undo.

## 105. Single-worker DML complexity

**CORE SIMPLIFICATION.**

It avoids parallel mutation, lock, uniqueness, error-reduction, and publication coordination while retaining vectorized read/evaluation work where safe.

## 106. Cursor-lifetime complexity

**CORE and proportionate.**

“Valid until next `Next()` or destruction” is simple, explicit, and avoids mandatory reference-counted result ownership.

## 107. Direct PhysicalAnalyze complexity

**CORE and proportionate.**

The required full visible heap scan and manifest-aware statistics publication do not benefit from join-order/access-path enumeration.

## 108. Implementation freedom

Chapter 31 does not improperly freeze:

```text
spool container
dedup container
batch size
spill encoding beyond common validation requirements
result chunk size
heap container
DDL task type
VACUUM scheduling primitive
statistics helper representation.
```

## 109. Existing reusable Verification coverage

[docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:21496) already covers substantial Chapter-31 input:

```text
V9/V11/V14/V15 transaction, lock, vacuum, and first-write matrices
V18/V19 DML and maintenance syntax/binding
V20 logical DML bags, targets, demand, and subqueries
V21 target, row-image, affected-count, RETURNING, DDL, and ANALYZE procedures
V23 retained-value lifetime
V24 memory/spill/cleanup
V25 ordinary expression provenance
V26 readiness/error transport
DML Execution Tests
Control-Operator Tests
Vacuum and Reclamation Tests
Statistics Publication and Versioning Tests.
```

## 110. Missing/partial Chapter-31 Verification coverage

Synchronization must wait for N31-1/N31-2. After decisions, dedicated coverage is still needed for:

```text
complete target-spool-before-write barriers
spooled-RID snapshot/epoch/claim handoff
exact stale-image rejection
ordinary-candidate closure versus first write
post-success RETURNING spill-read fault classification
explicit/autocommit affected-count delivery
cursor chunk lifetime across all vector forms
cursor terminal error versus FINISHED
non-row command envelopes
VACUUM exact physical concurrency composition
VACUUM completed-unit preservation
ANALYZE physical coordinator behavior
Chapter-31 chronology/stale-rule audit.
```

Existing V21 procedures currently mark the error-consequence composition complete despite N31-1’s frozen-owner conflict; this must be revisited only after policy resolution.

## 111. Previous-chapter regression table

| Frozen owner | Result |
|---|---|
| Ch. 9 snapshot/CommandId | No contradiction |
| Ch. 11 locks/UNIQUE | No contradiction |
| Ch. 14 maintenance/RID reuse | No contradiction |
| Ch. 15 DML/count/retry | No contradiction except N31-1 composition |
| Ch. 18 SQL surface | No unsupported join syntax added |
| Ch. 19 binding | No contradiction |
| Ch. 20 logical bags/demand | No contradiction |
| Ch. 21 DML/RETURNING/error | N31-1 exposed |
| Ch. 23 ownership | No contradiction |
| Ch. 24 memory/spill | No contradiction |
| Ch. 25 error ownership | No contradiction |
| Ch. 26 readiness/publication | N31-2 lacks final classification |
| Chs. 29–30 reuse | No contradiction |
| Ch. 34 statistics | No contradiction |
| Ch. 39 outcomes | N31-1 and N31-2 require reconciliation |

## 112. Maintenance concurrency matrix

| Pair | Owner | Required behavior | Generic scheduler role |
|---|---|---|---|
| VACUUM / VACUUM same table | §14.17.1 | Serialize via `TableVacuumOwner` and revalidate | None |
| VACUUM / VACUUM different tables | §14.17.1 | Concurrent | May schedule independent work |
| VACUUM / DML | Chs. 11, 14, 15 | Concurrent with epochs/claims/revalidation | Does not redefine safety |
| VACUUM / ANALYZE | §14.17.1 | Concurrent | Separate owners |
| ANALYZE / ANALYZE | Chs. 14, 34 | Concurrent, including same table | Distinct StatsVersions |
| ANALYZE / DML | Chs. 14, 21, 34 | Concurrent under fixed snapshot | DML may make stats stale |
| ANALYZE / DROP or CREATE INDEX | §14.17.1 | `STATS_PUBLISH`/`MANIFEST_CHANGE` wait and revalidate | No implicit bypass |

## 113. DML error-precedence example matrix

| Case | Candidates/events | Correct public result |
|---|---|---|
| A: row 1 valid; row 2 scalar error | One ordinary row-2 candidate; write timing varies | Error identity clear; `FA` versus `MA` unresolved by N31-1 |
| B: row 1 UNIQUE candidate; row 2 earlier-span scalar candidate | Both candidates established before publication | Row-2 scalar candidate wins; normally `FA` |
| C: UPDATE target 1 valid; target 2 NOT NULL | Later ordinary candidate; target 1 may already publish | Error identity clear; `FA` versus `MA` unresolved |
| D: target 1 publishes; later target RETURNING errors | Later ordinary RETURNING candidate | Error identity clear; consequence unresolved by N31-1 |
| E: dynamic deadlock after ordinary candidate exists | Dynamic failure outside source-span ranking | Chapter-11/39 deadlock owner; `MA`; no invented cross-class ranking |

## 114. Result-publication example matrix

| Scenario | Statement outcome | Transaction | Prefix | Count |
|---|---|---|---|---|
| SELECT prefix then expression error | Failed query | Normally ACTIVE | Retained | N/A |
| Explicit DML success then transport loss | Success remains historical | Automatic ABORT if session lost while ACTIVE | Retained | Was authoritative |
| Explicit DML success then spill-read failure | N31-2 unresolved | ACTIVE or MUST_ABORT unclear | Retained | Authority unclear |
| Autocommit precommit failure | Failed | ABORTED/failed | None | None |
| Autocommit postcommit transport failure | Successful execution; delivery uncertain | COMMITTED | Possible prefix | May be unobserved |
| Autocommit postcommit spill-read failure | Successful committed execution; delivery failure | COMMITTED | Possible prefix | May be unobserved |

## 115. Technical-consistency matrix — 220 checks

Each numbered item below is one independently evaluated technical check.

1. **001–010 — Boundary and ownership:** 001 chapter start; 002 chapter end; 003 subsection inventory; 004 Chapter-32 boundary; 005 document roles; 006 physical operator registry; 007 logical/physical separation; 008 transaction-owner delegation; 009 memory-owner delegation; 010 result-owner delegation.
2. **011–020 — Target materialization:** 011 complete-before-write; 012 RID retained; 013 required old values; 014 WHERE before spool; 015 rewrite joins remain supported-only; 016 no UPDATE FROM; 017 no DELETE USING; 018 finalized set immutable; 019 write phase consumes finalized state; 020 spool is attempt-local.
3. **021–030 — Target uniqueness:** 021 exact RID equality; 022 no hash-only identity; 023 collision resolution; 024 no payload identity; 025 no TxnId identity; 026 no version-family collapse; 027 uniqueness-proof path; 028 explicit-dedup path; 029 duplicate count suppression; 030 duplicate RETURNING suppression.
4. **031–040 — Halloween protection:** 031 index-key change; 032 predicate change; 033 replacement version; 034 heap scan; 035 index scan; 036 no post-write target addition; 037 CommandId not sole defense; 038 spill does not weaken barrier; 039 retry rebuilds targets; 040 one mutation per RID.
5. **041–050 — RID lifetime:** 041 snapshot pins needed history; 042 discovery epoch protects dereference; 043 TUPLE_WRITE request is retention claim; 044 epoch/claim overlap; 045 no epoch through wait; 046 post-wait re-fetch; 047 DEAD rejection; 048 UNUSED rejection; 049 rebound identity rejection; 050 vacuum cannot bypass live claims.
6. **051–060 — Old image and slots:** 051 assignment inputs retained; 052 full replacement inputs retained; 053 unique inputs retained; 054 RETURNING inputs retained; 055 hidden RID survives; 056 LogicalSlotId preserved; 057 irrelevant column pruning; 058 no stale reread substitution; 059 varlen bytes stable; 060 exact revalidated image authority.
7. **061–070 — Retry:** 061 RC prewrite eligibility; 062 same CommandId; 063 fresh snapshot; 064 spool discarded; 065 dedup discarded; 066 count discarded; 067 RETURNING discarded; 068 diagnostics discarded; 069 spill discarded; 070 no postwrite retry.
8. **071–080 — INSERT:** 071 typed input only; 072 multiplicity; 073 conversion; 074 NOT NULL; 075 canonical unique lock order; 076 current-state check; 077 current-command owner; 078 heap publication; 079 index publication; 080 final-new RETURNING.
9. **081–090 — UPDATE:** 081 simultaneous assignments; 082 copied unmentioned columns; 083 swap semantics; 084 exact old version; 085 safe vector grouping; 086 wait breaks stale batch; 087 exact excluded RID; 088 immediate key swap; 089 replacement version; 090 final-new RETURNING.
10. **091–100 — DELETE/count:** 091 exact write claim; 092 exact revalidation; 093 xmax/cmax; 094 no new tuple; 095 old index retained; 096 old-row RETURNING; 097 count distinct acted target; 098 stale target zero; 099 failed attempt zero authority; 100 count independent of physical writes.
11. **101–110 — Ordinary errors:** 101 candidate eligibility; 102 abandoned-attempt exclusion; 103 dedup exclusion; 104 SourceSpan start; 105 span specificity; 106 phase order; 107 no RID tie-break; 108 no batch tie-break; 109 dynamic failure separation; 110 N31-1 consequence conflict identified.
12. **111–120 — First write:** 111 publication not reservation; 112 publication not WAL durability; 113 publication not page flush; 114 publication not commit; 115 prewrite FA; 116 postwrite MA; 117 no user-DML undo; 118 no partial count; 119 no failed RETURNING prefix; 120 no same-TxnId postwrite retry.
13. **121–130 — RETURNING:** 121 statement-owned spool; 122 memory accounting; 123 spill capability; 124 INSERT image; 125 UPDATE image; 126 DELETE image; 127 unordered bag; 128 exact multiplicity; 129 prepublication barrier; 130 N31-2 post-success read ambiguity identified.
14. **131–140 — Explicit/autocommit result:** 131 explicit precommit consumption; 132 rollback nonretraction; 133 row not COMMIT acknowledgement; 134 autocommit C4–C5 withholding; 135 precommit failure no result; 136 postcommit transport cannot abort; 137 affected count same envelope; 138 no count persistence; 139 client uncertainty distinct; 140 session loss ACTIVE abort.
15. **141–150 — Cursor:** 141 chunk/FINISHED distinction; 142 terminal error not FINISHED; 143 chunk ownership; 144 lifetime to next call; 145 lifetime to destruction; 146 longer-life copy; 147 SELECT prefix retained; 148 failed query not success; 149 internal FINISHED not EOS; 150 no concurrent cursor promise.
16. **151–160 — DDL:** 151 control role; 152 SchemaLock delegation; 153 writer-gate delegation; 154 private CREATE INDEX; 155 no early committed metadata; 156 explicit success distinct from commit; 157 autocommit waits for commit; 158 no relational row bag; 159 transaction-owned gate lifetime; 160 no vector-hot-path prohibition.
17. **161–170 — VACUUM:** 161 Chapter-14 ownership; 162 nontransactional units; 163 same-table serialization; 164 different-table concurrency; 165 DML concurrency; 166 ANALYZE concurrency; 167 object liveness; 168 cancellation preserves completed units; 169 no WAL rollback fiction; 170 future-progress wording classified.
18. **171–180 — ANALYZE:** 171 resolved table; 172 stable snapshot; 173 full visible heap scan; 174 index statistics scans; 175 bounded state; 176 manifest revalidation; 177 transaction-local descriptor; 178 no partial global publication; 179 postrow failure abort; 180 same-table concurrent generations.
19. **181–190 — Memory/ownership:** 181 target spool fixed bytes; 182 target spool VARCHAR; 183 RETURNING spool fixed bytes; 184 RETURNING VARCHAR; 185 spill buffers; 186 result chunks; 187 attempt cleanup; 188 valid result transfer; 189 transaction locks not query-cleaned; 190 no cross-attempt spill.
20. **191–200 — Schema/locks/WAL:** 191 shared writer gate; 192 wake revalidation; 193 current index manifest; 194 no latches across waits; 195 transaction lock lifetime; 196 WAL-before-data; 197 exact MTR handoff; 198 no stale descriptor publication; 199 no DDL bypass; 200 no hidden physical undo.
21. **201–210 — Parallelism/document role:** 201 DML write single worker; 202 DDL coordinator; 203 VACUUM generic scheduler does not redefine ownership; 204 ANALYZE coordinator wording; 205 no mandatory container; 206 no mandatory batch size; 207 no mandatory spool codec; 208 stale “later architecture”; 209 stale future progress chunks; 210 stale baseline-implementation phrasing.
22. **211–220 — Verification/regression:** 211 V21 target reuse; 212 V15 first-write reuse; 213 V14 RID reuse; 214 V24 spill reuse; 215 V26 cursor transport reuse; 216 missing post-success spool-read case; 217 N31-1 blocks exact oracle; 218 maintenance matrix coverage needed; 219 no Ch29/30 regression; 220 no audit-created change.

```text
ACTUAL TECHNICAL CHECKS:
    220
```

## 116. Complete BLOCKING findings

### N31-1 — Ordinary DML candidate closure versus first persistent write

**Sections:** §§21.16.1, 21.20(29), 31.6–31.7, 39.1.3–39.1.4, and Chapter-41 verification obligations.

**Conflict:**

- Chapter 21 says physical visitation cannot change an ordinary error’s §39.1 transaction consequence.
- Chapter 39 expressly requires a multi-row case where rows 1–4 publish and a later conversion/arithmetic/constraint error produces `MA`.
- Chapter 31’s row/batch execution permits publication before later rows are evaluated.

**Observable example:**

```sql
BEGIN;
INSERT INTO t VALUES (1, 10), (2, 1 / 0);
```

Implementation A prevalidates demanded ordinary row semantics:

```text
error before first write -> FA -> transaction ACTIVE
```

Implementation B processes and publishes row 1 first:

```text
same error after first write -> MA -> automatic ABORT
```

**Alternative A:** Require closure/reduction of every semantically required ordinary DML candidate before the first persistent DML write.

**Alternative B:** Permit boundary-sensitive discovery, but define a canonical evaluation/publication schedule or explicitly authorize bounded transaction-state variation.

**Recommendation:** Alternative A. It best preserves the existing Chapter-21 rule that physical execution shape cannot change transaction consequence.

**Smallest edit surface:** §§21.16.1, 31.6–31.7, 39.1.3–39.1.4, and corresponding Chapter-41 obligation.

### N31-2 — Post-success explicit-transaction RETURNING spill-read failure

**Sections:** §§15.1.2, 15.7.3, 31.9–31.10, 39.1.3–39.1.4.

**Missing rule:**

```text
DML writes complete
statement declared successful
transaction remains ACTIVE
first RETURNING chunk reaches client
later attempt-local result-spool read fails
```

**Alternative A — delivery failure:** The statement remains successful, affected count remains authoritative, returned prefix remains, and transaction stays `ACTIVE`. Session loss may independently abort it.

**Alternative B — retroactive statement failure:** The read failure becomes post-write `MA`, automatically aborting the transaction and invalidating successful count authority even though returned rows cannot be retracted.

**Observable difference:** The client can either continue the explicit transaction or must observe automatic abort.

**Recommendation:** Alternative A. Once Chapter 31 publishes successful statement completion and transfers cursor ownership, spool reads should be result delivery, not retroactive statement execution. Autocommit remains `COMMITTED`.

**Smallest edit surface:** §§31.9–31.10 and §39.1.4, with a cross-reference in §15.1.2.

## 117. Complete MAJOR findings

```text
NONE
```

The apparent long-lived target-RID risk is closed by the statement snapshot plus short read-epoch/live-claim handoff and post-lock revalidation.

## 118. Complete MINOR findings

### MINOR-1 — Project chronology/status language

Exact stale phrases:

```text
§31.7:
    “unless a later architecture explicitly defines...”

§31.12:
    “It may later expose progress/debug result chunks...”

§31.12.1:
    “The baseline implementation is single-coordinator.”
```

Consequence: document-role leakage and, for VACUUM, a hypothetical future SQL result surface in canonical Architecture.

Smallest repair:

```text
state current v1 capability timelessly;
remove hypothetical future VACUUM result chunks;
state PhysicalAnalyze coordinator capability directly.
```

## 119. Complete EDITORIAL findings

```text
NONE
```

## 120. Complete DESIGN-SCOPE questions

```text
NONE
```

Target spooling, buffered `RETURNING`, single-worker DML, simple cursor lifetime, external spill support, and direct `PhysicalAnalyze` are proportionate.

## 121. Frozen semantic questions

```text
N31-1 — ordinary DML candidate closure versus first persistent write
N31-2 — post-success explicit-transaction RETURNING spill-read failure
```

No additional `N31-*` question was found.

## 122. Recommended fixing sequence

**STEP A — chronology/document role**

- Remove “later architecture,” hypothetical VACUUM result chunks, and “baseline implementation” wording.

**STEP B — target/RID clarification**

- No semantic decision is required.
- Optionally make the snapshot → short read epoch → live `TUPLE_WRITE` claim → revalidation composition explicit.

**STEP C — DML error/first-write decision**

- Resolve N31-1 across frozen Chapters 21, 31, 39, and 41.

**STEP D — result-envelope decision**

- Resolve N31-2 and define explicit/autocommit post-success result-spool read behavior.

**STEP E — control operators**

- Apply only the low-risk timeless wording cleanup; no VACUUM/ANALYZE semantic redesign is needed.

**STEP F — frozen decisions**

- Issue a dedicated decision package for N31-1 and N31-2 before editing Architecture or Verification.

## 123. Exact next action

```text
CHAPTER-31 FROZEN SEMANTIC / DESIGN DECISION PACKAGE
```

It should decide N31-1 and N31-2. Verification synchronization must wait.

## Final direct answers

- Is the target identity set finalized before mutation? **Yes.**
- Can one RID appear twice in the final spool? **No.**
- Does dedup preserve count and RETURNING multiplicity? **Yes.**
- Is “supported target-producing joins” valid v1 language? **Only for authorized internal semantic rewrites; it does not add UPDATE FROM/DELETE USING.**
- Can VACUUM reclaim/reuse a spooled target RID? **No.**
- What prevents it? **Active snapshot lifetime, short read epochs, live `TUPLE_WRITE` claims, and exact revalidation.**
- Does UPDATE use the exact revalidated old version? **Yes.**
- Can stale spooled values be applied after a conflicting wait? **No.**
- Does RC retry discard all attempt-local state? **Yes.**
- Can retry occur after first write? **No.**
- Does Chapter 31 correctly close ordinary DML error precedence? **No; N31-1 remains.**
- Can physical order change `ACTIVE` versus `MUST_ABORT` under the present text? **Yes, under competing permitted execution shapes; that is the blocking defect.**
- Can INSERT publish row 1 before a later higher-priority ordinary error? **The row-at-a-time prose permits it, while Chapter 21’s consequence invariant points the other way.**
- Can UPDATE publish before a later SET/RETURNING error? **Same unresolved conflict.**
- Are dynamic failures separate? **Yes.**
- Does INSERT detect same-command unique owners? **Yes.**
- Does UPDATE self-exclude only its exact old RID? **Yes.**
- Are key swaps immediate? **Yes.**
- Can vectorization cross a wait using stale values? **No.**
- Does DELETE leave index garbage to VACUUM? **Yes.**
- Is DML write parallelism single-worker? **Yes.**
- Is all related wording timeless? **No; one phrase is chronological.**
- Is RETURNING unordered? **Yes.**
- Can failed/retryable DML expose RETURNING rows? **No.**
- Can explicit-transaction RETURNING be exposed before COMMIT? **Yes, after statement success.**
- Can autocommit RETURNING be exposed before COMMIT? **No.**
- What happens after explicit-success RETURNING spill-read failure? **Unresolved by N31-2.**
- What happens after autocommit commit? **Transaction remains COMMITTED; result delivery may fail after a prefix.**
- Can returned rows be retracted? **No.**
- Where is affected count exposed? **Successful statement-result metadata/envelope.**
- Is no-RETURNING DML metadata defined? **Yes semantically; wire representation is free.**
- Can SELECT expose a prefix before later failure? **Yes.**
- Is cursor failure distinguishable from FINISHED? **Yes through Chapters 26/39, though concrete API syntax is unspecified.**
- Is chunk lifetime defined? **Yes.**
- Can query cleanup release transaction locks? **No.**
- Can failed-attempt spill data enter retry? **No.**
- Is DDL statement success distinct from commit? **Yes.**
- Is CREATE INDEX private until terminal publication? **Yes.**
- Is DDL single-coordinator wording architectural? **Yes.**
- Does Chapter 31 globally serialize VACUUM? **No.**
- Can different-table VACUUM operations run concurrently? **Yes.**
- Is future VACUUM progress output inappropriate? **Yes.**
- Does canceled VACUUM roll back completed units? **No.**
- Is ANALYZE transactional unlike VACUUM? **Yes.**
- Can ANALYZE globally expose partial/uncommitted statistics? **No.**
- Can post-publication ANALYZE failure leave transaction active? **No.**
- Is manifest revalidation complete? **Yes.**
- Is “baseline implementation single-coordinator” chronology leakage? **Yes.**
- Does PhysicalAnalyze bypass join/access-path enumeration? **Yes, correctly.**
- Does Chapter 31 contain correctness-relevant invention points? **Yes: N31-1 and N31-2.**
- Can Chapter 31 stand unchanged as ideal canonical v1 Architecture? **No.**

```text
CHAPTER 32 REVIEW:
    NOT STARTED

VERIFICATION MODIFICATION:
    NONE

IMPLEMENTATION:
    NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE
```
# 1. Decision-package verdict

**PROJECT-OWNER DECISIONS REQUIRED**

Both reported frozen questions are genuine:

- **N31-1:** The live Architecture simultaneously requires deterministic ordinary DML error precedence independent of physical visitation and permits ordinary errors after prior persistent row publication, producing different `FA`/`MA` outcomes.
- **N31-2:** The live Architecture does not classify a spilled `RETURNING` read failure after an explicit-transaction DML statement has already published successful completion.

Recommended decisions:

- **N31-1: Alternative A — close all ordinary DML candidates before the first persistent statement write.**
- **N31-2: Alternative A — after successful statement-result publication, later spool failures are result-delivery failures and do not retroactively fail the DML statement.**

These are recommendations only. No policy was changed.

# 2. Initial HEAD/status

```text
HEAD:
d61d80da4008d9f209269f18d9153f56901faa83

Commit:
d61d80d chapter 31 ARCHITECTURE analysis

Working tree:
clean

Index:
clean

Tracked modifications:
none

Staged modifications:
none

Untracked paths:
none
```

# 3. Final HEAD/status

```text
HEAD:
d61d80da4008d9f209269f18d9153f56901faa83

Commit:
d61d80d chapter 31 ARCHITECTURE analysis

Working tree:
clean

Index:
clean
```

`git diff --check` produced no output.

# 4. Audit-created changes

```text
AUDIT-CREATED CHANGES:
    NONE
```

# 5. Exact live sections consulted

Primary owner chain in [ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md):

- §§9.4, 9.6, 9.9–9.10 — transaction state, `CommandId`, statement snapshots.
- §§11.2–11.13 — tuple-write claims, unique-key locks, current-owner checks, waits and deadlocks.
- §12.12 — WAL append and runtime persistent publication.
- §§15.1.2, 15.2–15.4, 15.7 — affected counts, DML writes, retry and external output.
- §§19.20.2–19.20.3 — semantic provenance and deterministic ordinary errors.
- §§20.13–20.14 — DML bags, hidden slots, subqueries and demand.
- §§21.13–21.16.1 and 21.20 — UPDATE/DELETE/RETURNING and ordinary DML candidate precedence.
- §§24.1–24.10 — row staging, memory, spill and cleanup.
- §25.1.2 — DML expression-error handoff.
- §§26.3–26.4 — completion, readiness, error and result handoff.
- §§30.5–30.8 — post-readiness spill-source behavior, for comparison only.
- §§31.1–31.10 — target materialization, physical DML, `RETURNING`, result interface.
- §§39.1.2–39.1.7 and 39.3 — first-write boundary, statement outcomes, session loss and execution errors.
- §§41.3 and 41.5 — transaction and physical-execution verification requirements.

# 6. N31-1 conflict restatement

The conflict is real and accurately reported.

[§21.16.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:18593) requires ordinary multi-row DML candidates to be reduced by canonical semantic provenance. Physical RID order, batch order, worker order, spool order and visitation order must not select the public error or change its transaction consequence. Before publishing a write that would make a higher-precedence reachable candidate produce a different §39 consequence, execution must establish that no such candidate remains.

[§39.1.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27528) defines the first persistent statement write as transaction-owned WAL-backed database mutation crossing the §12.12 publication boundary. Locks, temporary spills, candidate construction and WAL reservation do not count.

[§39.1.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27577) distinguishes:

- before first write: recoverable ordinary error → `FA`, explicit transaction may remain `ACTIVE`;
- after first write: statement failure → `MA`, transaction becomes `MUST_ABORT` or is automatically aborted.

[§39.1.4](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:27618) nevertheless gives an explicit example where rows 1–4 publish and row 5 later encounters a conversion, arithmetic or constraint error, producing `MA`.

Meanwhile, [§§31.6–31.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:22909) permit row/batch-at-a-time validation and publication. Thus two conforming execution shapes can discover the same later ordinary error on opposite sides of the first-write boundary.

# 7. N31-1 scenario matrix

| Scenario | Prevalidate/close first | Publish-as-visited | Observable conflict |
|---|---|---|---|
| INSERT VALUES: row 2 divides by zero | Error before `W`; `FA`; explicit transaction remains `ACTIVE` | Row 1 crosses `W`; row 2 errors; `MA` | Same SQL/data, different transaction state |
| INSERT SELECT: source errors after valid rows | Entire demanded source is consumed before `W`; `FA` | Earlier target rows publish; later source error gives `MA` | Streaming shape changes outcome |
| UPDATE: target B `SET` error | All target assignments close before `W`; no target mutated; `FA` | Target A mutates first; target B error gives `MA` | Spool/mutation order changes outcome |
| UPDATE: later NOT NULL failure | Candidate established before `W`; `FA` | Earlier target mutates; later failure gives `MA` | Batch order changes outcome |
| UPDATE: later `RETURNING` error | All demanded `RETURNING` expressions close before `W`; `FA` | Earlier mutation crosses `W`; later result expression gives `MA` | Result-expression timing changes outcome |
| Existing-owner UNIQUE violation | All relevant locks/current-state checks before `W`; `FA` if ordinary candidate wins | Earlier unrelated row may publish first; violation becomes `MA` | Lock/check order changes transaction state |
| Same-statement duplicate unique key | Pending-owner set detects duplicate before `W` | Earlier occurrence may become a published current-command owner | Same semantic violation crosses different boundary |
| Deadlock during unique-lock acquisition | Dynamic failure; `MA` under the live matrix | Same | Not source-span ranked |
| OOM/cancellation | `FA` before `W`, `MA` after `W` | Same owner rule | Actual occurrence stage remains relevant |

# 8. N31-1 alternative comparison

| Alternative | Deterministic result | Preserves existing `FA` semantics | Main consequence | Assessment |
|---|---:|---:|---|---|
| **A. Ordinary-candidate closure barrier** | Yes | Yes | Statement-wide validation/staging before `W` | Strongest fit with Chapters 21 and 39 |
| **B. Canonical publication schedule** | Only if a canonical occurrence order exists | Partly | Creates internal semantic row order | Not generally feasible for unordered bags |
| **C. All ordinary write-DML failures abort** | Yes | No | Retains streaming but makes pre-write errors transaction-fatal | Simpler, but broad semantic change |
| **D. Permit physical-timing variation** | No | N/A | Same statement can leave `ACTIVE` or `MUST_ABORT` | Reject |

Alternative B cannot be made general without inventing an order for `INSERT SELECT` and UPDATE/DELETE target bags. RID, scan, spool or hash order would become observable through transaction state even if it were not exposed as row order.

Alternative C is coherent, but changes the established meaning of a recoverable pre-write DML error. Even a single-row statement that writes nothing could poison an explicit transaction.

Alternative D directly violates Chapter 21’s deterministic-error and physical-substitutability objective.

# 9. N31-1 ordinary/dynamic error-class table

| Error/failure | Ordinary candidate? | Included in A closure? | Before `W` | After `W` |
|---|---:|---:|---|---|
| Scalar conversion | Yes | Yes | `FA` | Unreachable as newly discovered ordinary work under A |
| Arithmetic error | Yes | Yes | `FA` | Same |
| UPDATE assignment/coercion | Yes | Yes | `FA` | Same |
| Runtime NOT NULL | Yes | Yes | `FA` | Same |
| CHECK | Not a v1 runtime constraint | N/A | N/A | N/A |
| Immediate UNIQUE violation | Yes | Yes, with required lock/current-state protocol | `FA` | Unreachable as newly discovered ordinary work under A |
| `RETURNING` expression error | Yes | Yes | `FA` | Unreachable after successful closure |
| INSERT SELECT source scalar/subquery error | Yes when semantically demanded | Yes | `FA` | Unreachable after closure |
| READ COMMITTED revalidation conflict | No; dynamic conflict | No | Retry or `FA` per owner | `MA` |
| REPEATABLE READ conflict | No | No | `MA` | `MA` |
| Deadlock | No | No | `MA` | `MA` |
| Serialization failure | No | No | `MA` | `MA` |
| OOM | No | No | `FA` | `MA` |
| `SpillIOError` | No | No | `FA` | `MA` |
| Cancellation | No | No | `FA` | `MA` |
| Persistent corruption/invariant failure | No | No | `NC`/fatal owner | `NC`/fatal owner |

Alternative A closes deterministic ordinary semantics; it does not pre-rank or suppress dynamic failures.

# 10. N31-1 Alternative-A staging requirements

Before `W`, the final attempt must complete all semantically demanded work capable of producing an ordinary candidate:

- Consume the complete demanded DML input or target set.
- Evaluate conversions, assignments, scalar/subquery expressions and `RETURNING`.
- Form complete candidate rows.
- Check runtime NOT NULL and supported row constraints.
- Derive every relevant unique key.
- Acquire the necessary UNIQUE_KEY locks in canonical order and perform current-state checks.
- Detect same-statement duplicate unique keys through an exact pending-owner set.
- Reduce all ordinary candidates to the Chapter-21 winner.
- Retain mutation-ready rows and result values with stable ownership.

This does not require retaining redundant representations, but it does require a complete, spillable staging phase.

# 11. INSERT VALUES implications

Before `W`, stage:

- every converted target row;
- exact NULL and VARCHAR representations;
- ordinary candidate provenance;
- unique-index keys and pending same-statement owners;
- demanded `RETURNING` values;
- eventual affected-row occurrences.

Tuple encoding and key derivation may remain vectorized and batched. Persistent heap/index publication cannot begin until closure succeeds.

# 12. INSERT SELECT implications

Alternative A requires consuming the entire semantically demanded source before publication.

Consequences:

- streaming source-to-target publication is not legal;
- source expression/conversion/subquery errors close before `W`;
- candidate target rows require a spill-capable staging collection;
- source occurrence multiplicity and candidate order remain nonsemantic;
- source side-plan state must finish consistently with Chapter 20 demand;
- self-reading INSERT cannot feed newly inserted rows because no insert occurs during source consumption.

This is the largest cost introduced by Alternative A.

# 13. UPDATE implications

The existing target spool supplies the target identity/Halloween barrier, but closure must additionally prepare, for all final targets:

- exact revalidated old-row authority;
- simultaneous assignment evaluation;
- complete candidate new rows;
- NOT NULL state;
- old/new unique keys;
- demanded `RETURNING` values;
- ordinary error candidates.

Mutation-ready state may extend the target spool or use another `RowCollection`. Revalidation and lock waits must not permit stale pre-wait row images to enter this state.

# 14. DELETE implications

DELETE already has a complete target spool and normally has no assignment or new-row constraint phase.

Under Alternative A:

- demanded DELETE `RETURNING` expressions must be evaluated and staged before `W`;
- retained old-row values must belong to the exact target version authorized for deletion;
- without `RETURNING` or another ordinary candidate-producing expression, no artificial candidate-row staging is required;
- dynamic target conflicts may still occur during the write phase.

# 15. RETURNING implications

`RETURNING` expressions are ordinary row-semantic candidates and therefore participate in N31-1 closure.

Under Alternative A:

- all demanded `RETURNING` expressions are evaluated before `W`;
- their rows are retained in the statement result spool;
- no client publication occurs during closure;
- after mutation, the spool remains unpublished until statement success;
- the spool serves both as pre-write candidate staging and eventual result storage.

A later result-spool I/O failure before boundary `R` remains an execution failure governed by `W`.

# 16. UNIQUE-lock implications

Alternative A requires all relevant unique-key checks to become authoritative before tuple publication. That means:

- derive the finite statement-wide key set;
- acquire UNIQUE_KEY locks in canonical `(IndexId, encoded key)` order;
- recheck current ownership after waits;
- use exact pending statement owners for duplicates among staged inserts/updates;
- exclude only the exact UPDATE old RID;
- retain immediate, not deferred-final-state, key-swap semantics.

Without retaining the locks through publication, another transaction could invalidate the precheck. Therefore the locks must be acquired before `W` and remain transaction-owned as Chapter 11 requires.

Costs include a larger lock footprint, earlier acquisition, longer lock duration and increased contention. Canonical ordering limits deadlock opportunities but does not eliminate dynamic deadlock outcomes.

# 17. READ COMMITTED retry implications

Alternative A composes cleanly with the existing retry boundary:

- revalidation conflicts while `W == false` may initiate an authorized same-`TxnId` retry;
- retry discards staged rows, target spool, pending unique owners, ordinary candidates, provisional count and `RETURNING` spool;
- the retry captures a fresh statement snapshot;
- immutable plan/descriptors may be reused under their owner;
- transaction-duration locks persist only where existing lock ownership requires;
- no retry is permitted after `W`.

Closure may discover more retry-relevant conflicts before publication, but does not extend retry past the canonical boundary.

# 18. N31-1 performance matrix

| Dimension | A: closure | B: canonical schedule | C: always abort |
|---|---|---|---|
| Correctness clarity | High | Low–medium | High |
| Implementation complexity | High | High | Low–medium |
| Memory pressure | High | Medium | Low |
| Spill complexity | High | Medium | Low |
| Lock footprint | High | Medium | Low–medium |
| Latency to first write | High | Medium | Low |
| Streaming INSERT SELECT | Lost | Order-dependent/problematic | Preserved |
| Vectorization friendliness | High during staging | Medium | High |
| Error reproducibility | High | Fragile | High |
| Existing semantic compatibility | High | Low | Low |
| Educational value | High | Medium | Medium |
| Performance cost | High | Medium | Low |

# 19. N31-1 recommendation

**Recommend Alternative A: ordinary-candidate closure before the first persistent statement write.**

It most faithfully preserves the already frozen Chapter-21 deterministic error contract and Chapter-39 pre-write `FA` semantics.

Required closure includes:

- demanded source/conversion/scalar errors;
- UPDATE assignment errors;
- runtime NOT NULL;
- immediate UNIQUE violations;
- demanded `RETURNING` errors;
- any future supported ordinary row constraint.

It excludes dynamic deadlock, conflict, OOM, spill, cancellation and storage failures.

The consequence is full spillable staging for `INSERT SELECT`, mutation-ready new-row staging for UPDATE, precomputation of `RETURNING`, and statement-wide canonical unique-lock acquisition. Existing `RowCollection`, `SpillManager`, target spool, `RETURNING` spool, memory manager and pending unique-owner mechanisms provide most infrastructure, but a distinct DML closure phase is still required.

The cost is high but follows directly from the project’s chosen combination of:

- no physical statement undo;
- pre-write recoverability;
- deterministic ordinary errors;
- transaction consequences independent of execution shape.

# 20. N31-1 minimal edit surface

For Alternative A:

- §11.9 and §§11.10.5–11.10.7 — statement-wide key acquisition and pending-owner composition.
- §§15.2–15.4 — prepublication DML handoff.
- §15.7 — retry interaction with staged closure.
- §§21.13–21.16.1 and §21.20 — make closure explicit.
- §§31.6–31.9 — insert/update/delete/`RETURNING` physical phases.
- §§39.1.3–39.1.4 — remove or narrow the ordinary row-5 post-write example; retain dynamic post-write outcomes.
- §§41.3 and 41.5 — verification obligations.

Alternative B would additionally require Chapter 20 to define canonical DML occurrence order.

Alternative C would require broader changes to §§15.1.2, 15.7, 21.16.1, 31.6–31.9, 39.1.2–39.1.4 and corresponding verification rules.

# 21. N31-2 gap restatement

Scenario:

```sql
BEGIN;
DML ... RETURNING ...;
```

All mutations finish, the statement publishes successful completion, the transaction remains `ACTIVE`, and the client receives one result chunk. A later read from the spilled `RETURNING` spool fails.

The live text defines failures during spool construction/finalization and establishes chunk-prefix non-retraction, but it does not say whether this later failure:

- retroactively fails the DML statement and therefore makes the transaction `MUST_ABORT`; or
- is a result-delivery failure after an already-final statement success, leaving the transaction `ACTIVE`.

That is a correctness-visible transaction-state ambiguity.

# 22. N31-2 ownership-handoff analysis

The clean ownership transition is:

**Boundary `R`: atomic publication of the successful statement-result envelope and transfer of its cursor/spool to the result/request owner.**

`R` is not:

- merely internal `Finalize`;
- cursor allocation before success;
- the first `Next()` call;
- first returned row.

Before `R`:

- the spool remains statement/attempt-owned;
- construction, finalization or validation failure is statement execution failure;
- the `W` boundary determines `FA` versus `MA`;
- no result/count authority has been published.

After `R`:

- DML statement success is final;
- affected count is final;
- the spool is result-cursor/request-owned;
- later spool or transport failure terminates delivery;
- it does not reopen DML execution, ordinary-candidate reduction or retry.

# 23. N31-2 explicit-transaction matrix

| Case | Alternative A: delivery failure | Alternative B: retroactive failure |
|---|---|---|
| No spill; all rows delivered | Statement successful; transaction `ACTIVE`; count authoritative | Same |
| Chunk 1 delivered; chunk 2 read fails | Statement remains successful; cursor error; prefix retained; transaction `ACTIVE` | Statement becomes failed; `MUST_ABORT`; prefix cannot be retracted |
| First post-success read fails | Successful statement; no row prefix; count authoritative; transaction `ACTIVE` | Retroactive failure and `MUST_ABORT` |
| Count delivered before rows; later read fails | Count remains authoritative | Previously authoritative count would be revoked |
| Rows delivered; count scheduled at EOS | Count remains a semantic fact but may be undelivered | Statement failure conflicts with already delivered rows |
| Client abandons cursor | Successful statement; result resources cleaned; transaction remains `ACTIVE` | Requires an arbitrary rule about whether abandonment aborts |

Under Alternative A, later statements and `COMMIT` remain eligible if the session remains usable. Requiring the old cursor to be closed before another command is an API sequencing choice, not transaction semantics.

# 24. N31-2 autocommit matrix

| Post-publication event | Transaction | Statement execution | Delivery |
|---|---|---|---|
| Complete delivery | `COMMITTED` | Successful | Complete |
| First spool read fails | `COMMITTED` | Successful | Failed before rows |
| Later spool read fails | `COMMITTED` | Successful | Prefix followed by cursor error |
| Transport failure | `COMMITTED` | Successful | Client observation uncertain |
| Client disconnect | `COMMITTED` | Successful | Delivery abandoned |

Successful autocommit result publication already follows the required commit boundary. No post-publication failure can undo the transaction.

Alternative B therefore cannot apply symmetrically to autocommit; it effectively collapses into Alternative A after commit.

# 25. N31-2 SELECT comparison

A SELECT can return a prefix and later fail because successful query completion has not yet been established.

DML `RETURNING` after `R` is different:

- the DML statement has already reached final successful completion;
- its persistent effects and affected count are established;
- the remaining activity is delivery of an already-owned result.

Under the recommendation, `RETURNING` delivery behaves like a result cursor after `R`, but its prior statement success remains final.

# 26. N31-2 Chapter-30 comparison

Chapter 30 permits:

- sort readiness;
- emitted prefix;
- later spill-read error;
- failed overall query completion.

That does not itself settle N31-2 because sort readiness is not successful DML statement publication.

Chapter 30 is useful precedent for:

- cursor failures after readiness;
- non-retraction of already returned rows;
- separation of temporary-spill integrity from persistent database state.

N31-2 additionally needs the explicit `R` ownership transition. After `R`, Alternative A adopts the same monotonic result-delivery principle without treating sort readiness as statement success.

# 27. N31-2 affected-count assessment

Under Alternative A, the affected-row count becomes a final semantic fact at `R`.

If later delivery fails before the client receives it:

- the count remains authoritative inside the completed statement result;
- delivery uncertainty does not alter database semantics;
- the API must report a cursor/result-delivery error, not claim that the DML was unexecuted.

This best matches Chapter 15’s successful statement-result envelope.

# 28. N31-2 retry/client-uncertainty assessment

After an Alternative-A post-`R` failure:

- no internal DML retry is legal;
- the client must not blindly re-execute the DML as though it failed;
- in an explicit transaction, the client may continue, commit or roll back if the session remains usable;
- connection/session loss may independently abort an `ACTIVE` explicit transaction under §39.1.7;
- autocommit remains committed;
- already delivered rows remain historical observations.

This is an “operation succeeded; result delivery failed or is incomplete” condition.

# 29. N31-2 recommendation

**Recommend Alternative A: classify failures after `R` as result-delivery failures.**

Exact rule:

- `R` occurs when successful statement completion and its result envelope are authoritatively published and the cursor/spool ownership is transferred.
- Explicit transaction remains `ACTIVE`.
- Autocommit is already `COMMITTED` before `R`.
- Affected count remains final.
- Returned prefix remains returned.
- Cursor terminates with an error, not `FINISHED`.
- No statement retry or retroactive abort occurs.
- Post-`R` cancellation is result-delivery cancellation.
- Session loss may separately abort an explicit `ACTIVE` transaction.
- The public error may still be named `SpillIOError`; ownership stage, not necessarily the enum, determines transaction consequence.

# 30. N31-2 minimal edit surface

For Alternative A:

- §15.1.2 — affected-count authority at successful result publication.
- §15.7.3 — successful external result handoff.
- §§31.9–31.10 — execution-owned versus result-owned spool and post-`R` failures.
- §§26.3.1–26.3.2 — result handoff cross-reference.
- §§39.1.3–39.1.4 — make clear that `FA`/`MA` applies to the executing statement before `R`, not later delivery.
- §§41.3 and 41.5 — deterministic post-success spool-read verification.

Alternative B touches the same sections but would have to redefine published statement success as revocable.

Alternative C additionally requires a strong spool prevalidation/rematerialization contract in Chapters 24 and 31.

# 31. Recommended combined DML lifecycle

1. **Materialize input/targets.**
   Build the exact DML occurrence or target domain.

2. **Close ordinary candidates.**
   Evaluate demanded source, assignments, constraints, unique keys and `RETURNING`; stage complete values; acquire authoritative unique locks/checks.

3. **Reduce ordinary errors.**
   If a canonical ordinary winner exists, fail before `W`; no persistent user-DML write exists.

4. **Execute persistent mutation.**
   Perform required dynamic revalidation and publish tuple/index state. Dynamic failure after `W` produces the existing abort consequence.

5. **Complete statement.**
   Finish all mutations; freeze affected count; finalize the `RETURNING` spool.

6. **Publish success.**
   - Explicit transaction: cross `R`; transaction remains `ACTIVE`.
   - Autocommit: cross `C`, then `R`.

7. **Deliver result.**
   Post-`R` cursor errors affect delivery only and cannot reopen statement execution.

# 32. W/R/C boundary table

| Boundary | Meaning | Consequence |
|---|---|---|
| `W` | First transaction-owned WAL-backed database mutation crosses §12.12 publication | Ends same-`TxnId` retry; later execution failure normally causes `MA` |
| `C` | Required autocommit terminal commit publication is complete | Mutation is committed and irrevocable |
| `R` | Successful statement-result envelope is published and result ownership transfers | Later cursor/spool failures are delivery failures |

Ordering:

```text
Explicit transaction:
ordinary closure -> W -> statement/spool completion -> R -> optional later C

Autocommit:
ordinary closure -> W -> statement/spool completion -> C -> R
```

For a no-row DML result, `R` still publishes command completion and affected-count metadata.

# 33. Consolidated error ownership table

| Phase/error | Transaction state | Statement status | Client-visible result | Retry |
|---|---|---|---|---|
| Ordinary candidate before `W` | Explicit remains `ACTIVE` (`FA`) | Failed | No successful result | No, except independently authorized conflict retry |
| RC conflict before `W` | Remains eligible | Attempt abandoned | Nothing exposed | Yes, fresh attempt |
| RR/serialization conflict before `W` | `MUST_ABORT`/abort | Failed | None | No |
| Deadlock before `W` | `MUST_ABORT`/abort | Failed | None | No |
| OOM/spill/cancel before `W` | `FA` where recoverable | Failed | None | No |
| Dynamic failure after `W`, before `R` | `MA` | Failed | No successful result | No |
| Cancellation after `W`, before `R` | `MA` | Failed | None | No |
| `RETURNING` construction/finalization failure before `R` | `MA` when `W` crossed | Failed | None | No |
| Explicit post-`R` spool-read failure | Remains `ACTIVE` | Historically successful | Cursor error; prefix retained | No |
| Explicit post-`R` transport failure, session usable | Remains `ACTIVE` | Successful | Delivery uncertain | No |
| Explicit post-`R` session loss | Aborted by session-loss owner | Statement remains historically successful | Delivery uncertain | No |
| Autocommit post-`C`/`R` spool failure | `COMMITTED` | Successful | Cursor error/prefix | No |
| Autocommit post-`C`/`R` transport loss | `COMMITTED` | Successful | Delivery uncertain | No |
| Persistent corruption | `NC`/fatal owner | Failed as applicable | Error | No |

# 34. Combined performance consequence

Under N31-1 Alternative A:

- scalar evaluation, row formation, key derivation and `RETURNING` remain vectorizable;
- every write DML becomes blocking before its first persistent write;
- `INSERT SELECT` loses streaming target publication;
- UPDATE reuses the target spool but requires mutation-ready staged new rows;
- DELETE usually adds only `RETURNING` staging;
- staging may spill through Chapter 24;
- unique locks are acquired earlier and retained longer;
- latency to the first write increases;
- staging may cause double I/O: spill candidate rows, then read them for mutation;
- physical mutation can remain batched after closure;
- no new general undo subsystem is required.

N31-2 Alternative A adds little execution cost. It primarily introduces a clear ownership transition and terminal cursor-error state.

# 35. Educational/project-scope assessment

| Mechanism | Classification | Reason |
|---|---|---|
| N31-1 statement-wide candidate closure | **JUSTIFIED ADVANCED** | Expensive and stricter than many systems, but required by the project’s deterministic `FA`/`MA` policy without undo |
| Spillable DML staging | **JUSTIFIED ADVANCED** | Makes bounded-memory closure implementable using existing database mechanisms |
| Statement-wide unique-lock preparation | **JUSTIFIED ADVANCED** | Teaches lock-before-publication and current-state uniqueness |
| N31-2 execution/result ownership transfer | **CORE** | Fundamental distinction between database success and delivery success |
| Non-retractable result prefix | **CORE** | Required for any practical cursor/result interface |
| Full future-result rematerialization | **POSSIBLE OVERENGINEERING** | High I/O/memory cost without eliminating all future delivery failures |

# 36. Rejected alternatives

The project should explicitly reject:

- physical-order-dependent `ACTIVE` versus `MUST_ABORT`;
- hidden RID, scan, spool, worker or hash order as semantic DML order;
- general physical user-DML statement undo, which v1 does not provide;
- pretending a published mutation never crossed `W`;
- source-span ranking of deadlock, OOM, cancellation or storage failures;
- same-`TxnId` retry after `W`;
- retracting already returned result rows;
- retroactively revoking a published affected count;
- rolling back an already committed autocommit transaction because result delivery failed;
- requiring all future spill reads to be incapable of failure before exposing a cursor;
- treating cursor abandonment as retroactive DML failure.

# 37. N31-1 DECISION CARD

N31-1 DECISION CARD

Question:
What must be established before the first persistent user-DML write so ordinary error selection and transaction consequence cannot depend on physical visitation?

Option A:
Close and canonically reduce every semantically demanded ordinary DML candidate before the first persistent statement write.

Option B:
Define a canonical publication order for all DML occurrences and validate/publish in that order.

Option C:
Make every ordinary runtime error from a write-capable DML statement transaction-fatal, whether or not a persistent write occurred.

Recommended:
Option A.

Why:
It preserves Chapter 21’s deterministic candidate ordering and Chapter 39’s recoverable pre-write failure semantics without adding unsupported physical undo or hidden row order.

Implementation consequence:
All DML becomes blocking before `W`; `INSERT SELECT` requires complete spillable staging; UPDATE stages complete candidate rows; demanded `RETURNING` is precomputed; relevant unique locks/checks cover the complete candidate domain.

Architecture edit surface:
§§11.9–11.10, 15.2–15.7, 21.13–21.16.1, 21.20, 31.6–31.9, 39.1.3–39.1.4 and §§41.3/41.5.

Verification consequence:
Add deterministic multi-row tests proving ordinary candidates close before `W`, dynamic failures remain occurrence-owned, INSERT SELECT is fully staged, unique locks are authoritative, and physical visitation cannot change `FA`/`MA`.

# 38. N31-2 DECISION CARD

N31-2 DECISION CARD

Question:
What is the transaction and statement consequence when a spilled `RETURNING` cursor fails after successful explicit-transaction statement publication?

Option A:
Treat it as result-delivery failure: statement success and affected count remain final; transaction remains `ACTIVE`; the cursor terminates with error.

Option B:
Retroactively fail the DML statement and make the transaction `MUST_ABORT`, despite already published success or returned rows.

Option C:
Require complete pre-reading, validation or failure-free rematerialization of the result before successful publication.

Recommended:
Option A.

Why:
It creates one monotonic ownership transition, preserves already published facts, aligns explicit and autocommit delivery behavior, and avoids disproportionate whole-result rereading.

Implementation consequence:
At `R`, transfer the finalized spool from statement ownership to cursor/request ownership. Post-`R` spill and transport errors do not re-enter DML retry or first-write consequence logic.

Architecture edit surface:
§15.1.2, §15.7.3, §§26.3.1–26.3.2, §§31.9–31.10, §§39.1.3–39.1.4 and §§41.3/41.5.

Verification consequence:
Add first-read and later-read spill failures before and after `R`, explicit/autocommit transaction-state checks, count-authority checks, prefix non-retraction and session-loss separation.

# 39. MINOR-1 deferred cleanup record

The later fix campaign should replace or remove:

- “unless a later architecture explicitly defines...”
- “It may later expose progress/debug result chunks...”
- “The baseline implementation is single-coordinator.”

These are chronology/document-role defects. No user policy choice is needed.

# 40. Recommended post-decision fix sequence

**FIX A:** Low-risk chronology and document-role cleanup, including the deferred MINOR-1 phrases.

**FIX B:** Synchronized N31-1 Architecture repair across ordinary candidate ownership, physical DML staging, first-write consequences and verification hooks.

**FIX C:** N31-2 result-ownership repair defining boundary `R`, count authority and post-success cursor failures.

**FIX D:** Read-only Chapter-31 final Architecture closure audit.

**FIX E:** Chapter-31 Verification synchronization.

# 41. Explicit confirmation

```text
ARCHITECTURE NOT MODIFIED
VERIFICATION NOT MODIFIED
DEVELOPMENT NOT MODIFIED
PROJECT_STATE NOT MODIFIED
IMPLEMENTATION NOT STARTED
NO BUILD/TEST/SANITIZER/BENCHMARK
NO STAGING/COMMIT
AUDIT-CREATED CHANGES = NONE
```

CHAPTER 31 AWAITS PROJECT-OWNER DECISIONS:
    N31-1
    N31-2