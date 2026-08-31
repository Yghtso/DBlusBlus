# Chapter 21 review verdict

**CHAPTER 21 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED**

Chapter 21 is strong on DDL publication, storage/catalog lifetime, transaction envelopes, uniqueness, and cross-layer integration. However, four blocking semantic gaps permit divergent persistent state or user-visible errors:

1. General `UPDATE` enforcement of non-primary-key `NOT NULL` columns is undefined.
2. `DROP INDEX` behavior for a constraint-owned backing index is undefined.
3. `CREATE INDEX` scans only “committed” rows, contradicting the explicitly legal same-transaction DDL → DML → DDL sequence.
4. Error selection across unordered multi-row DML occurrences is undefined.

Two additional major policy gaps affect observable behavior:

5. `RETURNING` row ordering is unspecified.
6. Same-transaction DROP/recreate name revalidation is ambiguous.

Counts:

| Severity | Count |
|---|---:|
| BLOCKING | 4 |
| MAJOR | 2 |
| MINOR | 3 |
| EDITORIAL | 1 |

No files were modified.

## Scope and boundaries

Primary text read: [docs/ARCHITECTURE.md](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:17848).

- Chapter number: 21
- Exact title: **DDL/DML Semantic Planning and SQL v1 Scope**
- Heading starts: line 17848
- Last Chapter-21 content: line 18599
- Review extraction boundary: lines 17848–18601
- Line 18600: `# Part VI — Physical Execution`
- Chapter 22 starts at line 18602: **Physical Plan and Runtime Operator Model**

Context consulted:

- Architecture front matter
- Chapters 4, 7–17, 19–20
- Chapter 22 boundary and §§22.1–22.4 only
- Chapter 31 DML/DDL/result contracts
- Chapter 34 where needed for ANALYZE
- §39 error ownership
- §41.4 verification obligations
- Relevant portions of [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:6615)
- Live `AGENTS.md`

Neither review artifacts nor archival review text were read.

## Complete heading inventory

| Section | Exact heading | Principal responsibility |
|---|---|---|
| 21 | DDL/DML Semantic Planning and SQL v1 Scope | Upper DDL/DML semantic integration and v1 SQL scope |
| 21.1 | Scope | Chapter ownership and handoffs |
| 21.2 | Conservative DDL concurrency model | Database-wide schema exclusivity |
| 21.2.1 | Target-table writer gate | DDL/DML target-table exclusion |
| 21.3 | Catalog visibility during binding | Snapshot-qualified catalog lookup |
| 21.4 | Durable catalog-object IDs | Allocation timing and nonreuse handoff |
| 21.5 | DDL private physical resources | Private/final/orphan file lifecycle integration |
| 21.6 | CREATE TABLE | CREATE TABLE semantics |
| 21.6.1 | Binding | Schema and constraint validation |
| 21.6.2 | Execution/publication | File/catalog/cache publication sequence |
| 21.7 | PRIMARY KEY | PK normalization and enforcement |
| 21.8 | CREATE INDEX | Index creation semantics |
| 21.8.1 | Binding | Target/key/index specification |
| 21.8.2 | Offline build protocol | Writer exclusion, build, publication, abort |
| 21.9 | DROP and physical object retirement | Catalog deletion and delayed physical retirement |
| 21.10 | Catalog cache publication | Terminal cache publication and descriptor lifetime |
| 21.11 | INSERT binding | Canonical target-column mapping |
| 21.12 | Default expressions | Closed immutable default semantics |
| 21.12.1 | DefaultValueBlob v1 | Persistent default-value format |
| 21.13 | UPDATE binding/planning | UPDATE logical handoff |
| 21.14 | DELETE binding/planning | DELETE logical handoff |
| 21.15 | RETURNING | DML row images and result envelope |
| 21.16 | Error contract for semantic planning | Front-end category preservation |
| 21.17 | Parser error recovery | Parser/batch recovery policy |
| 21.17.1 | ANALYZE binding and transaction boundary | ANALYZE identities, visibility, publication |
| 21.18 | SQL v1 supported target | Closed supported statement surface |
| 21.19 | Explicitly deferred SQL/front-end features | Unsupported v1 surface |
| 21.20 | Upper semantic-layer invariants | Cross-layer invariant registry |

## Section-by-section review

Codes: `OK` = sufficient; `F-B` = blocking finding; `F-M` = major; `F-N` = minor; `E` = editorial. “N/A” means the section does not own that domain.

| Section | Architectural role | Timelessness | Ownership | Depth | Terms | Atomicity | CommandId | DML | DDL | Errors | Durability | Determinism | References | Consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 21.1 | Ownership boundary | OK | OK | Adequate | OK | N/A | N/A | OK | OK | N/A | OK | N/A | Broad but valid | OK | OK |
| 21.2 | DDL serialization | OK | OK | Strong | OK | OK | N/A | N/A | Strong | Strong | Strong | Strong | Precise | OK | OK |
| 21.2.1 | Writer gate | OK | OK | Strong | OK | OK | N/A | Strong | Strong | Strong | Strong | Strong | Precise | Conflict in 21.8.2 | F-B |
| 21.3 | Catalog visibility | OK | OK | Strong | OK | N/A | Clear | N/A | Strong | N/A | N/A | Strong | Implicit Ch9/10 | Same-name revalidation unclear | F-M |
| 21.4 | Durable IDs | OK | Integrated owner | Strong | OK | OK | N/A | N/A | Strong | Strong | Strong | Strong | Precise | OK | OK |
| 21.5 | Private resources | OK | OK | Strong | OK | Strong | N/A | N/A | Strong | Strong | Strong | Strong | Precise | OK | OK |
| 21.6 | CREATE TABLE | OK | OK | Adequate | OK | Strong | N/A | N/A | Strong | Strong | Strong | Strong | Precise | Name recreation unclear | F-M |
| 21.6.1 | CREATE binding | Minor temporal wording | Duplicates Ch19 partly | Adequate | “Initial” vague | N/A | N/A | N/A | Clear | Clear | N/A | Clear | Limited | Compatible | F-N |
| 21.6.2 | CREATE publication | OK | OK | Strong | OK | Strong | N/A | N/A | Strong | Strong | Strong | Strong | Precise | Same-name check ambiguous | F-M |
| 21.7 | PRIMARY KEY | OK | OK | Strong | OK | Strong | Clear | Strong | Strong | Strong | N/A | Strong | Precise | OK | OK |
| 21.8 | CREATE INDEX | OK | OK | Strong | OK | Strong | N/A | N/A | Strong | Strong | Strong | Intended strong | Precise | Own writes omitted | F-B |
| 21.8.1 | Index binding | Minor temporal wording | Duplicates Ch19 partly | Strong | “Initial” vague | N/A | N/A | N/A | Strong | Strong | N/A | Strong | Good | OK | F-N |
| 21.8.2 | Offline build | OK | OK | Strong | OK | Strong | Uses current owner | N/A | Strong | Strong | Strong | Strong except row set | Precise | Contradiction | F-B |
| 21.9 | DROP/retirement | OK | OK | Strong | OK | Strong | N/A | N/A | Strong | Incomplete dependency error | Strong | Strong | Precise | Backing-index gap | F-B |
| 21.10 | Cache publication | OK | OK | Strong | OK | Strong | N/A | N/A | Strong | Strong | Strong | Strong | Precise | OK | OK |
| 21.11 | INSERT binding | OK | Ch19 duplication | Adequate | OK | Delegated | Clear | Mostly strong | N/A | Incomplete cross-row order | N/A | Order gap | Good | Compatible | F-N |
| 21.12 | Defaults | OK | Appropriate | Strong | OK | Strong | N/A | Strong | Strong | Strong | Strong | Strong | Precise | OK | OK |
| 21.12.1 | DefaultValueBlob | Mostly timeless | Appropriate | Strong | OK | N/A | N/A | Strong | Strong | Strong | Strong | Strong | Precise | OK | OK |
| 21.13 | UPDATE planning | OK | Ch19 duplication | Thin on constraints | OK | Delegated | Clear | Missing general NN check | N/A | Order gap | N/A | Order gap | Broad Ch15 ref | Incomplete | F-B |
| 21.14 | DELETE planning | OK | Ch19 duplication | Adequate | OK | Delegated | Clear | Clear | N/A | Order gap | N/A | Order gap | Good | Compatible | F-N |
| 21.15 | RETURNING | OK | Appropriate | Strong except order | OK | Strong | Clear | Image clear; order missing | N/A | Strong envelope | Strong | Missing sequence rule | Good | Incomplete | F-M |
| 21.16 | Error contract | OK | Appropriate summary | Adequate | OK | Delegated | N/A | Categories broad | DDL categories broad | Broad | N/A | Does not solve row order | §39 implicit | Compatible | E |
| 21.17 | Parser recovery | Project-time wording | Wrong chapter owner | Thin | “initial” | N/A | N/A | N/A | N/A | Optional policy | N/A | “where practical” | Missing Ch18 ref | No contradiction | F-N |
| 21.17.1 | ANALYZE | OK | Integrated owner | Strong | OK | Strong | Strong | N/A | Maintenance | Strong | Strong | Strong | Precise | OK | OK |
| 21.18 | Supported target | Project chronology | Appropriate inventory | Adequate | “first serious” | N/A | N/A | Clear scope | Clear scope | N/A | N/A | N/A | Good | Semantically fine | F-N |
| 21.19 | Deferred features | Chronology present | Appropriate inventory | Adequate | “future…initial engine” | N/A | N/A | Clear exclusion | Clear exclusion | N/A | N/A | N/A | None needed | Semantically fine | F-N |
| 21.20 | Invariants | Mostly timeless | Appropriate | Strong | OK | Strong | Clear | Reflects gaps | Strong | Delegated | Strong | Partly incomplete | Mostly precise | Cannot close findings | F-B |

## Canonical owner map

| Mechanism | Canonical owner | Chapter-21 role |
|---|---|---|
| Statement admission and CommandId | §§9.4, 9.6 | Consumes |
| RC/RR snapshots | §§9.9–9.10 | Consumes for binding/ANALYZE/DML |
| Current-command tuple visibility | Chapter 10 | Consumes |
| Tuple and unique locks | Chapter 11 | Integrates with DDL gates |
| `SchemaLock`, writer/publication gates | §11.13 plus §21.2 | Chapter 21 defines DDL usage |
| WAL publication | Chapter 12 | Delegates |
| Recovery and durable ID allocators | Chapter 13 | Delegates allocation/recovery |
| Object retirement coordination | §14.17.1 | Integrates DROP/ANALYZE |
| DML physical mutation/count/retry | Chapter 15 | Delegates |
| Catalog rows, IDs, SchemaVer, cache | Chapter 16 | Integrates publication |
| Scalar/default/coercion semantics | Chapter 17 | Delegates |
| Syntax/raw AST | Chapter 18 | Referenced; §21.17 is misplaced |
| Binding namespaces and errors | Chapter 19 | Chapter 21 partly duplicates |
| Logical DML children/bag/order/subqueries | Chapter 20 | Consumes |
| DDL publication/default format/v1 scope | Chapter 21 | Owns |
| Physical operator vocabulary | Chapter 22 | Downstream |
| DML target spooling/RETURNING execution | Chapter 31 | Downstream |
| Execution errors | §39.3 | Downstream |
| Transaction consequence | §39.1 | Delegates |
| Verification methodology | `VERIFICATION.md`/§41 | Referenced only |

The principal Chapter-20 → Chapter-21 handoff is:

```text
fully bound, typed logical DML relation
+ hidden target RID where applicable
+ bag/order/subquery/demand semantics
    ->
Chapter-21 statement operation, catalog/constraint/publication integration
```

The principal Chapter-21 → downstream handoff is:

```text
resolved DDL/DML statement operation
+ immutable catalog descriptors/IDs
+ row-image and constraint requirements
+ transaction/publication protocol
    ->
Chapter 22 physical plan vocabulary
    ->
Chapter 31 execution, spooling, mutation, and result delivery
    ->
Chapters 4–16 storage/transaction/catalog durability mechanisms
```

## Statement-attempt, CommandId, and transaction model

| Case | Attempt/CommandId | Provisional effects | Failure/result |
|---|---|---|---|
| Statement admission | One CommandId assigned once | Attempt-local state starts empty | ID consumed on every terminal statement outcome |
| RC pre-write retry | New attempt and fresh statement snapshot; same CommandId | Spools/count/subquery state discarded | Transaction remains `ACTIVE` |
| RR conflict | Same transaction snapshot | No snapshot refresh | Serialization failure; transaction aborts |
| Failure before first published write | Same admitted statement | Temporary/private state unwinds | Explicit transaction may remain `ACTIVE` |
| Failure after first published write | Same attempt; no retry | Physical effects remain as transaction-owned garbage | `MUST_ABORT` → automatic ABORT |
| Explicit-transaction success | Statement completes while transaction remains `ACTIVE` | Result/count may be returned | No durability implication |
| Autocommit success | Statement success followed by implicit COMMIT | Count/RETURNING withheld through C4–C5 | Published only after coherent terminal commit |
| Commit failure | Governed by §39.1.5 | No false success envelope | Outcome depends on exact C0–C6 boundary |

Statement atomicity is therefore semantic rather than physical: a failed multi-row DML statement cannot partially commit, but already-published bytes are not physically undone. If publication occurred, the entire transaction aborts, including prior successful statements in that explicit transaction.

Locks acquired by failed statements remain transaction-owned until C5/A3. This is consistent with Chapters 11 and 15.

## DML semantic review

### INSERT

| Topic | Live result |
|---|---|
| Source mapping | Explicit source values map to one canonical target-column order |
| Omitted columns | Persisted typed default when present; otherwise typed NULL when legal; impossible NOT NULL omission rejected |
| Coercion | Closed §17.8.5 assignment matrix |
| Defaults | Closed immutable expressions folded once during DDL; INSERT consumes stored typed constant |
| Descriptor | Binding uses snapshot-visible immutable descriptor, including own earlier DDL |
| Version creation | New RID; `xmin/cmin`; invalid `xmax`; canonical `cmax=0` |
| Indexes | Heap redo precedes all referring index MTRs |
| Uniqueness | Immediate; same-command prior rows are owners |
| `RETURNING` image | Final inserted/new row |
| Affected rows | One per successful logical input row in final successful attempt |
| Self-reference | Source uses containing snapshot/CommandId; current-command inserted versions are not rediscovered |
| Order | Returned row order is undefined—major finding |
| Error selection | Multiple unordered row failures have no canonical precedence—blocking finding |

### UPDATE

| Topic | Live result |
|---|---|
| Target identity | Physical RID preserved through logical plan and finalized spool |
| Duplicate targets | Deduplicated; each RID occurs at most once |
| Stale target | Re-fetch/revalidate after lock; RC retry before publication, abort after; RR abort |
| RHS image | Old target values retained by spool and used to construct complete new tuple |
| Assignment semantics | Effectively simultaneous against the old row; `SET a=b,b=a` swaps |
| Unmentioned columns | Preserve old values |
| Physical version | New RID/version plus old `xmax/cmax`; new index entries; old entries retained |
| No-op update | Counts once and still follows baseline version/index semantics; no optimization is independently authorized |
| Uniqueness | Immediate per target; exact old-RID self-exclusion only |
| Key swap/cycle | Fails; no deferred final-state permutation |
| `RETURNING` | Final new row, once per finalized target |
| Affected rows | One per distinct finalized target, including `SET x=x` |
| NOT NULL | General non-PK enforcement point missing—blocking finding |
| Order/error | Mutation order may vary; error precedence undefined—blocking finding |

### DELETE

| Topic | Live result |
|---|---|
| Target identity | Exact RID in finalized spool |
| Duplicate targets | One occurrence per RID |
| Stale target | Same lock/revalidation/isolation rules as UPDATE |
| Mutation | Sets `xmax/cmax`; no new tuple version |
| Indexes | Entries retained; vacuum removes garbage |
| `RETURNING` | Old row image retained independently of later physical reclamation |
| Affected rows | One per distinct finalized/revalidated logical delete |
| Halloween/rediscovery | Finalized deduplicated target set prevents repeated action |
| Order/error | Same unresolved cross-row error-order issue |

### Row-image matrix

| Operation | Evaluation source | Final image | `RETURNING` image |
|---|---|---|---|
| INSERT | Source values + persisted defaults + assignment coercions | New inserted row | New row |
| UPDATE | Old spooled row; RHS references old target values | Complete replacement row | Final new row |
| DELETE | Old spooled row | Logical deletion of old version | Old row |

### Affected-row matrix

| Case | Count |
|---|---:|
| INSERT N source occurrences successfully inserted | N |
| UPDATE N distinct finalized targets | N |
| UPDATE `SET x=x` | Counts each target |
| Duplicate UPDATE target discovery | One per RID |
| DELETE N distinct finalized targets | N |
| Stale/nonqualifying target skipped | 0 |
| Abandoned retry | 0 contribution |
| Failed statement | No successful count |
| Autocommit commit failure | No successful count publication |

### DML order/determinism matrix

| Input/state | Target action order | RETURNING order | Error rule | Final state |
|---|---|---|---|---|
| Ordered INSERT source | Not explicitly inherited | Undefined | Undefined across rows | Determinate absent errors/constraints |
| Unordered INSERT source | Physical freedom | Undefined | Undefined | Uniqueness result mostly order-independent |
| UPDATE/DELETE target spool | Spool iteration unspecified | Undefined | Undefined | Each target once |
| Duplicate target | Deduplicated | One row | N/A | Determinate |
| Stale target before writes | Whole-attempt retry | Old output discarded | Conflict owner precise | Determinate |
| Stale target after writes | No retry; abort | None | Conflict returned | No partial commit |
| Key swap/cycle | First attempted replacement conflicts under any order | None | `UniqueViolation` category | Abort/no permutation |

## Defaults and constraints

### DEFAULT matrix

| Case | Result |
|---|---|
| Literal/cast/immutable operator default | Fold once during DDL |
| Function default | Unsupported; registry empty |
| Column/subquery/aggregate/parameter | Rejected |
| Assignment coercion | Applied before persistence |
| Nullable `DEFAULT NULL` | Valid |
| NOT NULL `DEFAULT NULL` | Rejected |
| Omitted nullable column without default | Typed NULL |
| Omitted NOT NULL column without default | Rejected |
| Reopen | Decode `DefaultValueBlob`, validate nested `PersistedScalarV1` |
| Per-row reevaluation | Never occurs in v1 |

### Constraint matrix

| Constraint | INSERT | UPDATE | CREATE validation | Status |
|---|---|---|---|---|
| NOT NULL | Explicit runtime check | General check missing | Catalog nullability | **Finding** |
| PRIMARY KEY | NOT NULL then UNIQUE | PK components covered by §21.7 | One PK; normalized constraint/index | Consistent |
| UNIQUE | Any-NULL nonconflict; otherwise immediate | Immediate per target | Backing/standalone unique index | Consistent |
| CHECK | Unsupported | N/A | Rejected | Consistent |
| FOREIGN KEY | Unsupported | N/A | Rejected | Consistent |

### UNIQUE matrix

| Case | Outcome |
|---|---|
| Fully non-NULL duplicate | Conflict |
| Any NULL component | No duplicate conflict |
| Composite partial NULL | No duplicate conflict |
| NaN/NaN | Equal under canonical encoding |
| `+0.0/-0.0` | Equal |
| Same-statement duplicate INSERT | Later occurrence conflicts |
| Multi-row UPDATE collision | Immediate conflict |
| Two-row swap | Fails |
| Three-row cycle | Fails |
| Concurrent uncommitted owner | Wait then full recheck |
| Aborted creator | Ignored |
| Aborted deleter | Old owner still conflicts |
| Own earlier-command live owner | Conflicts |
| Own current-command other row | Conflicts |
| Exact current UPDATE old RID | Self-excluded only for that operation |

## DDL review

### CREATE TABLE

The protocol is otherwise complete:

1. Acquire `SchemaLock`.
2. Revalidate the name.
3. Durably allocate nonreusable identities.
4. Create and initialize private heap/FSM/index files.
5. Durably publish final names.
6. Install transaction-owned MVCC catalog rows.
7. Publish cache entries only after terminal COMMITTED.
8. On abort, catalog rows remain invisible and files become orphan-retirement input.

Initial `SchemaVer` is 1; ColumnIds are 1…N; object/File IDs may be burned by failure.

The unresolved point is whether execution revalidation against “current committed catalog state” includes the current transaction’s earlier self-deletions. That affects same-transaction DROP/recreate.

### CREATE INDEX

The offline protocol correctly provides:

- global schema serialization;
- exclusive target writer gate;
- writer drain;
- private B+ construction;
- exact key encoding;
- uniqueness validation;
- durable final-name publication before catalog mutation;
- manifest revalidation;
- transaction-local then global descriptor publication;
- abort/orphan handling.

Blocking contradiction:

```text
CREATE TABLE t(...);
INSERT INTO t ...;
CREATE INDEX i ON t(...);
COMMIT;
```

is legal according to §§21.2.1 and 11.13: DDL’s retained exclusive writer gate subsumes the intervening DML shared request, and subsequent DDL can reuse ownership. But §21.8.2 step 7 scans only “current logically live committed row versions.” The inserted rows are current logical owners of the same transaction but not committed. A literal implementation omits them and can publish an incomplete committed index.

The build row set must explicitly include all current logical owners that can become part of the DDL transaction’s committed table state, including the owner transaction’s earlier-command rows.

### DROP and lifecycle

`DROP TABLE` is clear:

- table, indexes, constraints form one semantic operation;
- catalog rows become transactionally invisible;
- old snapshots/descriptors remain usable;
- physical unlink waits for snapshot/descriptor/BufferPool ownership drain;
- unlink durability requires directory synchronization;
- abort restores semantic visibility.

`DROP INDEX` is incomplete for a constraint-owned backing index. The architecture permits at least three possible implementations:

1. reject the DROP;
2. drop the owning PK/UNIQUE constraint too;
3. delete only the index row, which §16.5 forbids as invalid catalog state.

No rule selects between the first two legal policies. Because SQL has no `DROP CONSTRAINT` or CASCADE/RESTRICT surface, this requires an explicit architecture decision.

### DDL object-lifecycle matrix

| State | Catalog | File | Cache | Recovery |
|---|---|---|---|---|
| Before ID allocation | None | None | None | Nothing |
| Private construction | None | `PRIVATE_DURABLE` or incomplete orphan | None | Cleanup candidate |
| Final name before catalog | None | `FINAL_DURABLE_UNCOMMITTED` | None | Orphan unless committed owner appears |
| Catalog rows, uncommitted | Self-visible after command | Final file | Transaction-local only | Loser rows become invisible |
| Durable COMMIT | Committed | `CATALOG_COMMITTED` | Install or safe bypass | Reconstructible |
| CREATE abort | Aborted rows | Orphan | No global entry | Cleanup |
| DROP uncommitted | Self-deleted only | Retained | Not globally removed | Abort restores |
| DROP committed | Invisible to new snapshots | `RETIRED_LINKED` | Current-name removal | No resurrection |
| Retirement drain complete | Invisible | Unlink + directory fsync | Old descriptors gone | Durable absence |

### DDL visibility matrix

| Observer | Uncommitted CREATE | Committed CREATE | Uncommitted DROP | Committed DROP |
|---|---|---|---|---|
| Owning transaction, later command | Visible | Visible | Invisible | Invisible |
| Other transaction, newer RC statement | Invisible | Visible after commit | Still visible | Invisible after commit |
| Older RR transaction | Invisible | Not newly visible | Old object remains visible | Old descriptor remains usable |
| Recovery before commit | Loser/invisible | N/A | Original remains | N/A |
| Recovery after durable commit | Visible/reconstructed | Visible | Dropped | Dropped |

### DDL failure matrix

| Failure point | Statement/transaction | Catalog | Physical residue |
|---|---|---|---|
| ID reservation | Pre-publication failure | None | ID gap |
| Private file create/init | May remain active after cleanup transfer | None | Orphan |
| Final-name publication | Failure; no commit | None | Pending/final orphan |
| First catalog tuple publication | Crosses write boundary | Transaction must abort on later failure | Aborted catalog bytes |
| Index build uniqueness failure before catalog | Prepublication failure | None | Orphan |
| Cache install after durable COMMIT | Transaction remains committed | Authoritative | Install, invalidate/bypass, or noncontinuable |
| DROP catalog marker failure before publication | May remain active | Object remains | File remains |
| Failure after DROP marker publication | Mandatory abort | Delete ineffective | File remains |
| Crash before COMMIT | Loser | Invisible | Orphan/retained |
| Crash after durable COMMIT | Committed | Reconstructed | Required files present or retirement resumed |

### Catalog/SchemaVer matrix

| Operation | IDs | Tuple SchemaVer | Cache/publication |
|---|---|---|---|
| CREATE TABLE | New TableId/ConstraintIds/FileIds | 1 | Self-visible after statement; global at commit |
| CREATE INDEX | New IndexId/FileId | Existing table SchemaVer unchanged | Manifest transaction-local then global |
| DROP INDEX | IDs never reused | SchemaVer unchanged | Current-name removal at commit |
| DROP TABLE | IDs never reused | Table identity retired | Current-name removal at commit |
| Failed DDL | Allocated IDs remain consumed | No committed new version | No global publication |

### WAL/durability matrix

| Effect | WAL/durability owner | Requirement |
|---|---|---|
| DML heap mutation | Chapters 12/15 | WAL before page publication/data flush |
| DML index mutation | Chapters 8/12/15 | Heap redo precedes referring B+ MTR |
| Catalog tuple mutation | Ordinary catalog MVCC/WAL | Terminal commit controls visibility |
| Private file initialization | §§4.7, 12 | Durable private bytes before publication |
| CREATE final-name publication | §4.7.4 | File sync and directory sync before catalog commitment |
| DROP retirement | §§4.7.7, 21.9 | Unlink only after drain; directory fsync |
| COMMIT | §15.5 | C3 durable WAL, C4 publication, C5 coherent cleanup |
| Recovery | Chapter 13 | Required committed files present; orphan ownership resolved |
| Second crash | §§4.7, 13.20 | Committed DDL remains reconstructible |

DDL durability and second-crash safety are otherwise complete.

## Error matrix

| Condition | Category/owner | Statement consequence |
|---|---|---|
| Parse/bind/type/default definition | §§19.20, 21.16, 39.2 | Pre-write; explicit transaction may remain active |
| Unique conflict | §11.10 / `UniqueViolation` | FA before first write; MA after earlier row writes |
| NOT NULL INSERT | Constraint violation | Same first-write rule |
| NOT NULL UPDATE | **Enforcement/category point missing** | **Finding** |
| Stale RC target before write | Retry | Fresh snapshot, same CommandId |
| Stale RC target after write | Transaction conflict | Mandatory abort |
| RR stale target | Serialization failure | Mandatory abort |
| DML expression/subquery error | Chapters 17/20/39 | FA or MA by publication boundary |
| DDL namespace failure | §§4.7, 39.1 | FA with owned orphan, MA after catalog write |
| Deadlock victim | §11.13 | Mandatory abort |
| Cache failure after commit | §§21.10, 39.1.5 | Commit remains final; fallback or noncontinuable |
| Multiple unordered DML row errors | **No owner** | **Blocking ambiguity** |

## Cross-reference audit

Every explicit Chapter-21 reference resolves. The table groups repeated references with identical purpose.

| Source | Target | Purpose | Owner/precision | Circular? | Status |
|---|---|---|---|---:|---|
| 21.1 | Chapter 15 | DML mutation ordering | Correct broad owner | No | Good |
| 21.1 | Chapter 20 | Logical semantics | Correct broad owner | No | Good |
| 21.2/21.2.1 | §11.13 | Gate modes/order/deadlocks | Exact | No | Good |
| 21.2 | §39.1.4 | Failed-attempt consequence | Exact | No | Good |
| 21.4 | §13.2.6 | Object-ID allocator | Exact | No | Good |
| 21.4 | §16.3 | ColumnId rules | Exact | No | Good |
| 21.5 | §§4.7.1–4.7.7 | Private/final/orphan lifecycle | Exact | No | Good |
| 21.5 | §§4.7.4, 4.7.6–4.7.7 | Publication/cleanup | Exact | No | Good |
| 21.5 | §39.1.2 | First published write | Exact | No | Good |
| 21.6.2 | §16.5 | Catalog row shapes | Correct, somewhat broad | No | Good |
| 21.7 | §§16.5.4–16.5.6 | PK metadata | Exact range | No | Good |
| 21.7 | §11.10 | Runtime uniqueness | Exact | No | Good |
| 21.8.1 | §14.17.1 | Manifest publication | Exact | No | Good |
| 21.8.1 | Chapter 11 / §11.10.2 | Unique semantics/NULL | Exact after broad intro | No | Good |
| 21.8.1 | §§16.5.4–16.5.5 | Index rows | Exact | No | Good |
| 21.8.2 | §4.7.4 | File publication | Exact | No | Good |
| 21.8.2 | §11.13 | Manifest gate | Exact | No | Good |
| 21.8.2 | §§15.5/15.6 | Terminal release | Exact | No | Good |
| 21.8.2 | §11.10.9 | Unique build | Exact | No | Good |
| 21.9 | §14.17.1 | RETIRING coordination | Exact | No | Good |
| 21.9 | §11.13 | Wait graph | Exact | No | Good |
| 21.9 | §7.12.5 | File/frame drain | Exact | No | Good |
| 21.9 | §4.7.7 | Durable unlink | Exact | No | Good |
| 21.10 | §16.10 | Cache/descriptor lifetime | Exact | No | Good |
| 21.10 | §39.1.5 | Postcommit cache failure | Exact | No | Good |
| 21.11 | §17.8.5 | Assignment coercion | Exact | No | Good |
| 21.11 | §20.14 | Subqueries | Exact domain | No | Good |
| 21.12 | §§17.10.2, 17.8.5 | Folding/coercion | Exact | No | Good |
| 21.12.1 | §§17.13, 4.14.2 | Scalar codec/version dispatch | Exact | No | Good |
| 21.13 | §§17.8.5, 20.14 | UPDATE coercion/subqueries | Exact | No | Good |
| 21.13 | Chapter 15 | UPDATE protocol | Correct but §15.3 preferred | No | Editorial |
| 21.14 | §20.14 | DELETE subqueries | Exact domain | No | Good |
| 21.15 | §20.14.2 | Subquery scope | Exact | No | Good |
| 21.15 | Chapter 15 | Retry | Correct but §15.7 preferred | No | Editorial |
| 21.15 | §31.9 | Result buffering | Exact | No | Good |
| 21.15 | §39.1 | Transaction consequence | Correct broad matrix | No | Good |
| 21.17.1 | §§14.17.1, 11.13 | ANALYZE publication/gates | Exact | No | Good |
| 21.17.1 | §39.1 | Failure boundary | Correct | No | Good |
| 21.18 | §20.14 | Supported subqueries | Exact | No | Good |
| 21.20 | §§17.8.5, 4.7, 20.14, 39.1 | Invariant delegation | Correct | No | Good |

## Documentation-model audit

| Criterion | Result |
|---|---|
| No chronology | Finding |
| No current implementation state | Pass |
| No DEVELOPMENT sequencing | Pass |
| No VERIFICATION recipe | Pass |
| No PROJECT_STATE leakage | Pass |
| No devlog/history | Pass |
| Statement attempt precise | Precise through owners |
| CommandId precise | Precise |
| Statement atomicity precise | Precise |
| Retry precise | Precise |
| INSERT precise | Except cross-row order |
| UPDATE precise | NOT NULL and error order gaps |
| DELETE precise | Except error/order policy |
| RETURNING precise | Image/envelope yes; order no |
| Affected rows precise | Yes |
| DEFAULT precise | Yes |
| Uniqueness precise | Yes |
| DDL visibility precise | Except same-transaction name recreation |
| DDL physical/catalog publication | Strong |
| Failure/rollback precise | Strong |
| WAL/recovery ownership | Strong |
| Ordering/error determinism | Finding |
| Implementation freedom | Generally preserved |
| Cross-references | Good; two editorial broad references |
| Analytical rationale | Strong overall |
| Timelessness | Local cleanup needed |

### Complete temporal-language classification

| Section/phrase | Class | Assessment |
|---|---|---|
| 21.2.1 “later transaction-gate wait” | A/B runtime acquisition order | Valid |
| 21.2.1 “later schema-changing DDL statement” | B transaction statement order | Valid |
| 21.3 “current transaction…earlier DDL statement” | B transaction visibility | Valid |
| 21.5 “later statement failure” | A failure ordering | Valid |
| 21.6.1 “Initial table constraints” | C intended v1 scope, but vague | Minor cleanup |
| 21.6.1 “deferred” constraints | C durable v1 exclusion | Valid |
| 21.6.2 “initial ColumnIds/schema version” | A object-creation state | Valid |
| 21.6.2 “later user COMMIT” | B transaction order | Valid |
| 21.8.1 “Initial index keys” | C intended v1 scope, but vague | Minor cleanup |
| 21.8.1 “deferred” expression indexes | C durable v1 exclusion | Valid |
| 21.8.2 “current live committed rows” | B current-owner semantics | Semantically problematic, not chronology |
| 21.12.1 future blob version | C format evolution boundary | Acceptable but could be more timeless |
| 21.13/21.15 later error | A row/statement runtime order | Valid |
| 21.16 “may later be mapped” | B downstream presentation stage | Valid |
| 21.17 “initial parser” | E project framing | Finding |
| 21.17 “later independent errors” | A batch source order | Valid |
| 21.17.1 current descriptors/rows | B live transaction/object state | Valid |
| 21.17.1 own later statements | B transaction order | Valid |
| 21.18 “intended first serious SQL surface” | E project chronology | Finding |
| 21.19 “future architecture-compatible…initial engine” | E roadmap framing | Finding |

Project-time/current-state wording exists, but no actual implementation-progress narration exists.

### Implementation-coupling audit

No unnecessary implementation coupling was found.

Correctness-relevant physical architecture retained appropriately includes:

- private/final file lifecycle;
- B+ file build and MTR publication;
- target/result spools as downstream semantic boundaries;
- explicit vector-position nonidentity;
- file synchronization barriers.

No source files, C++ classes, helper APIs, allocator layouts, hash-table design, or concrete executor class structure are prescribed. The optional bulk sorted/grouped uniqueness check preserves algorithm freedom.

## Technical consistency matrix — 230 actual questions

Statuses: `C` consistent; `CS` consistent but specialized/delegated; `F` finding; `N/A` unsupported by live v1.

| # | Question/result | Status |
|---:|---|:---:|
| 1 | One admitted statement owns one CommandId? Yes. | C |
| 2 | Internal retries retain it? Yes. | C |
| 3 | Failed statements consume it? Yes. | C |
| 4 | Retry state is attempt-local? Yes. | C |
| 5 | Provisional counts reset? Yes. | C |
| 6 | Subquery state resets? Yes. | C |
| 7 | RC retry gets a fresh snapshot? Yes. | C |
| 8 | RR retry refreshes snapshot? No. | C |
| 9 | Post-write retry is permitted? No. | C |
| 10 | Post-write failure automatically aborts? Yes. | C |
| 11 | Parse/bind failure consumes CommandId? Yes. | C |
| 12 | CommandId zero is legal? Yes. | C |
| 13 | Maximum CommandId is usable once? Yes. | C |
| 14 | IDs can be reused after failure? No. | C |
| 15 | Same-command inserts are ordinary-scan visible? No. | C |
| 16 | Same-command deletes hide old rows from same statement? No. | C |
| 17 | Same-command rows remain uniqueness owners? Yes. | C |
| 18 | DROP/recreate revalidation has one exact self-visibility policy? No. | F |
| 19 | Autocommit receives a distinct command envelope? Yes. | C |
| 20 | Transaction-control termination needs another CommandId? No. | C |
| 21 | INSERT explicit target list maps canonically? Yes. | C |
| 22 | Duplicate targets are rejected? Yes. | C |
| 23 | Omitted defaulted column uses default? Yes. | C |
| 24 | Omitted nullable no-default column uses typed NULL? Yes. | C |
| 25 | Omitted NOT NULL no-default column fails? Yes. | C |
| 26 | Assignment coercion is closed? Yes. | C |
| 27 | Explicit-cast-only conversions stay explicit? Yes. | C |
| 28 | INSERT SELECT shares target mapping? Yes. | C |
| 29 | Target names are rebound at execution? No. | C |
| 30 | Input duplicate rows remain distinct inputs? Yes. | C |
| 31 | Defaults may reference table columns? No. | C |
| 32 | Defaults may contain subqueries? No. | C |
| 33 | Defaults may contain aggregates? No. | C |
| 34 | Defaults may contain functions? No. | C |
| 35 | Immutable operators/casts are allowed? Yes. | C |
| 36 | Default folds once at DDL binding? Yes. | C |
| 37 | Default is coerced before persistence? Yes. | C |
| 38 | INSERT reevaluates default per row? No. | C |
| 39 | Default uses descriptor selected by binding snapshot? Yes. | C |
| 40 | Transaction-local earlier DDL can supply descriptor? Yes. | C |
| 41 | INSERT creates fresh RID/version? Yes. | C |
| 42 | `cmax` invalid encoding is canonical zero? Yes. | C |
| 43 | Heap redo precedes index references? Yes. | C |
| 44 | Runtime INSERT NOT NULL is checked? Yes. | C |
| 45 | PK NULL checks precede duplicate checks? Yes. | C |
| 46 | Same-command duplicate INSERT conflicts? Yes. | C |
| 47 | Failed row after earlier writes aborts transaction? Yes. | C |
| 48 | Successful input occurrence counts once? Yes. | C |
| 49 | INSERT RETURNING uses new image? Yes. | C |
| 50 | INSERT RETURNING order is defined? No. | F |
| 51 | UPDATE identifies target by RID? Yes. | C |
| 52 | RID survives logical planning? Yes. | C |
| 53 | Target set finalizes before mutation? Yes. | C |
| 54 | Duplicate target RIDs are removed? Yes. | C |
| 55 | Target is revalidated after lock wait? Yes. | C |
| 56 | RC stale target before writes retries? Yes. | C |
| 57 | RC stale target after writes aborts? Yes. | C |
| 58 | RR stale target aborts? Yes. | C |
| 59 | Stale target can be blindly followed? No. | C |
| 60 | A qualifying target acts at most once? Yes. | C |
| 61 | SET RHS sees old row values? Yes. | CS |
| 62 | Assignments are effectively simultaneous? Yes. | CS |
| 63 | Earlier SET output feeds later SET RHS? No. | CS |
| 64 | Unmentioned columns retain old values? Yes. | C |
| 65 | Complete new tuple is constructed? Yes. | C |
| 66 | No-op UPDATE counts? Yes. | C |
| 67 | Baseline no-op UPDATE creates new version/index entries? Yes. | C |
| 68 | General post-assignment NOT NULL is explicitly enforced? No. | F |
| 69 | UPDATE RETURNING uses new row? Yes. | C |
| 70 | Duplicate target yields one RETURNING row? Yes. | C |
| 71 | New update RID is fresh? Yes. | C |
| 72 | Replacement `prev` is old RID? Yes. | C |
| 73 | New version publishes before old xmax? Yes. | C |
| 74 | Old index entries remain? Yes. | C |
| 75 | New entry added even for unchanged indexed key? Yes. | C |
| 76 | Old/new unique locks use deterministic key order? Yes. | C |
| 77 | Exact old target alone is self-excluded? Yes. | C |
| 78 | Another current-command target is excluded? No. | C |
| 79 | Key swap succeeds as final-state permutation? No. | C |
| 80 | Key cycle succeeds? No. | C |
| 81 | DELETE target is exact RID? Yes. | C |
| 82 | DELETE target spool deduplicates? Yes. | C |
| 83 | DELETE revalidates after lock? Yes. | C |
| 84 | DELETE publishes xmax/cmax? Yes. | C |
| 85 | DELETE creates new tuple? No. | C |
| 86 | DELETE removes index entries synchronously? No. | C |
| 87 | Vacuum owns entry cleanup? Yes. | C |
| 88 | DELETE RETURNING uses old image? Yes. | C |
| 89 | Physical tuple reuse can alter returned image? No. | C |
| 90 | DELETE affected count is one per finalized target? Yes. | C |
| 91 | Ordered DML input establishes action order? Undefined. | F |
| 92 | Unordered DML has canonical target order? No. | C |
| 93 | Physical RID order is semantic? No. | C |
| 94 | Duplicate target discovery affects order/count? No. | C |
| 95 | Retry output from abandoned attempt escapes? No. | C |
| 96 | Partial RETURNING prefix escapes on statement failure? No. | C |
| 97 | Explicit-transaction result can precede COMMIT? Yes. | C |
| 98 | Competing cross-row runtime errors have defined precedence? No. | F |
| 99 | RETURNING sequence is explicitly ordered/unordered? No. | F |
| 100 | Affected counts depend on target order? No. | C |
| 101 | DML subquery obtains fresh snapshot? No. | C |
| 102 | DML subquery obtains new CommandId? No. | C |
| 103 | Subquery sees current-command inserts ordinarily? No. | C |
| 104 | Subquery sees old row deleted in current command? Yes. | C |
| 105 | Lazy subquery state is per attempt? Yes. | C |
| 106 | INSERT SELECT can recursively see its inserted rows? No. | C |
| 107 | UPDATE can rediscover new versions? Target spool prevents it. | C |
| 108 | DELETE can act twice on rediscovered RID? No. | C |
| 109 | Current-command visibility alone is Halloween defense? No. | C |
| 110 | Physical target materialization algorithm is mandated? Only semantic spool boundary. | CS |
| 111 | V1 constraints are PK, UNIQUE, NOT NULL? Yes. | C |
| 112 | PK implies UNIQUE? Yes. | C |
| 113 | PK implies NOT NULL on every component? Yes. | C |
| 114 | UNIQUE allows multiple NULL-containing keys? Yes. | C |
| 115 | Composite any-NULL key bypasses conflict? Yes. | C |
| 116 | NaNs canonicalize equal for unique keys? Yes. | C |
| 117 | Signed zeros canonicalize equal? Yes. | C |
| 118 | VARCHAR uses binary key equality? Yes. | C |
| 119 | Constraints are immediate? Yes. | C |
| 120 | Deferrable constraints exist? No. | N/A |
| 121 | Unique probe uses snapshot visibility alone? No. | C |
| 122 | Complete equal-key range is rechecked? Yes. | C |
| 123 | Uncommitted creator causes wait? Yes. | C |
| 124 | Uncommitted delete frees key? No. | C |
| 125 | Aborted creator owns key? No. | C |
| 126 | Aborted delete frees key? No. | C |
| 127 | Unique locks last through terminal publication? Yes. | C |
| 128 | Wait invalidates prior uniqueness conclusions? Yes. | C |
| 129 | Unique build uses same encoded equality/NULL rule? Yes. | C |
| 130 | Batch pending duplicates require attempt-local detection? Yes. | C |
| 131 | CREATE TABLE binding allocates persistent IDs? No. | C |
| 132 | Table name is revalidated under lock? Yes. | C |
| 133 | TableId is durably allocated first? Yes. | C |
| 134 | Heap/FSM files start private? Yes. | C |
| 135 | Required unique indexes are built privately? Yes. | C |
| 136 | Final names are durable before catalog rows? Yes. | C |
| 137 | Catalog rows use exact schema-v1 normalization? Yes. | C |
| 138 | Initial SchemaVer is 1? Yes. | C |
| 139 | Abort leaves catalog invisible? Yes. | C |
| 140 | Abort may leave cleanup-owned physical residue? Yes. | C |
| 141 | CREATE INDEX takes SchemaLock? Yes. | C |
| 142 | It takes exclusive target writer gate? Yes. | C |
| 143 | Existing writers drain before scan? Yes. | C |
| 144 | New writers remain blocked through terminal outcome? Yes. | C |
| 145 | Half-built index is plan-visible? No. | C |
| 146 | Historical RR snapshot may omit current committed rows? No. | C |
| 147 | Build explicitly includes own earlier-command uncommitted rows? No. | F |
| 148 | Unique build validates duplicate live owners? Yes. | C |
| 149 | Manifest revalidates before catalog mutation? Yes. | C |
| 150 | File is durable before descriptor publication? Yes. | C |
| 151 | DROP TABLE is transactional? Yes. | C |
| 152 | DROP INDEX is transactional? Yes. | C |
| 153 | DROP TABLE includes indexes/constraints? Yes. | C |
| 154 | DROP waits for target writers? Yes. | C |
| 155 | Backing-index DROP dependency policy is defined? No. | F |
| 156 | DROP unlinks immediately? No. | C |
| 157 | Older snapshots may retain dropped object? Yes. | C |
| 158 | Abort restores logical LIVE state? Yes. | C |
| 159 | Committed DROP can be undone by unlink failure? No. | C |
| 160 | IDs become reusable after DROP? No. | C |
| 161 | Own later command sees completed DDL? Yes. | C |
| 162 | Other transaction sees uncommitted DDL? No. | C |
| 163 | Old RR snapshot sees newly committed object? No. | C |
| 164 | Cache can override snapshot visibility? No. | C |
| 165 | Descriptor mutation in place is allowed? No. | C |
| 166 | Index-only DDL increments tuple SchemaVer? No. | C |
| 167 | CREATE TABLE starts SchemaVer 1? Yes. | C |
| 168 | DROP/recreate same name gets new IDs? Yes if permitted. | CS |
| 169 | Same-transaction DROP/recreate is explicitly permitted/rejected? No. | F |
| 170 | Old plans can retain immutable descriptors? Yes. | C |
| 171 | Private file existence means committed object? No. | C |
| 172 | Final-uncommitted filename means committed object? No. | C |
| 173 | COMMIT may precede final-name fsync? No. | C |
| 174 | Namespace-sync failure can be reported success? No. | C |
| 175 | Catalog mutation is ordinary WAL-backed state? Yes. | C |
| 176 | Private build WAL can belong to orphan cleanup? Yes. | C |
| 177 | DROP unlink durability needs parent fsync? Yes. | C |
| 178 | Missing committed object file permits READY? No. | C |
| 179 | Orphan classification must precede READY? Yes. | C |
| 180 | Unknown external files may be guessed orphan? No. | C |
| 181 | Durable committed DDL survives restart? Yes. | C |
| 182 | Crash loser DDL becomes visible? No. | C |
| 183 | Recovery reruns SQL DDL? No. | C |
| 184 | Recovery may skip proven orphan-file redo? Yes. | C |
| 185 | Second crash can lose acknowledged CREATE? No. | C |
| 186 | Cache failure can change durable commit? No. | C |
| 187 | Incoherent postcommit cache state may be noncontinuable? Yes. | C |
| 188 | Cleanup failure resurrects DROP? No. | C |
| 189 | Failed DDL may burn object/File IDs? Yes. | C |
| 190 | Committed catalog corruption prevents READY? Yes. | C |
| 191 | Logical semantics depend on C++ class layout? No. | C |
| 192 | IDs depend on pointer addresses? No. | C |
| 193 | DDL requires a specific bulk-sort implementation? No. | C |
| 194 | Target spool storage form is semantically fixed? No. | C |
| 195 | Cache container is fixed? No. | C |
| 196 | Catalog heap row order is authority? No. | C |
| 197 | CREATE INDEX B+ algorithm beyond owner is fixed? No. | C |
| 198 | Result buffering implementation is fixed in Chapter 21? No. | C |
| 199 | File publication sequence is correctness-relevant? Yes. | C |
| 200 | Implementation freedom preserves all publication barriers? Yes. | C |
| 201 | Chapter contains project chronology? Yes, locally. | F |
| 202 | Chapter contains current implementation status? No. | C |
| 203 | Chapter contains development sequencing? No. | C |
| 204 | Parser recovery sits with canonical parser owner? No. | F |
| 205 | DML/DDL binding prose duplicates Chapter 19? Partly. | F |
| 206 | Chapter contains test recipes? No. | C |
| 207 | Chapter contains devlog/history? No. | C |
| 208 | Chapter is generally analytical? Yes. | C |
| 209 | Durable v1 exclusions are explicit? Yes. | C |
| 210 | Technical rationale is preserved? Yes. | C |
| 211 | Chapter 15 affected rows are preserved? Yes. | C |
| 212 | Chapter 16 catalog identity is preserved? Yes. | C |
| 213 | Chapter 17 coercion/default semantics are preserved? Yes. | C |
| 214 | Chapter 19 namespaces are preserved? Yes, despite duplication. | C |
| 215 | Chapter 20 bag/subquery handoff is preserved? Yes. | C |
| 216 | Chapter 31 one-target-once is preserved? Yes. | C |
| 217 | §39 statement consequence is preserved? Yes. | C |
| 218 | All explicit references exist? Yes. | C |
| 219 | Two broad Chapter-15 references could be narrower? Yes. | CS |
| 220 | No circular owner reference was found? Yes. | C |
| 221 | Statement-attempt verification exists? Yes. | C |
| 222 | CommandId verification exists? Yes. | C |
| 223 | Target dedup/Halloween verification exists? Yes. | C |
| 224 | Affected-row/result-envelope verification exists? Yes. | C |
| 225 | Catalog CREATE/DROP lifecycle verification exists? Yes, broad. | CS |
| 226 | Exact offline CREATE INDEX protocol has complete coverage? No. | F |
| 227 | UPDATE NOT NULL can be verified deterministically now? No. | F |
| 228 | Backing-index DROP can be verified now? No. | F |
| 229 | DML error/RETURNING ordering can be verified now? No. | F |
| 230 | Chapter 21 is architecture-clean? No. | F |

## Previous-chapter compatibility

| Frozen owner | Result |
|---|---|
| Chapter 15 | Compatible except Chapter 21 leaves additional row-order/constraint policy undefined; affected rows, physical versions, retry, and result envelope are preserved |
| Chapter 16 | Compatible, but `DROP INDEX` dependency behavior must be chosen so constraint/index references remain valid |
| Chapter 19 | Semantic behavior compatible; Chapter 21 duplicates some binder responsibilities |
| Chapter 20 | Bag/order/subquery semantics preserved; the missing DML observable-order rule is exposed precisely because Chapter 20 does not create hidden row order |
| Chapters 9–14, 17 | No redefinition found |
| Chapter 29 | Unchanged |
| Persistence/recovery | No format or recovery contradiction except the incomplete CREATE INDEX row-set rule |

## Complete findings

### BLOCKING

#### B21-1 — General UPDATE NOT NULL enforcement is undefined

- Section: §21.13, composed with §§15.3 and 31.7
- Type: CONSTRAINT SEMANTICS
- Evidence: INSERT explicitly checks runtime NOT NULL in §31.6, and §21.7 explicitly checks PK components. UPDATE constructs a complete new tuple but never requires checking every descriptor-nonnullable column after RHS evaluation/coercion.
- Competing outcomes:
  - reject `UPDATE t SET nn = NULL`;
  - persist a NULL in a declared NOT NULL column.
- Consequence: divergent committed data and invalid trusted constraint metadata.
- Correct owner: Chapter 21 operation semantics, physically enforced by Chapter 31.
- Smallest decision: require descriptor-wide NOT NULL validation of the final UPDATE row before publishing that target’s replacement, define its error category/origin, then use §39.1 for transaction consequence.

#### B21-2 — DROP INDEX dependency behavior is undefined

- Section: §21.9
- Type: DDL SEMANTICS
- Evidence: `DROP TABLE` explicitly includes dependent indexes/constraints; `DROP INDEX` does not state what happens when `sys_constraints.index_id` owns the index.
- Competing outcomes: reject; drop constraint too; or create invalid catalog state.
- Consequence: divergent catalog state or corrupted constraint authority.
- Correct owner: Chapter 21, consistent with §16.5.
- Smallest decision: define RESTRICT-like rejection or an explicit atomic dependency-drop rule. Given the v1 grammar, rejection is the smallest surface.

#### B21-3 — CREATE INDEX can omit own earlier DML

- Sections: §§21.2.1 and 21.8.2
- Type: CATALOG PUBLICATION
- Evidence: DDL-first then same-table DML is legal under retained exclusive writer ownership, and subsequent DDL is legal. The build scan nevertheless includes only “current logically live committed row versions.”
- Competing outcomes: scan committed-only rows or scan all current owners that may publish with this transaction.
- Consequence: a committed index may omit rows committed in the same transaction.
- Correct owner: Chapter 21’s offline build row-set contract, composed with Chapters 10–11.
- Smallest decision: define the build input as all current logical owners in the final transaction-local table state, including the DDL transaction’s earlier-command rows.

#### B21-4 — Multi-row DML runtime error precedence is undefined

- Sections: §§21.11, 21.13–21.15; Chapters 20 and 31
- Type: ERROR ORDERING
- Evidence: DML sources/target spools may be unordered, while different row occurrences may fail at different executable expressions and SourceSpans. No rule defines or explicitly permits which error wins.
- Competing outcomes: physical encounter order, logical ordered-child order, deterministic source-span precedence, or unspecified permitted variation.
- Consequence: same statement/state may return different categories/spans across plans, scan orders, batching, or spills.
- Correct owner: Chapter 21 statement semantics, with §39 retaining transaction consequence.
- Smallest decision: define the semantic error-selection domain for ordered and unordered DML without mandating a physical visitation algorithm.

### MAJOR

#### M21-1 — RETURNING order is unspecified

- Section: §21.15
- Type: RETURNING SEMANTICS
- Evidence: row image, buffering, and publication are exact; sequence ordering is absent.
- Competing outcomes: inherit ordered input, spool order, target encounter order, or unordered bag.
- Consequence: conforming implementations may return different sequences.
- Correct owner: Chapter 21, composed with Chapter 20 ordering.
- Future action: explicitly state ordered inheritance or that all v1 DML `RETURNING` results are unordered unless a future syntax supplies order.

#### M21-2 — Same-transaction DROP/recreate name policy is ambiguous

- Sections: §§21.3, 21.6.2, 21.8.2, 21.9
- Type: CATALOG PUBLICATION
- Evidence: own earlier DDL is visible through self/CommandId semantics, but CREATE TABLE revalidates against “current committed catalog state.”
- Competing outcomes: an earlier self-DROP frees the name for reuse in the same transaction, or the still-committed predecessor blocks it until transaction end.
- Consequence: different success/failure and committed object identity.
- Correct owner: Chapter 21 catalog-name revalidation.
- Future action: define revalidation against an exact current-owner view including or excluding own earlier catalog deletions.

### MINOR

#### N21-1 — Project-time wording

Sections §21.6.1, §21.8.1, §21.17, §21.18, and §21.19 contain “Initial…”, “initial parser”, “intended first serious SQL surface”, and “future architecture-compatible…initial engine.” These should become timeless v1 scope wording.

Type: TEMPORALITY.

#### N21-2 — Binder ownership duplication

Sections §§21.6.1, 21.8.1, 21.11, 21.13, and 21.14 restate name/type/duplicate-target validation owned by Chapter 19. Chapter 21 should retain only the bound-statement-to-operation handoff and Chapter-21-specific target/default/publication consequences.

Type: DOCUMENT OWNERSHIP.

#### N21-3 — Parser recovery is in the wrong canonical chapter

Section §21.17 defines parser/batch recovery despite Chapter 18 owning syntax/parser behavior. It should be moved or delegated without changing behavior.

Type: DOCUMENT OWNERSHIP.

### EDITORIAL

#### E21-1 — Two broad references could be narrower

- §21.13 “Chapter 15’s update protocol” → §15.3
- §21.15 “Chapter 15’s retry rule” → §15.7

Type: CROSS-REFERENCE.

## Frozen architecture semantic questions

1. At what exact point must UPDATE enforce all descriptor `NOT NULL` constraints?
2. Is dropping a PK/UNIQUE backing index rejected, or does it atomically drop the constraint?
3. Which transaction-local own rows belong to an offline CREATE INDEX build?
4. What selects the user-visible error among multiple failing DML row occurrences?
5. What ordering, if any, does DML `RETURNING` guarantee?
6. Can a transaction DROP and recreate the same table/index name before commit?

No architecture edit should proceed until these are explicitly decided.

## Follow-up verification gaps

| Architecture area | Current procedural coverage | Status |
|---|---|---|
| Statement attempts/CommandId/retry | Detailed deterministic DML harness | COMPLETE |
| Target deduplication/stale targets/Halloween | Detailed spool and lock procedures | COMPLETE |
| Affected rows/result envelope | Complete | COMPLETE |
| Immediate uniqueness/key swaps/concurrency | Complete | COMPLETE |
| Default blob/coercion/catalog reconstruction | Complete | COMPLETE |
| CREATE/DROP file/catalog/cache lifecycle | Broad matrix exists | PARTIAL |
| Full offline CREATE INDEX build/failure/publication matrix | No exact end-to-end family found | MISSING |
| General UPDATE NOT NULL | Cannot define oracle | BLOCKED BY SEMANTIC QUESTION |
| Constraint-backed DROP INDEX | Cannot define oracle | BLOCKED BY SEMANTIC QUESTION |
| Own-transaction CREATE INDEX row set | Cannot define oracle | BLOCKED BY SEMANTIC QUESTION |
| DML error ordering | Cannot define oracle | BLOCKED BY SEMANTIC QUESTION |
| RETURNING order | Cannot define oracle | BLOCKED BY SEMANTIC QUESTION |
| Same-transaction name recreation | Cannot define oracle | BLOCKED BY SEMANTIC QUESTION |
| ANALYZE transaction/cache publication | Existing catalog/statistics methods reusable | PARTIAL/REUSABLE |
| DDL recovery/second crash | Existing namespace/WAL/recovery methods reusable | PARTIAL/REUSABLE |

These are future verification gaps, not additional architecture findings.

## Implementer-invention assessment

A conforming implementer can determine without invention:

- statement-attempt and CommandId boundaries;
- pre-write retry versus post-write abort;
- physical no-undo semantics;
- INSERT target mapping and defaults;
- UPDATE/DELETE target identity, deduplication, and stale-target handling;
- assignment old-row image;
- no-op UPDATE count and baseline physical version behavior;
- row images and affected-row counts;
- statement-local subquery visibility;
- Halloween protection;
- immediate uniqueness, NULL behavior, key swaps/cycles, and lock lifetime;
- DDL transaction visibility;
- CREATE file/catalog ordering;
- ID nonreuse;
- DROP retirement;
- cache publication;
- WAL/recovery and second-crash behavior.

It must still invent policy for the six frozen questions above. Therefore Chapter 21 cannot yet stand as a complete canonical v1 statement-semantics contract.

## Chapter-22 boundary

Exact next heading:

```text
# 22. Physical Plan and Runtime Operator Model
```

Principal handoff:

```text
Chapter-21 resolved DDL/DML operation
    ->
Chapter-22 immutable physical operator and execution-state vocabulary
```

Explicit downstream operator roles include `PhysicalInsert`, `PhysicalUpdate`, `PhysicalDelete`, `PhysicalCreateTable`, `PhysicalCreateIndex`, and `PhysicalDrop`; detailed DML/DDL execution is delegated to Chapter 31.

Chapter 22 was not directly reviewed.

Recommended future Chapter-22 review scope, after Chapter 21 closes: physical-plan/execution-state separation, capability registry, DML/DDL operator conformance, LogicalSlotId/schema preservation, and avoidance of current implementation-state narration.

## Recommended next action

**Frozen semantic architecture review required.**

Decision order recommended:

1. CREATE INDEX transaction-local build row set.
2. UPDATE NOT NULL enforcement.
3. DROP INDEX dependency policy.
4. DML cross-row error precedence.
5. RETURNING ordering.
6. Same-transaction name reuse.
7. Then targeted document-only cleanup for temporality, binder ownership, parser ownership, and reference precision.
8. Only afterward synchronize Chapter-21 verification.

## Repository and scope guarantee

Initial state:

- HEAD: `0252ee54337aa54c824e41b76e9967a6c87fdb93`
- Working tree: clean
- Index: clean

Final state:

- HEAD: `0252ee54337aa54c824e41b76e9967a6c87fdb93`
- Working tree: clean
- Index: clean
- `git diff --check`: passed
- Files modified by audit: **none**
- Audit-created repository changes: **none**
- External repository changes observed: **none at either Git check**
- Review artifact: not read, modified, moved, removed, or staged
- Build/tests/benchmarks: not run
- Implementation work: none
- Verification synchronization: not performed
- Chapter 22 direct review: not started
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
