# Chapter 24 — FROZEN ARCHITECTURE SEMANTIC REVIEW REQUIRED

Chapter 24 establishes the intended ownership shape, but six correctness-sensitive decisions remain undefined. Two are blocking because the current wording permits either unbounded unaccounted execution memory or unsafe size/offset handling.

## Repository state

| Check | Initial | Final |
|---|---|---|
| Branch | `main` | `main` |
| HEAD | `d1ba2aaea5df5860b25228dec8743f451529c0b3` | unchanged |
| Working tree | clean | clean |
| Index | clean | clean |
| `git diff --check` | — | passed |
| Audit-created changes | none | none |

Historical review artifacts were not read, modified, moved, or staged. No files were modified.

## Scope and structure

Chapter 24 is [ARCHITECTURE.md:19664](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19664):

> `# 24. Query Memory, Row Storage, and Spill`

It ends at line 19932. Chapter 25 begins at [ARCHITECTURE.md:19933](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19933):

> `# 25. Vectorized Expression Execution`

| Section | Heading | Responsibility | Upstream owner | Downstream consumer | Role |
|---|---|---|---|---|---|
| 24.1 | Execution RowLayout | Temporary row representation | Ch17/22/23 | Ch28–31 | Architecture-appropriate, incomplete size domain |
| 24.2 | RowCollection | Append-oriented retained-row storage | Ch17/20/23 | Ch28–31 | Architecture-appropriate, incomplete oversized-row contract; temporal wording |
| 24.3 | QueryArena | Bounded query-lifetime metadata allocation | Ch22/23 | Ch25–32 | Architecture-appropriate |
| 24.4 | QueryMemoryManager | Query/global accounting and limits | Ch22 | All execution operators | Architecture-appropriate, blocking accounting ambiguity |
| 24.5 | MemoryReservation | Tracked operator-memory ownership | §24.4 | Blocking operators | Architecture-appropriate, incomplete reservation state model |
| 24.6 | Soft and hard pressure | Pressure, denial, spill/retry, OOM | §§24.4–24.5 | Ch26, Ch28–31, §39 | Architecture-appropriate, incomplete terminal behavior |
| 24.7 | SpillManager | Temporary-file ownership/lifecycle | Ch12/22 | Ch28–31 | Architecture-appropriate, incomplete crash-leftover contract |
| 24.8 | Spill block contract | Generic temporary block framing | Ch17/23 | Ch28–30 | Architecture-appropriate, blocking checked-decoder gap |
| 24.9 | Spill I/O | Sequential buffered I/O | §24.7 | Ch28–30 | Architecture-appropriate; temporal wording |
| 24.10 | Error and cleanup boundary | Resource cleanup and §39 handoff | Ch21/22/39 | Ch26/31 | Architecture-appropriate |
| 24.11 | Memory/spill invariants | Consolidated rules | §§24.1–24.10 | All execution chapters | Architecture-appropriate but inherits open questions |

## Findings

| Severity | Count |
|---|---:|
| BLOCKING | 2 |
| MAJOR | 5 |
| MINOR | 2 |
| EDITORIAL | 0 |

### Blocking findings

#### B24-1 — Query-memory accounting domain is incomplete

- Section: [§§24.4–24.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19739)
- Type: `MEMORY ACCOUNTING`
- Evidence: temporary process-memory buffers are accounted only “when large or when owned by tracked arenas/reservations”; neither “large” nor the aggregate treatment of many small allocations is defined.
- Competing interpretations:
  1. every query-owned allocation and capacity charge participates in the hard gate;
  2. individually small direct allocations remain untracked even if their aggregate grows with rows, blocks, runs, or partitions.
- Consequence: interpretation 2 permits unbounded query-dependent memory outside the hard limit.
- Smallest action: freeze the accounted-byte universe, measurement basis, explicit exemptions, and invariant over live accounted ownership.

#### B24-2 — Checked size/address arithmetic and spill pre-access validation are undefined

- Sections: [§24.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19666), [§24.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19693), [§24.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19857)
- Type: `SIZE ARITHMETIC`
- Evidence: row lengths, varlen offsets, row/block counts, reservation sums, spill lengths, and file offsets have no general checked-arithmetic rule. “Structural mismatch” is classified, but validation before allocation, range formation, or dereference is not required.
- Competing interpretations:
  1. validate every derived extent with checked arithmetic before allocation/access;
  2. trust self-generated temporary metadata and validate only CRC or obvious framing.
- Consequence: wraparound, under-accounting, out-of-bounds access, oversized allocation, or memory-unsafe spill decoding.
- Smallest action: freeze checked representability, pre-access validation, and controlled failure classification for all runtime size/count/offset calculations.

### Major findings

#### M24-1 — Reservation approval and physical allocation are not separated

- Sections: §§24.5–24.6
- Type: `RESERVATION SEMANTICS`
- Ambiguity: whether a reservation represents approved future budget, already allocated/held bytes, or both; what happens when allocation fails after approval; when the charge becomes live; how it is rolled back.
- Consequence: temporary hard-limit overrun, leaked accounting, or double release.
- Action: define a representation-neutral reservation/allocation state machine.

#### M24-2 — Spill/reclaim/retry has no terminal progress rule

- Section: §24.6
- Type: `RESOURCE SEMANTICS`
- Ambiguity: a denied spillable operator must spill and retry, but behavior is undefined when no eligible state remains or retry cannot free enough memory.
- Consequence: infinite reclaim/retry, livelock, or implementation-dependent failure category.
- Action: require finite progress and a controlled terminal resource failure when no exact progress-producing fallback remains.

#### M24-3 — Oversized retained row/value behavior is undefined

- Sections: §§24.1–24.2
- Type: `ROW STORAGE`
- Ambiguity: the 256 KiB block size is a target, but no rule covers a single exact row or varlen value larger than an ordinary block or descriptor domain.
- Consequence: dedicated oversized storage, alternate exact representation, arbitrary rejection, or accidental persistent heap-tuple limit reuse.
- Action: freeze exact oversized-row applicability/fallback/failure policy without mandating a concrete representation.

#### M24-4 — Crash-leftover spill reclamation is optional and collision isolation is not explicit

- Section: §24.7
- Type: `TEMP FILE LIFETIME`
- Evidence: startup maintenance “may remove” proven managed files.
- Consequence: indefinite accumulation after crashes and eventual temp-resource exhaustion; the text also does not explicitly state that a new query cannot adopt/collide with a stale file.
- Action: define the required safety postcondition: stale files never become live query/database state, names cannot collide with live ownership, and managed leftovers receive bounded/eventual reclamation.

#### M24-5 — Allocator failure normalization is qualified by “where possible”

- Section: §24.6
- Type: `ALLOCATION SEMANTICS`
- Comparison: §39.3 treats execution OOM as a controlled category.
- Consequence: a catchable allocation failure could either become `OutOfMemory` or escape as an uncontrolled implementation failure.
- Action: distinguish catchable allocation denial, address-domain failure, and unrecoverable process termination; require controlled handling where execution remains capable of returning an error.

### Minor findings

#### N24-1 — Project-time wording

- Type: `TEMPORALITY`
- Occurrences:
  - §24.2: “The initial target block size is”
  - §24.9: “The initial operator-level target is”
- These should become timeless v1 tuning statements.
- Document-only.

#### N24-2 — Resource policy versus SQL semantics lacks a local analytical handoff

- Type: `ANALYTICAL DEPTH`
- Upstream Chapters 17, 20, 23, and §39 make the answer determinate, but Chapter 24 does not explicitly say that budgets, block sizes, and spill thresholds cannot redefine scalar domains, row counts, multiplicity, or required order.
- Document-only after semantic decisions.

## Frozen Chapter-24 semantic questions

| Decision | Exact question | Frozen constraints |
|---|---|---|
| Q24-1 | What complete set and measurement basis of query-owned memory participates in per-query/global hard accounting, and what categories are explicitly exempt? | Ch17 resource-qualified domains; Ch22 QEC ownership; Ch23 accounted reusable buffers; no unbounded bypass |
| Q24-2 | What is the exact reservation → allocation → ownership → release state model, including allocation failure after reservation and accounting transfer? | Hard-limit enforcement; §39 controlled OOM; no leaks/double release |
| Q24-3 | When spill/reclaim cannot produce enough memory, what finite-progress and terminal failure rule applies? | No semantic approximation; bounded execution progress; §39 resource categories |
| Q24-4 | How does RowCollection retain a single row/value outside its ordinary block/descriptor domain? | Ch17 arbitrary finite VARCHAR; D23-S3 no truncation; heap tuple limit is not a runtime limit |
| Q24-5 | What checked arithmetic, representability, and pre-access validation rules govern row/block/reservation/spill sizes, counts, offsets, and extents? | No wrap/truncation/OOB; malformed temp data is not persistent corruption |
| Q24-6 | What namespace, collision, adoption, and eventual cleanup rules govern crash-leftover spill files? | Spill is temporary, never recovered/WAL-logged, unrelated files untouched |

No direct frozen-owner contradiction was found. These are missing Chapter-24 contracts, not reasons to reopen Chapters 17–23.

## Canonical owner map

| Concept | Canonical owner | Result |
|---|---|---|
| SQL scalar/NULL/VARCHAR/FLOAT semantics | Chapter 17 | Earlier owner |
| Bag multiplicity and required order | Chapter 20 | Earlier owner |
| Attempt/retry and publication consequences | Chapters 15/21, §39.1 | Earlier owner |
| QueryExecutionContext and runtime ownership | Chapter 22 | Earlier owner |
| DataChunk/StringRef borrowing and representability | Chapter 23 | Earlier owner |
| RowLayout and generic RowCollection | Chapter 24 | Owned here |
| QueryArena | Chapter 24 | Owned here |
| QueryMemoryManager/reservations/pressure | Chapter 24 | Owned here, incomplete |
| Generic SpillManager and temporary block framing | Chapter 24 | Owned here, incomplete |
| Expression scratch/result vectors | Chapter 25 | Later owner |
| Pipeline cancellation/finalization | Chapter 26 | Later owner |
| Join/aggregate/sort spill payload semantics | Chapters 28–30 | Later owner |
| DML target/RETURNING spool and result cursor | Chapter 31 | Later owner |
| Public execution-error categories | §39 | Earlier/cross-cutting owner |

## Final architecture models

### Query memory and accounting

Determinate:

- One query owns a `QueryMemoryManager`.
- It participates in a database/global execution budget.
- It exposes per-query soft and hard limits.
- BufferPool capacity is excluded.
- QueryArena pages are nonspillable and accounted.
- Large operator allocations use tracked reservations.
- Spill buffers are accounted.
- Large untracked allocation after reservation denial is forbidden.

Undetermined:

- the complete accounted allocation universe;
- requested versus capacity/usable-byte accounting;
- metadata/header/allocator-overhead treatment;
- aggregation of small allocations;
- reservation-before-allocation behavior;
- transfer and reload accounting.

The hard limit is therefore conceptually present but not yet mechanically well-defined.

### QueryArena

| Property | Result |
|---|---|
| Owner | Query |
| Lifetime | Query execution end |
| Intended contents | Small metadata/state |
| Spillability | Nonspillable |
| Accounted | Yes, backing pages |
| Unbounded row-dependent use | Forbidden |
| Borrowing | Legal only within D23-S2 owner stability |
| Persistence | Forbidden |
| Open issue | “Small/bounded” needs the Q24-1 accounting universe to remain enforceable |

### RowCollection

| Property | Result |
|---|---|
| Semantics | Query-temporary retained rows |
| Schema | `RowLayout` derived from resolved logical types |
| Shape | Append-oriented blocks, associated varlen storage |
| Row identity | Query-local handle only; never RID or semantic row ID |
| Ordering | Physical append order; semantic order only when an owning operator establishes it |
| VARCHAR | Retaining storage owns exact bytes |
| NULL | Independent null bitmap; no payload sentinel |
| Spill | Handle/representation changes across spill |
| Block target | 256 KiB, configurable/tuning |
| Open issue | Single row/value outside ordinary block/descriptor domain |

### Spill

| Dimension | Result |
|---|---|
| Eligibility | Reservation/operator declared |
| Trigger | Soft pressure may trigger; hard denial triggers spill/reclaim for spillable state |
| SQL transparency | Required by Ch17/20 and specialized Ch28–31 contracts |
| Durability | Temporary only |
| WAL | None |
| Crash recovery | None |
| Generic framing | magic, temporary version, identity, payload length, row count, CRC32C, payload |
| Encoding | Explicit integers; no native object/pointer dump |
| Error | Structural/checksum mismatch → `SpillIOError` |
| Disk exhaustion | `SpillIOError` through the execution/spill owner |
| Reload owner | Owning operator/query; memory must be accounted |
| Open issues | checked decoder bounds, retry termination, crash leftovers |

## Core matrices

### Memory-accounting and owner matrix

| Category | Owner | Accounted? | Spillable? | Release | Persistent | Status |
|---|---|---:|---:|---|---:|---|
| QueryArena pages | QueryArena/query | yes | no | query teardown | no | clear |
| Small arena objects | Arena page owner | indirectly | no | bulk arena release | no | clear |
| DataChunk/vector large buffers | Operator/query | yes under Ch23/24 | usually no | reset/teardown | no | partial basis |
| RowCollection fixed rows | Operator/collection | intended yes | owner-dependent | release/spill | no | blocked Q24-1 |
| RowCollection varlen | Operator/collection | intended yes | owner-dependent | release/spill | no | blocked Q24-1/Q24-4 |
| Block metadata/directories | Operator | not exact | owner-dependent | operator teardown | no | blocked Q24-1 |
| Hash/aggregate/sort state | Operator reservation | yes | declared | finalize/spill/teardown | no | clear except accounting basis |
| Spill read/write buffers | Query/operator | yes | n/a | I/O completion/teardown | no | clear |
| Reloaded blocks | Operator/query | implied | may re-spill | release/re-spill | no | partial |
| DML/result spool | Statement/request owner | yes | yes | discard/delivery end | no | delegated Ch31 |
| BufferPool frames | BufferPool | excluded | n/a | BufferPool | yes/no page cache | clear |

### Reservation/allocation matrix

| Event | Current rule | Gap |
|---|---|---|
| Request growth below limits | grant implied | charge point undefined |
| Request crosses soft budget | pressure may be requested | advisory by design |
| Request crosses hard limit | deny | simultaneous query/global rule implicit |
| Spillable denial | spill/release then retry | terminal no-progress case undefined |
| Nonspillable denial | controlled OOM | clear |
| Allocation fails after grant | not specified | Q24-2/M24-5 |
| Allocation succeeds below requested capacity | not specified | Q24-2 |
| Release | returns accounted bytes | exact once-only transition unspecified |
| Ownership transfer | not specified | permitted implementation detail, but accounting continuity required |
| Cancellation/error | all reservations unwind | clear final postcondition |

### Resource-limit and size matrix

| Domain | SQL limit? | Runtime bound/fallback | Failure owner | Status |
|---|---:|---|---|---|
| Query soft budget | no | pressure hint | QMM | clear |
| Query hard maximum | no | allocation gate | OOM/spill | accounting basis open |
| Global execution limit | no | shared gate | QMM | concurrency invariant implicit |
| Single allocation | no | exact allocation or resource failure | §39 | M24-5 |
| Row block target | no | configurable | RowCollection | clear as target |
| Single oversized row | no | undefined | — | Q24-4 |
| VARCHAR length | no new SQL limit | exact form or D23-S3 failure | Ch23/39 | preserved |
| Block/run/file count | no | checked domain absent | — | Q24-5 |
| Spill payload/file offset | no | checked domain absent | SpillManager | Q24-5 |
| Temp disk | no | spill failure | SpillIOError | consistent |

### Spill eligibility/transparency matrix

| Structure | Spillable | Semantic preservation owner | Status |
|---|---:|---|---|
| QueryArena | no | Ch24 | complete |
| Hash join rows/partitions | yes | Ch28 | complete |
| Aggregate keys/raw rows/exact state | yes | Ch29 | complete |
| Sort rows/runs | yes | Ch30 | complete |
| DISTINCT state | yes | Ch29 | complete |
| DML target spool | yes | Ch31 | complete |
| RETURNING spool | yes | Ch31 | complete |
| Arbitrary operator metadata | declaration-dependent | owning operator | appropriate |
| Pinned/live borrowed input | not until owned/materialized | Ch23/24 | complete |
| Oversized single retained row | undefined | Ch24 | Q24-4 |

For every successful spill path, values, NULL state, multiplicity, required order, exact VARCHAR bytes, and operator-owned error semantics must match the in-memory path. Spill may change performance, memory use, physical order where no order is promised, and introduce legitimate I/O/resource failure.

### Spill-format and persistence matrix

| Object | Persistent DB state? | WAL? | Recovered? | Raw pointers legal? |
|---|---:|---:|---:|---:|
| Database page | yes | owner-defined | yes | no |
| WAL | yes | self | replayed | no |
| Catalog/persisted scalar | yes | through DB protocols | yes | no |
| Runtime Vector/DataChunk | no | no | no | process-local only |
| QueryArena | no | no | no | process-local only |
| RowCollection memory | no | no | no | process-local only |
| Spill file | no | no | no | no |
| Temporary run metadata | no | no | no | no |
| Reloaded StringRef | no | no | no | must point to a new valid owner |

### Spill-error taxonomy

| Condition | Classification |
|---|---|
| Hard reservation denial, nonspillable | `OutOfMemory` |
| Exact supported allocation unavailable | `OutOfMemory` |
| No exact VARCHAR representation | `ExecutionError` representability/resource cause |
| Spill read/write/ENOSPC | `SpillIOError` |
| CRC or structural mismatch | `SpillIOError` |
| Cancellation | `QueryCancelled` |
| Invalid internal reservation/accounting state | internal invariant |
| Invalid spill offset causing unsafe access | must be rejected; exact checked rule blocked by Q24-5 |
| SQL/transaction consequence | §39.1 publication boundary |

### Cleanup matrix

| Exit path | Memory/reservations | Spill files | Transaction consequence |
|---|---|---|---|
| Success | release/transfer | delete | owner-defined success |
| Ordinary semantic error | release | delete | §39.1 |
| `ExecutionError` | release | delete | §39.1 |
| `OutOfMemory` | release | delete | §39.1 |
| `SpillIOError` | release | delete | §39.1 |
| Cancellation | release | delete | command/transaction owner |
| Deadlock/mandatory abort | attempt resources release; locks through abort | delete | Ch11/15/39 |
| Pre-write retry | discard attempt-local state | delete attempt files | Ch15/31 |
| Query teardown | release all query ownership | delete | none independently |
| Process crash | process memory disappears | leftovers possible | query aborted; Q24-6 open |

### Invalid-state matrix

| State | Classification | Pre-use rejection required? |
|---|---|---:|
| Reservation underflow/double release | internal invariant | yes |
| Live accounted sum beyond hard gate | internal accounting defect | yes |
| Row count beyond represented block domain | internal/resource depending construction | yes |
| Schema/RowLayout mismatch | internal invariant | yes |
| Invalid varlen offset/length | malformed temporary state | yes |
| Dangling retained StringRef | internal lifetime violation | yes |
| Raw pointer in spill bytes | invalid spill representation | yes |
| CRC/structural mismatch | `SpillIOError` | yes |
| Reload without accounting | internal accounting defect | yes |
| Endless spill/retry without progress | invalid liveness state, presently undefined | Q24-3 |
| Unsupported oversized retained form | controlled resource/representability outcome, presently undefined | Q24-4 |
| Stale crash spill adopted by new query | forbidden result, mechanism presently incomplete | Q24-6 |

### Determinism matrix

| Perturbation | Successful values/NULL/bag/order | Semantic error/transaction | Resource failure |
|---|---|---|---|
| Memory budget | unchanged | unchanged if successful | may change spill/failure |
| Block size | unchanged | unchanged | resource profile may change |
| Spill threshold | unchanged | unchanged | I/O exposure may change |
| Spill/no-spill path | unchanged | unchanged | spill path can raise I/O error |
| Allocation address | unchanged | unchanged | no semantic effect |
| Row/block boundary | unchanged | unchanged | no semantic effect |
| Temp-file name | unchanged | unchanged | collision must be prevented |
| Reload order | cannot alter required order | unchanged | no semantic effect |
| Pointer value | unchanged | unchanged | no semantic effect |
| Metadata allocation order | unchanged | unchanged | real OOM timing may vary within resource policy |

### Cross-chapter handoff matrix

| Handoff | Contract | Status |
|---|---|---|
| Ch5→24 | Temporary rows are not heap tuples | consistent |
| Ch17→24 | Exact scalar/NULL/VARCHAR semantics | consistent; Q24-4 needed for retention capability |
| Ch20→24 | Bag/order unaffected by storage/spill | consistent |
| Ch21→24 | Attempt and publication consequences | consistent |
| Ch22→24 | QEC/runtime owns managers and mutable work | consistent |
| Ch23→24 | Value-stable borrow, owned retention, exact VARCHAR | consistent |
| Ch24→25 | Expression state/scratch uses query memory; varlen output ownership | precise immediate handoff |
| Ch24→26 | Breakers declare memory/spill; cancellation unwinds | consistent |
| Ch24→28 | Join RowCollection/reservation/Grace spill | consistent |
| Ch24→29 | Aggregate exact-state accounting/spill | consistent |
| Ch24→30 | Sort reservation/run format/merge | consistent |
| Ch24→31 | Target/result spools and result ownership | consistent |
| Ch24→39 | OOM/SpillIO and publication consequences | consistent except M24-5 wording |

Chapter 24 contains no explicit Chapter-25 cross-reference. The immediate handoff is nevertheless clear: Chapter 24 owns execution memory and temporary ownership; Chapter 25 consumes query memory for expression state and owns vectorized expression evaluation.

## Temporality and document-owner audit

### Complete meaningful temporal classification

| Phrase | Section | Class | Result |
|---|---|---|---|
| “initial target block size” | 24.2 | E — project chronology | finding |
| “later reconstructed” | 24.2 | A — runtime ordering | legitimate |
| “currently reserved/held bytes” | 24.4 | B — runtime state | legitimate |
| “bytes currently held” | 24.5 | B — runtime state | legitimate |
| “not yet reached” | 24.6 | B — pressure-state comparison | legitimate |
| “initial operator-level target” | 24.9 | E — project chronology | finding |
| “current statement’s first published…” | 24.10/24.11 | B — transaction runtime state | legitimate |

Project-chronology count: **2**.

| Document concern | Result |
|---|---|
| DEVELOPMENT sequencing | none besides the two temporal target phrases |
| VERIFICATION procedure | none |
| PROJECT_STATE/current implementation narration | none |
| History/devlog narration | none |
| Benchmark procedure/results | none |
| Implementation coupling | C++20 `std::bad_alloc` and RAII terminology are compatible with the platform baseline; no mechanism-level finding |
| Normative language | Too weak around accounting, checked decode, allocator failure, and crash cleanup |
| Analytical depth | Strong ownership rationale; insufficient resource-domain and oversized-row rationale |
| Implementation freedom | Generally good; decisions can be closed without fixing APIs, allocators, block structures, or large-string representations |

### Explicit cross-references

| Source | Target | Purpose | Exists/owner | Quality |
|---|---|---|---|---|
| §24.8 | Chapters 28–30 | Specialized spill payloads | yes; correct later owners | GOOD |
| §24.10 | §39.1 | Transaction consequence of OOM/SpillIO | yes; canonical owner | GOOD |
| §24.11.13 | §39.1 | Consolidated transaction boundary | yes | GOOD |

No circular ownership or stale reference was found.

## Verification cross-check

Consulted:

- V23-G through V23-M ownership, reset, representability, persistence, and invalid-state families.
- Execution Testing Strategy / Spill Tests.
- Physical-Plan Validator memory/spill checks.
- String Lifetime Tests.
- Pipeline Finalization and Resource Tests.
- Hash Join, Aggregate, Sort, and DML spool tests.

| Chapter-24 mechanism | Current coverage | Missing work | Blocked? |
|---|---|---|---:|
| RowLayout/RowCollection basic ownership | PARTIAL | generic schema/layout/block cases | Q24-4/Q24-5 |
| QueryArena ownership/lifetime | PARTIAL | bounded-use and accounting oracle | Q24-1 |
| Hard/soft budgets | PARTIAL | exact ledger and simultaneous limits | Q24-1 |
| Reservation lifecycle | MISSING | reservation/allocation state oracle | Q24-2 |
| Spill/retry progress | MISSING | finite reclaim state machine | Q24-3 |
| Retained VARCHAR stability | COMPLETE via V23 | oversized form still absent | Q24-4 |
| Spill semantic transparency | PARTIAL/strong operator reuse | generic cross-operator matrix | no |
| Spill block framing/CRC | PARTIAL | malformed length/count/offset matrix | Q24-5 |
| OOM vs SpillIO | PARTIAL | exact new decision cases | Q24-2/Q24-5 |
| Cancellation/error cleanup | COMPLETE for non-crash paths | crash-leftover path absent | Q24-6 |
| Retry cleanup | PARTIAL | complete attempt-local memory ledger | Q24-1/Q24-2 |
| Runtime/persistence boundary | COMPLETE via V23/operator tests | crash namespace still absent | Q24-6 |

Verification synchronization must wait for Q24-1 through Q24-6.

## Technical consistency question matrix

Status abbreviations: `C` = consistent, `CS` = consistent but specialized, `F` = finding, `N/A` = concept absent or intentionally delegated.

### 1–20: structure and ownership

| # | Question | Status |
|---:|---|---|
| 1 | Is the Chapter-24 boundary exact? | C |
| 2 | Is Chapter 25 excluded from substantive review? | C |
| 3 | Does Ch24 own temporary row storage? | C |
| 4 | Does Ch24 avoid redefining SQL scalar semantics? | C |
| 5 | Does Ch24 avoid redefining bag semantics? | C |
| 6 | Does Ch24 avoid redefining transaction consequences? | C |
| 7 | Is QueryExecutionContext ownership inherited from Ch22? | C |
| 8 | Are runtime pointers nonpersistent? | C |
| 9 | Are specialized spill payloads delegated? | C |
| 10 | Is expression execution left to Ch25? | C |
| 11 | Is pipeline scheduling left to Ch26? | C |
| 12 | Are error categories delegated to §39? | C |
| 13 | Is RowLayout distinct from heap tuples? | C |
| 14 | Is RowLayout distinct from DataChunk? | C |
| 15 | Are row handles distinct from RIDs? | C |
| 16 | Are row handles query-local? | C |
| 17 | Is spilled handle identity separate? | C |
| 18 | Does physical row position avoid semantic identity? | C |
| 19 | Are persistent MVCC headers excluded? | C |
| 20 | Is Chapter 24 free of owner cycles? | C |

### 21–40: memory budget and accounting

| # | Question | Status |
|---:|---|---|
| 21 | Is accounting per query? | C |
| 22 | Is a global execution budget present? | C |
| 23 | Is a per-query soft budget present? | C |
| 24 | Is a per-query hard maximum present? | C |
| 25 | Are defaults nonarchitectural? | C |
| 26 | Is BufferPool capacity excluded? | C |
| 27 | Are arena pages accounted? | C |
| 28 | Are large operator allocations accounted? | C |
| 29 | Are spill buffers accounted? | C |
| 30 | Is every query-owned allocation category enumerated? | F |
| 31 | Is “large” defined? | F |
| 32 | Are many small allocations aggregated? | F |
| 33 | Are block headers accounted? | F |
| 34 | Are offset/directories accounted? | F |
| 35 | Is allocator/capacity rounding treated explicitly? | F |
| 36 | Is the accounting unit requested bytes? | F |
| 37 | Is it usable/capacity bytes? | F |
| 38 | Are explicit exemptions bounded? | F |
| 39 | Can unbounded metadata bypass the budget? | F |
| 40 | Is the hard-limit sum objectively testable? | F |

### 41–60: reservation and allocation

| # | Question | Status |
|---:|---|---|
| 41 | Do large operators obtain tracked reservations? | C |
| 42 | Does a reservation record owner? | C |
| 43 | Does it record held bytes? | C |
| 44 | Does it record spillability? | C |
| 45 | Is growth explicit? | C |
| 46 | Is bypass after denial forbidden? | C |
| 47 | Does release return accounted bytes? | C |
| 48 | Is reservation distinct from allocation? | F |
| 49 | Is the reservation charge point defined? | F |
| 50 | Does approval guarantee allocation capacity? | F |
| 51 | Can allocation occur before accounting? | F |
| 52 | Is post-grant allocation failure classified? | F |
| 53 | Is failed-allocation charge rollback defined? | F |
| 54 | Is partial allocation defined? | F |
| 55 | Is ownership transfer accounting atomic? | F |
| 56 | Is double release forbidden explicitly? | F |
| 57 | Is reservation underflow classified? | F |
| 58 | Is release idempotence required or once-only? | F |
| 59 | Are catchable allocator failures always controlled? | F |
| 60 | Is native address-domain refusal classified? | F |

### 61–80: arena and operator ownership

| # | Question | Status |
|---:|---|---|
| 61 | Is QueryArena query-owned? | C |
| 62 | Is its lifetime query-scoped? | C |
| 63 | Is allocation bump-oriented? | CS |
| 64 | Is bulk release defined? | C |
| 65 | Is it limited to small state/metadata? | C |
| 66 | Are unbounded hash tables excluded? | C |
| 67 | Are large RowCollections excluded? | C |
| 68 | Are sort runs excluded? | C |
| 69 | Are large vectors excluded? | C |
| 70 | Are DML/result spools excluded? | C |
| 71 | Is arena backing nonspillable? | C |
| 72 | Is arena backing accounted? | C |
| 73 | Can arena pointers outlive query end? | C—no |
| 74 | Does D23-S2 govern arena-backed borrows? | C |
| 75 | Is unbounded row-dependent arena use forbidden? | C |
| 76 | Is growth bounded independently of object count? | F |
| 77 | Do streaming operators own temporary state? | C |
| 78 | Do breakers declare spillability? | C |
| 79 | Are detailed algorithms delegated? | C |
| 80 | Is operator-state persistence forbidden? | C |

### 81–100: RowCollection and blocks

| # | Question | Status |
|---:|---|---|
| 81 | Is RowCollection query-temporary? | C |
| 82 | Is it append-oriented? | C |
| 83 | Are blocks owned by the collection/operator? | C |
| 84 | Are in-memory handles stable while backing lives? | C |
| 85 | Is bulk deallocation supported? | C |
| 86 | Is per-row allocation minimized? | CS |
| 87 | Are varlen blocks supported? | C |
| 88 | Is RowLayout derived from resolved types? | C |
| 89 | Is NULL state represented independently? | C |
| 90 | Are fixed and varlen regions distinguished? | C |
| 91 | Are raw movable-row pointers avoided? | C |
| 92 | Is insertion order SQL order? | C—no |
| 93 | Can block position become row identity? | C—no |
| 94 | Is 256 KiB a semantic maximum? | C—no |
| 95 | Is 256 KiB a tuning target? | C |
| 96 | Is an empty block state specified? | N/A |
| 97 | Is partial/full-block behavior semantically relevant? | N/A |
| 98 | Is an oversized single row handled? | F |
| 99 | Is a dedicated oversized block required or optional? | F |
| 100 | Is failure for an unsupported oversized row classified? | F |

### 101–120: retained values and lifetime

| # | Question | Status |
|---:|---|---|
| 101 | Does retained VARCHAR storage own bytes? | C |
| 102 | Can RowCollection keep only borrowed StringRef metadata? | C—no |
| 103 | Must input-chunk reset leave retained rows unchanged? | C |
| 104 | Does D23-S2 apply to retained borrowed values? | C |
| 105 | Can stable owner transfer replace copying? | C |
| 106 | Are exact embedded-NUL bytes preserved? | C |
| 107 | Is NULL independent of payload? | C |
| 108 | Can stale payload define a NULL value? | C—no |
| 109 | Is large VARCHAR truncation forbidden? | C |
| 110 | Is uint32 wrap forbidden? | C |
| 111 | Is heap tuple size a runtime row maximum? | C—no |
| 112 | Can a >UINT32_MAX exact alternate be retained? | F |
| 113 | Can lack of a RowCollection form cause controlled failure? | F |
| 114 | Is that failure’s exact category stated? | F |
| 115 | Are retained varlen bytes charged once? | F |
| 116 | Are associated varlen block headers charged? | F |
| 117 | Can inactive/released storage remain borrowed? | C—no |
| 118 | Are reloaded StringRefs given a new owner? | C |
| 119 | Can raw page pointers be retained? | C—no |
| 120 | Can result values outlive their owner without transfer? | C—no |

### 121–140: pressure and spill semantics

| # | Question | Status |
|---:|---|---|
| 121 | Is the soft budget advisory pressure? | C |
| 122 | Can soft pressure trigger spill early? | C |
| 123 | Is the hard limit an allocation gate? | C |
| 124 | Does denial distinguish spillable/nonspillable state? | C |
| 125 | Does nonspillable denial yield OOM? | C |
| 126 | Must spillable state release before retry? | C |
| 127 | Are callbacks outside accounting lock? | CS |
| 128 | Can callbacks release and re-request? | C |
| 129 | Is recursive lock coupling prohibited? | C |
| 130 | Is spill trigger timing semantically observable? | C—no |
| 131 | Can spill threshold alter successful values? | C—no |
| 132 | Can spill alter NULLs? | C—no |
| 133 | Can spill alter multiplicity? | C—no |
| 134 | Can spill alter required order? | C—no |
| 135 | Can spill introduce genuine I/O failure? | C |
| 136 | Is no-progress spill retry forbidden? | F |
| 137 | Is “nothing eligible to spill” handled? | F |
| 138 | Is repeated denial terminally classified? | F |
| 139 | Is recursive repartition bounded by operator owners? | C |
| 140 | Is a correctness fallback required where declared? | C |

### 141–160: spill format and I/O

| # | Question | Status |
|---:|---|---|
| 141 | Is spill data temporary? | C |
| 142 | Is it outside WAL? | C |
| 143 | Is it outside crash recovery? | C |
| 144 | Is it outside persistent format compatibility? | C |
| 145 | Is generic framing self-describing? | C |
| 146 | Is magic present? | C |
| 147 | Is a temporary version present? | C |
| 148 | Is operator/run/partition identity present? | C |
| 149 | Is payload length present? | C |
| 150 | Is row count present? | C |
| 151 | Is CRC32C present? | C |
| 152 | Is integer serialization explicit? | C |
| 153 | Is native object dumping forbidden? | C |
| 154 | Is raw pointer serialization forbidden? | C |
| 155 | Is structural mismatch SpillIOError? | C |
| 156 | Are lengths checked before allocation/access? | F |
| 157 | Are offsets checked before range formation? | F |
| 158 | Are count×width calculations checked? | F |
| 159 | Are file-offset calculations checked? | F |
| 160 | Is malformed decode memory-safe by explicit rule? | F |

### 161–180: errors and cleanup

| # | Question | Status |
|---:|---|---|
| 161 | Is spill read/write failure controlled? | C |
| 162 | Is temp disk exhaustion a spill I/O failure? | C |
| 163 | Is OOM distinct from SpillIOError? | C |
| 164 | Is representability failure distinct from OOM? | C |
| 165 | Does §39.1 own transaction consequences? | C |
| 166 | Does cleanup avoid independently committing/aborting? | C |
| 167 | Are normal-completion spill files cleaned? | C |
| 168 | Are error-path spill files cleaned? | C |
| 169 | Are cancellation spill files cleaned? | C |
| 170 | Are reservations unwound? | C |
| 171 | Are RowCollections unwound? | C |
| 172 | Are arena pages unwound? | C |
| 173 | Is operator state unwound? | C |
| 174 | Are pre-write retry resources released? | C |
| 175 | Is target/RETURNING retry state discarded? | C |
| 176 | Are crash leftovers necessarily removed? | F |
| 177 | Can stale files collide with a new query? | F |
| 178 | Can stale files be adopted as live state? | F |
| 179 | Are unrelated temp files protected? | C |
| 180 | Is cleanup failure transaction-fatal by itself? | C—no |

### 181–200: size, concurrency, and resource sharing

| # | Question | Status |
|---:|---|---|
| 181 | Are reservation additions checked? | F |
| 182 | Are reservation subtractions checked? | F |
| 183 | Are row byte totals checked? | F |
| 184 | Are block byte totals checked? | F |
| 185 | Are varlen offset+length extents checked? | F |
| 186 | Are spill record lengths checked? | F |
| 187 | Are block/run counts checked? | F |
| 188 | Are partition counts checked? | F |
| 189 | Are file offsets exactly addressable? | F |
| 190 | Is silent narrowing forbidden locally? | F |
| 191 | Do parallel reservations share one exact total? | C conceptually |
| 192 | Is the sum invariant explicit? | F |
| 193 | Can ordinary reservation races exceed the hard bound? | F |
| 194 | Are manager operations logically atomic? | C implied |
| 195 | Is a lock implementation mandated? | C—no |
| 196 | Is allocator overhead required to equal RSS? | N/A |
| 197 | Is OS RSS itself the architectural budget? | N/A |
| 198 | Is global fairness defined? | N/A |
| 199 | Are worker-local caches accounted? | F under Q24-1 |
| 200 | Can one query consume another’s reservation? | C—no by ownership |

### 201–220: persistence and determinism

| # | Question | Status |
|---:|---|---|
| 201 | Can RowCollection bytes become heap tuple bytes directly? | C—no |
| 202 | Can StringRef pointers enter spill? | C—no |
| 203 | Can Vector pointers enter spill? | C—no |
| 204 | Can QueryArena addresses enter spill? | C—no |
| 205 | Are reloaded values newly owned? | C |
| 206 | Does spill preserve exact VARCHAR bytes? | C |
| 207 | Does spill preserve NULL independently? | C |
| 208 | Does spill preserve FLOAT semantics? | C |
| 209 | Does spill preserve repeated rows? | C |
| 210 | Does generic spill define a second comparator? | C—no |
| 211 | Can block size alter successful SQL results? | C—no |
| 212 | Can budget alter successful SQL results? | C—no |
| 213 | Can budget alter resource success/failure? | C |
| 214 | Can spill threshold alter SQL meaning? | C—no |
| 215 | Can pointer address alter SQL meaning? | C—no |
| 216 | Can temp-file name alter SQL meaning? | C—no |
| 217 | Can reload order alter required order? | C—no |
| 218 | Can physical append order establish order? | C—no |
| 219 | Can one implementation resource-fail where another succeeds? | C within exact capability/resources |
| 220 | Is that permitted boundary fully specified? | F for Q24-1–Q24-5 |

### 221–240: handoffs, documentation, and closure

| # | Question | Status |
|---:|---|---|
| 221 | Does Ch17 remain scalar owner? | C |
| 222 | Does Ch20 remain bag/order owner? | C |
| 223 | Does Ch21 remain attempt owner? | C |
| 224 | Does Ch22 remain QEC owner? | C |
| 225 | Does Ch23 remain borrow/StringRef owner? | C |
| 226 | Does Ch25 receive expression-memory ownership cleanly? | C |
| 227 | Does Ch26 own pipeline cleanup scheduling? | C |
| 228 | Does Ch28 own join spill semantics? | C |
| 229 | Does Ch29 own aggregate spill semantics? | C |
| 230 | Does Ch30 own sort-run semantics? | C |
| 231 | Does Ch31 own result transfer/publication? | C |
| 232 | Does §39 own resource-error consequences? | C |
| 233 | Is there DEVELOPMENT procedure leakage? | C—none |
| 234 | Is there VERIFICATION procedure leakage? | C—none |
| 235 | Is there PROJECT_STATE leakage? | C—none |
| 236 | Is there history/devlog leakage? | C—none |
| 237 | Is project chronology absent? | F |
| 238 | Can every correctness question be implemented without invention? | F |
| 239 | Is Verification fully synchronized? | F |
| 240 | Is Chapter 24 ready to close? | F |

## Implementer-invention assessment

An implementer can answer the ownership, persistence, borrowing, operator specialization, and transaction-consequence questions without invention.

Correctness-sensitive invention is still required for:

- accounted-byte membership and measurement;
- reservation/allocation transitions;
- terminal spill/retry behavior;
- oversized rows and large retained values;
- checked arithmetic and decoder validation;
- crash-leftover namespace/reclamation.

Therefore the answer to the central review question is **no**: two nominally conforming implementations can differ in hard-limit enforcement, controlled resource failures, progress, memory safety, and temp-resource cleanup.

## Regression results

- Chapter 17 values: preserved; no SQL domain change identified.
- Chapter 20 bags/order: preserved; physical blocks/handles do not create semantic identity or order.
- Chapter 21 attempts: preserved; pre-write retries discard attempt-local spools.
- Chapter 22 runtime ownership: preserved.
- Chapter 23 borrowing/StringRef/runtime-persistence: preserved.
- Transaction semantics: unchanged and delegated to §39.1.
- Persistent formats: unchanged; spill remains temporary.
- Chapter 25 review: not started.

## Direct final answers

- Project chronology present? **Yes—2 phrases.**
- Current implementation narration? **No.**
- DEVELOPMENT-owned material? **No substantive leakage.**
- VERIFICATION procedure? **No.**
- PROJECT_STATE leakage? **No.**
- History/devlog leakage? **No.**
- Memory-budget ambiguity? **Yes.**
- Accounting ambiguity? **Yes.**
- Reservation ambiguity? **Yes.**
- Allocation ambiguity? **Yes.**
- QueryArena purpose/lifetime ambiguity? **No**, but its accounting closure depends on Q24-1.
- Unbounded-memory bypass ambiguity? **Yes.**
- RowCollection basic ownership ambiguity? **No.**
- Oversized-row ambiguity? **Yes.**
- Retained-VARCHAR ownership ambiguity? **No for ordinary values; yes for oversized representability.**
- Spill-eligibility ambiguity? **No at the generic declaration level.**
- Spill-format ambiguity? **Yes for checked domains/decoding, not for persistence status.**
- Spill-transparency ambiguity? **No when specialized operator contracts are applied.**
- Spill-error ambiguity? **Yes for unsafe/unrepresentable size construction; ordinary I/O is `SpillIOError`.**
- Temp-file cleanup ambiguity? **Yes after process crash.**
- Retry/cancellation cleanup ambiguity? **Cancellation no; spill-retry termination yes.**
- Runtime/persistent boundary ambiguity? **No.**
- Size-arithmetic ambiguity? **Yes.**
- Result-owner ambiguity? **No; Chapter 31 owns it.**
- Correctness-relevant implementer invention? **Yes.**
- Can Chapter 24 stand years later as canonical v1 Architecture? **Not yet.**

## Recommended next action

Perform a **frozen Chapter-24 semantic decision package** for Q24-1 through Q24-6.

Do not perform document cleanup or Verification synchronization first. After owner approval and semantic integration:

1. targeted Chapter-24 document-only cleanup for N24-1/N24-2;
2. Chapter-24 Verification synchronization;
3. only then declare Chapter 24 fully reviewed and closed.

Chapter 25 review remains **NOT STARTED**. No Verification synchronization, implementation, build, test, sanitizer, or benchmark occurred. Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.

# Q24-1–Q24-6 — DECISION PACKAGE COMPLETE

Six linked, independent v1 decisions are recommended for architecture-owner approval:

- D24-S1 — Complete conservative query-memory accounting
- D24-S2 — Continuously accounted memory ownership
- D24-S3 — Finite resource-pressure progress
- D24-S4 — Exact retained-row representation applicability
- D24-S5 — Checked runtime extents and safe spill validation
- D24-S6 — Spill namespace isolation and crash-leftover reclamation

No additional frozen semantic question was discovered. Chapter 24 remains not clean and not fully closed pending owner approval and semantic integration.

## Repository state

| Check | Initial | Final |
|---|---|---|
| Branch | `main` | `main` |
| HEAD | `04c527d4a7d56f3575b87124dbd88c8d5e175b03` | unchanged |
| Working tree | clean | clean |
| Index | clean | clean |
| `git diff --check` | — | passed |
| Task-created changes | none | none |

Historical review artifacts were not read, modified, moved, or staged.

## Architecture analyzed

Primary:

- [Chapter 24, §§24.1–24.11](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:19664)

Frozen owner context:

- Chapter 17: scalar domains, VARCHAR, generic owned values
- Chapter 20: bag/order and demanded execution
- Chapters 15/21: statement attempts, retry, RETURNING, publication consequences
- Chapter 22: `QueryExecutionContext` and runtime ownership
- Chapter 23: D23-S2 borrowing, D23-S3 representability, runtime/persistence separation
- Chapter 25: expression-state/result-memory consumer
- Chapter 26: pipeline progress, cancellation, retained ownership
- Chapters 28–31: join/aggregate/sort/DML retained and spill specialization
- [§39.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26376): transaction consequences
- [§39.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:26738): execution error taxonomy
- `docs/VERIFICATION.md`: existing V23 and execution resource/spill families, read-only

Frozen constraints include:

- VARCHAR remains an arbitrary finite byte string subject to owning resource limits.
- Runtime resource limits do not become SQL grammar/type limits.
- Successful executions preserve exact values, NULLs, multiplicity, required order, demanded errors, and transaction effects.
- Borrowed values remain alive and value-stable.
- No runtime pointer enters a persistent or spill encoding.
- Missing exact runtime representation is controlled `ExecutionError`; failed allocation for a supported exact representation is `OutOfMemory`.
- Spill I/O, temp-disk exhaustion, and malformed spill data use `SpillIOError`.
- Statement/transaction consequences remain exclusively governed by §39.1.

---

# Q24-1 — Complete accounting universe and measurement basis

## Ambiguity

The live text accounts ordinary process memory only “when large” or when held by tracked arenas/reservations. It does not define “large,” aggregate many small allocations, or specify what amount an allocation contributes to the hard gate.

That permits an implementation to keep row-, group-, task-, block-, run-, or partition-dependent growth outside the budget by dividing it into individually small allocations.

## Recommended accounting universe

| Category | Query-owned? | Potentially unbounded aggregate? | Recommended treatment |
|---|---:|---:|---|
| QueryArena backing pages | yes | yes, with query shape/state | charge backing capacity |
| DataChunk/vector owned buffers | yes | yes | charge owned capacity |
| StringHeap capacity | yes | yes | charge owner region |
| SelectionVector capacity | yes | yes | charge owner region |
| RowCollection fixed payload | yes | yes | charge block/region capacity |
| RowCollection varlen payload | yes | yes | charge block/region capacity |
| RowCollection headers/offset tables | yes | yes across blocks | include in block or separate owner charge |
| Hash directory and build rows | yes | yes | operator reservation |
| Aggregate keys/states/directories | yes | yes | operator reservation |
| Sort records/runs/merge buffers | yes | yes | operator reservation |
| DISTINCT state | yes | yes | operator reservation |
| DML/RETURNING spool while execution-owned | yes | yes | spool/accounted owner |
| Spill read/write buffers | yes | yes | query/operator charge |
| Reloaded spill blocks | yes | yes | operator/reload charge |
| Run/partition/block directories | yes | yes | owning reservation |
| Worker-local dynamic buffers/state | yes | potentially | query/operator charge |
| Fixed-size query-context bookkeeping | yes | independently bounded | explicit bounded exemption permitted |
| Immutable cached physical plan | generally not execution-owned | separately owned | excluded from query budget |
| BufferPool frames | no; BufferPool owner | separately bounded | excluded |
| Worker-pool thread stacks/infrastructure | scheduler/process owner | separately bounded | excluded |
| Transaction/lock-manager storage | transaction subsystem owner | separately governed | excluded from query budget |
| Hidden allocator metadata/OS paging overhead | not exactly measurable | operational overhead | no exact RSS charge required |

## Measurement basis

Alternatives:

- Requested bytes alone are too weak when an implementation deliberately commits larger reusable capacity.
- Exact allocator-reserved bytes or RSS are not portable or reliably observable.
- Usable/capacity bytes are correct when defined as implementation-controlled capacity committed to the query.
- Conservative owner-region accounting preserves implementation freedom.

Recommended basis:

> Charge no less than the implementation-controlled memory capacity committed and made available to the query-owned region.

This includes explicit vector capacity, block capacity, arena pages, offset arrays, internal headers the implementation controls, and visible allocator rounding. It does not require knowledge of hidden malloc metadata, virtual-memory page tables, resident-page status, or physical RSS.

Conservative over-accounting is permitted. Systematic under-accounting is forbidden.

## Many small allocations and metadata

An individual allocation’s size is irrelevant to exemption. If aggregate growth depends on rows, values, groups, blocks, runs, partitions, tasks, workers, retained results, or query shape, an accounted owner must cover it.

A parent page/block/arena charge may cover all contained objects. Per-object charging and duplicate charging are not required.

## QueryArena

QueryArena remains:

- query-owned;
- nonspillable;
- restricted to bounded-purpose metadata/state;
- charged by backing-page capacity.

It cannot become a loophole for unbounded row-, group-, run-, partition-, or retained-result data. Objects inside an already charged arena page need not carry separate charges.

## Hard-gate concurrency

For one growth grant, both applicable totals must be checked as one coherent accounting transition:

```text
query_live_charge + new_charge <= query_hard_limit
global_live_charge + new_charge <= global_hard_limit
```

The arithmetic is exact under D24-S5. Concurrent requests may use any synchronization mechanism, but cannot each pass stale checks and collectively oversubscribe a hard gate.

## Double accounting

Every owned capacity must be covered at least once in each applicable ledger. The same bytes need not be charged repeatedly through both a parent block and every child object.

Query and global totals are separate applicable ledgers, not accidental duplicate charging. Temporary conservative overlap during ownership transfer is permitted.

## Recommended D24-S1

**D24-S1 — Complete conservative query-memory accounting**

### Exact proposed normative wording

> Query-memory accounting is logical committed-capacity accounting; it is not an exact operating-system RSS limit.
>
> Every process-memory region that becomes query-owned and whose aggregate capacity can grow with query input, query shape, retained results, operator state, blocks, groups, runs, partitions, tasks, or worker-local execution state MUST be covered by a live accounted owner.
>
> The accounted charge for a region MUST be no smaller than the implementation-controlled memory capacity committed and made available to that query-owned region. Accounting MAY occur at arena-page, allocation, buffer-capacity, block, collection, reservation, or another owner-region granularity and MAY conservatively overcharge. Hidden allocator metadata, virtual-memory implementation details, and physical RSS need not be measured.
>
> Individually small allocations are not exempt when their aggregate growth is unbounded. QueryArena backing pages, owned vector/chunk/string capacity, retained-row and variable-length storage, dynamic operator metadata, spill I/O buffers, reloaded spill blocks, and query-owned worker-local dynamic storage participate through an applicable accounted owner.
>
> A parent region’s charge MAY cover contained objects; the same physical capacity need not be charged again for each contained object. No owned capacity may be omitted merely to avoid double accounting.
>
> An exemption is legal only when the memory is governed by another explicit architecture owner or its aggregate capacity is bounded independently of query input and execution growth. BufferPool frame capacity is outside query-memory accounting because the BufferPool is its separate bounded owner.
>
> For every ordinary growth grant, the per-query and global hard-gate totals are checked and updated as one logically atomic accounting transition. Concurrent requests MUST NOT collectively grant live accounted ownership beyond either applicable hard limit.

---

# Q24-2 — Reservation, allocation, ownership, and release

## Ambiguity

The live `MemoryReservation` wording conflates:

- a request;
- permission under a budget;
- physical allocation;
- currently owned capacity.

It does not define post-grant allocation failure, allocator rounding, transfer continuity, or release underflow.

## Conceptual distinctions

| Concept | Meaning |
|---|---|
| Request | Proposed increase; creates no charge or ownership |
| Grant | Budget authority and live accounting charge |
| Physical allocation | Attempt to obtain actual storage |
| Owned memory | Capacity exposed or retained as query execution state |
| Transfer | Change of lifetime/accounting owner without a gap |
| Release | One terminal removal of the charge when ownership ends |

A grant is not proof that allocation succeeded.

## Ownership before accounting

The canonical invariant is:

> Memory cannot become live query-owned capacity before a sufficient accounting charge exists.

Reserve-before-allocation is the normal realization. Another allocator ordering is permitted only if provisional storage:

- remains private and inaccessible as query state;
- is already covered by an existing charge or an explicitly bounded construction allowance;
- is immediately released if sufficient accounting cannot be obtained.

This prevents allocate-first from becoming an unbounded transient bypass.

## Allocator rounding and capacity growth

If a request for `G` produces implementation-controlled capacity `A > G`, the live charge must cover `A` before that capacity becomes query-owned.

The implementation may:

- obtain an enlarged grant;
- keep only capacity covered by the grant;
- discard/roll back the provisional allocation.

It may not charge `G` while exposing `A`.

For in-place growth, the same rule applies to the positive capacity delta.

## Allocation failure

For an exact supported and representable form:

- catchable allocation denial becomes controlled `OutOfMemory`;
- unused post-grant charge is released;
- existing owned capacity and its existing charge remain coherent;
- no partially initialized memory becomes live query state.

## Catchable versus non-catchable failure

The architecture can require controlled handling only while execution remains capable of returning an error.

- Catchable allocator denial or allocation exception: `OutOfMemory`.
- OS process kill, `SIGKILL`, or another non-catchable termination: outside the controlled-return guarantee.
- An implementation may not deliberately let an ordinary catchable allocation denial escape as an uncontrolled failure.

This fully replaces “where possible.”

## Address-domain failure

A mathematically valid extent that cannot be represented in the runtime allocation/address domain is not physical-memory exhaustion. It is controlled `ExecutionError` with a runtime size/address-representability/resource-limit cause under D24-S5.

## Release and double release

Each charge has one live accounting owner. Release happens exactly once when ownership ends and decrements each applicable total exactly once.

Double release, underflow, or releasing more capacity than owned is an internal invariant violation. No idempotent public API is mandated.

## Transfer

Transfer must preserve continuous coverage:

```text
old owner charged
    -> atomic/logically continuous handoff
new owner charged
```

A temporary conservative overlap is permitted. An unaccounted gap is not.

If ownership leaves the query boundary, the receiving Chapter-31 or other canonical owner must assume valid lifetime/resource ownership; this decision does not redefine result semantics.

## Recommended D24-S2

**D24-S2 — Continuously accounted memory ownership**

### Exact proposed normative wording

> A memory-growth request, an accounting grant, a physical allocation, and live query ownership are distinct concepts.
>
> A request creates neither a charge nor query-owned memory. A granted reservation or accounted capacity is budget authority; it is not proof that physical allocation succeeded.
>
> Memory MUST NOT become live query-owned capacity until a charge sufficient under D24-S1 is held against every applicable hard gate. Any implementation ordering is permitted only if provisional allocator storage remains private, is already covered by an applicable charge or explicit bounded construction allowance, and is immediately discarded when sufficient accounting cannot be established.
>
> Once memory becomes query-owned, its full implementation-controlled committed capacity remains continuously covered for the complete ownership interval. If allocation or growth commits more capacity than originally requested, the additional charge MUST be obtained before that capacity is exposed or retained as query state.
>
> If a catchable allocation for an exact supported and representable form fails after accounting approval, every unused grant from that attempt is released and execution reports `OutOfMemory`. A catchable allocation denial MUST NOT escape merely as an uncontrolled allocator/runtime failure.
>
> Failure because the exact requested extent is outside a supported runtime size or address domain is governed by D24-S5 and is not `OutOfMemory`. Non-catchable process termination is outside the guarantee that execution returns a controlled error.
>
> A live charge has one accounting owner. Release occurs exactly once when ownership ends and updates the query and global totals exactly once. Double release, accounting underflow, and release of unowned capacity are internal invariant violations.
>
> Ownership transfer MUST preserve continuous lifetime and accounting coverage. It may use a logically atomic handoff or temporary conservative overlap, but MUST NOT create an unaccounted ownership gap.

---

# Q24-3 — Finite spill/reclaim/retry progress

## Ambiguity

The current protocol says a spillable operator spills/releases state and retries. It does not define what happens when:

- no eligible state exists;
- spilling frees no useful memory;
- the same grant remains denied;
- recursive partitioning fails to reduce pressure;
- no exact fallback remains.

## Progress definition

A pressure action is relevant progress only when it changes state that can help satisfy the denied request, such as:

- reducing live accounted capacity;
- reducing the requested growth;
- replacing state with an exact lower-memory representation;
- advancing a finite spill/repartition/fallback state;
- freeing an eligible owner;
- switching to an already-authorized exact operator-local fallback.

Writing bytes without reducing or predictably reducing future memory demand is not sufficient by itself.

## Finite progress

No arbitrary numeric retry limit is required. Instead, every pressure cycle must do one of:

1. obtain the grant;
2. terminate with a controlled error or cancellation;
3. advance a well-founded finite pressure state relevant to the same demand.

The same equivalent request cannot be repeated indefinitely with unchanged relevant state.

## No eligible victim or fallback

If no exact progress-producing action remains, the request terminates with `OutOfMemory`. A spillable label is not a promise that every input can always be reduced enough.

## Failure distinctions

- Hard-gate denial with no exact progress path: `OutOfMemory`.
- Actual spill read/write, temp-storage, or ENOSPC failure: `SpillIOError`.
- Cancellation: `QueryCancelled`.
- Internal no-progress loop: invalid operator/resource-liveness implementation.

## Optimizer/replanning boundary

This decision does not introduce runtime reoptimization. The selected operator exhausts only exact actions in its existing execution contract.

Physical planning remains responsible for retaining legal alternatives when optional algorithms are ineligible. Q24-3 does not require switching to a different physical plan after execution starts.

## Recommended D24-S3

**D24-S3 — Finite resource-pressure progress**

### Exact proposed normative wording

> When a hard-gate growth request is denied, a spillable owner MUST attempt an applicable exact pressure action when its execution contract has one. The owner MAY choose among exact reclaim, spill, representation reduction, or operator-local fallback actions permitted by its existing contract.
>
> A retry is permitted only after the action changes state relevant to satisfying the denied request. Relevant progress includes reducing live accounted capacity, reducing the required charge, or advancing a well-founded finite exact spill/reclaim/fallback state that can make the request satisfiable.
>
> Execution MUST NOT repeat an equivalent denied request through an unbounded no-progress spill, reclaim, repartition, or retry cycle. No fixed global retry count is required, but every pressure cycle MUST either obtain the grant, terminate, or advance such a finite progress state.
>
> If no exact progress-producing action remains that can make the request satisfiable within the applicable hard limits, execution terminates with controlled `OutOfMemory`.
>
> Failure of an actual spill read, write, temporary-storage allocation, or temporary-storage capacity operation remains `SpillIOError`. Cancellation remains `QueryCancelled`.
>
> Pressure handling MUST preserve exact SQL values, NULL state, multiplicity, required order, demanded semantic errors, and transaction behavior. It MUST NOT approximate, truncate, deduplicate, reorder required output, or invent runtime reoptimization to obtain memory progress.

---

# Q24-4 — Oversized retained rows and values

## Ambiguity

The 256 KiB block size is described as a target, but ordinary `RowLayout` offset/length domains and behavior for one row larger than an ordinary block are undefined.

## Block target

The target is an allocation/performance choice, not:

- a SQL row-size maximum;
- a VARCHAR length;
- a query cardinality limit;
- a persistent tuple limit.

A row may exceed the target while still fitting an ordinary implementation’s exact block form.

## RowLayout applicability

An ordinary `RowLayout`/block representation is applicable only when its exact row size, offsets, lengths, and descriptors are representable under D24-S5.

If it is not applicable, an implementation may use:

- a dedicated oversized allocation;
- physically segmented storage;
- another RowLayout form;
- an operator-specific exact retained representation.

Physical segmentation is legal; semantic splitting of one SQL value or occurrence is not.

## SQL-domain regression

The persistent heap tuple limit does not apply to runtime/intermediate retained rows. No oversized runtime row is invalid SQL merely because it cannot be inserted into one ordinary RowCollection block.

## Large VARCHAR composition

D23-S3 solves the runtime scalar representation problem, not the retained-row problem. A `> UINT32_MAX` exact runtime VARCHAR can be retained only if the retaining owner also supports an exact length/storage representation.

Copying bytes into a narrower descriptor does not solve representability.

## Unsupported retained form

If no exact retained representation is supported for the valid row/value, retention fails with controlled `ExecutionError` carrying a runtime row/value-representability/resource-limit cause.

If an exact retained form exists but memory allocation fails, the result is `OutOfMemory`.

If an exact supported form is available to the retaining operation, failure cannot be caused merely by selecting a known-incapable ordinary block form.

## Recommended D24-S4

**D24-S4 — Exact retained-row representation applicability**

### Exact proposed normative wording

> `RowLayout`, ordinary `RowCollection` blocks, and their offset/length descriptors are runtime representations, not SQL row-size, VARCHAR-length, cardinality, or persistent tuple-format limits. The ordinary block-size target is not a correctness maximum.
>
> A retained row or value MUST NOT be truncated, wrapped, clipped, semantically split, or rejected as invalid SQL merely because it exceeds an ordinary block target or descriptor domain. Physical segmentation is permitted only when it preserves one exact logical row occurrence and each exact scalar value.
>
> An implementation MAY use a dedicated oversized allocation, an alternate exact row layout, an operator-specific retained form, or another exact representation preserving Chapters 17, 20, 22, and 23.
>
> An ordinary retained-row representation is inapplicable when it cannot exactly represent the required row size, value length, offset, or extent. If an exact supported retained representation is available to the retaining operation, execution MUST NOT fail merely because a known-incapable ordinary representation was selected.
>
> If no supported exact retained representation is available, retention fails with controlled `ExecutionError` carrying a runtime row/value-representability/resource-limit cause. If an exact retained representation is supported but its required allocation cannot be obtained, the failure remains `OutOfMemory`.
>
> The persistent heap-tuple size limit is not a universal runtime or retained-row maximum.

---

# Q24-5 — Checked arithmetic and safe spill validation

## Ambiguity

Chapter 24 lacks a universal exact rule for:

- row/block/varlen sizes;
- offset plus length;
- count times width;
- reservation additions/subtractions;
- block/run/partition counts;
- spill payload/record lengths;
- file offsets and I/O extents.

It also does not state that spill metadata must be validated before it controls allocation or access.

## Mathematical-domain-first rule

Every size, count, offset, and extent first denotes an exact nonnegative mathematical value. The implementation then proves that value representable in every domain that will consume it.

Permitted mechanisms include checked arithmetic, wider integers, prevalidated bounds, domain-specific exact representations, or equivalent methods.

Forbidden:

- signed overflow;
- unsigned wrap;
- modulo reduction;
- narrowing truncation;
- pointer-range overflow;
- file-offset truncation.

## Arithmetic matrix

| Calculation | Required result |
|---|---|
| `count × width` | exact before allocation/range construction |
| fixed bytes + varlen bytes | exact complete row extent |
| `offset + length` | exact and within owning payload |
| `live_total + charge` | exact before hard-gate comparison |
| charge subtraction | exact, no underflow |
| block count growth | exact or controlled failure |
| run/partition count | exact or controlled failure |
| spill payload length | exact and encodable |
| spill record/header + payload | exact |
| file offset + I/O length | exact and addressable |
| allocation extent/capacity rounding | exact before allocator call/exposure |

## Runtime representability

Before a value controls allocation, accounting, range construction, pointer arithmetic, serialization, seek, read, or write, it must be representable in the corresponding runtime or temporary-storage domain.

A valid in-memory demand outside every supported runtime size/address representation fails with controlled `ExecutionError` carrying a runtime size/address-representability/resource-limit cause.

A representable supported allocation that cannot obtain memory remains `OutOfMemory`.

## Spill offset classification

A spill record, run, or file extent that cannot be represented by the supported temporary encoding or file-offset/I/O domain produces `SpillIOError` with a spill range/addressability/resource cause. It is a failure of the spill-storage path, not invalid SQL or persistent corruption.

If another exact supported spill form is available to that owner, a known-incapable form cannot be selected merely to cause failure.

## Pre-access validation

Before decoded metadata controls allocation or access, validation must establish, as applicable:

- complete framing;
- recognized magic/version;
- representable payload and record lengths;
- representable row/block/run counts;
- exact fixed/count-derived extents;
- offsets and lengths contained within their owner payload;
- file offset and I/O extent representability;
- CRC and owner-specific structure.

The precise safe order is implementation-specific. No unvalidated decoded size may drive allocation, pointer arithmetic, dereference, or I/O range construction.

## Self-generated temporary state

Self-generation does not remove validation requirements. Partial I/O, process crash, stale leftovers, disk faults, external modification, and implementation defects can all produce malformed temporary bytes.

The requirement is memory-safe controlled handling; it does not introduce a general hostile-temp-file security or durable recovery contract.

## Error classification

- Malformed spill framing, checksum, range, count, or offset: `SpillIOError`.
- It is not persistent database corruption.
- An impossible accepted in-memory state created despite construction invariants: internal invariant failure.
- Reservation underflow/double release: internal invariant failure.

## Recommended D24-S5

**D24-S5 — Checked runtime extents and safe spill validation**

### Exact proposed normative wording

> Every Chapter-24 size, count, offset, capacity, extent, reservation-total, and count-derived byte calculation denotes an exact mathematical nonnegative value.
>
> Before such a value controls allocation, accounting, range construction, pointer arithmetic, serialization, seek, read, write, or dereference, the implementation MUST prove it exactly representable in every consuming runtime or temporary-storage domain and MUST prove every derived range lies within its owning extent.
>
> Silent signed overflow, unsigned wraparound, modulo reduction, narrowing truncation, pointer-range overflow, and file-offset truncation are forbidden. Implementations MAY use checked arithmetic, wider representations, prevalidated bounds, or another exact mechanism.
>
> Query/global grant addition and release subtraction use exact checked arithmetic. Accounting overflow MUST NOT produce a grant, and release underflow or double release is an internal invariant violation.
>
> A valid in-memory query demand outside every supported runtime size or address representation fails with controlled `ExecutionError` carrying a runtime size/address-representability/resource-limit cause. Failure to allocate a supported exact and representable extent remains `OutOfMemory`.
>
> A spill record, run, or file extent outside the supported temporary encoding or file-offset/I/O domain fails with `SpillIOError` carrying a spill range/addressability/resource cause. When another exact supported spill representation is available to the owner, a known-incapable form is inapplicable.
>
> Spill framing, version, lengths, counts, offsets, derived extents, and owner-specific structure MUST be validated before decoded values control allocation or memory/file access. CRC and structural validation apply within safely established ranges. Self-generated temporary data is not exempt from these checks.
>
> Invalid temporary spill framing, checksum, count, offset, range, or extent is `SpillIOError`, not persistent database corruption. A self-generated in-memory state that violates an already-established construction invariant remains an internal defect.

---

# Q24-6 — Spill namespace and crash-leftover reclamation

## Ambiguity

Normal cleanup is defined, but startup cleanup is optional. Namespace uniqueness, collision behavior, stale adoption, and required reclamation are not specified.

## Live namespace model

Each live spill object needs fresh exclusive temporary ownership sufficient to distinguish:

- database/engine instance where a directory is shared;
- process lifetime;
- query and statement attempt;
- run/partition/block object;
- stale crash leftovers.

These are conceptual ownership dimensions, not required filename fields or persistent IDs.

## Collision isolation and stale adoption

Creating a spill resource must establish fresh exclusive ownership.

A new query may not:

- adopt a pre-existing path as its own;
- overwrite it blindly;
- infer ownership from a reused query-local counter;
- treat stale bytes as valid current-query state.

On collision, the implementation must choose another fresh identity or fail safely. If the filesystem prevents fresh spill creation for a query, the operation reports `SpillIOError`.

Blind adoption/overwrite despite the ownership invariant is an internal defect.

## Normal cleanup

Success, error, cancellation, and abandoned-attempt retry teardown delete every spill resource still owned by that query/attempt. No resource transfers silently to a restarted attempt.

## Crash leftovers

After a process crash:

- memory ownership disappears;
- the query is aborted;
- files may remain;
- those files are garbage temporary resources only;
- they are never replayed, recovered, or used as database state.

## Eventual reclamation

Mandatory synchronous deletion of every stale file during database open is unnecessarily restrictive.

Best-effort cleanup alone is insufficient.

Recommended contract:

- immediate safety isolation and non-adoption;
- during continued healthy operation, the managed temp subsystem must eventually reclaim proven stale managed resources;
- initialization, periodic maintenance, namespace-local cleanup, or a combination may implement it;
- repeated crashes or external inability to access/delete files cannot be promised away, but healthy operation cannot knowingly permit unbounded stale accumulation.

## Unrelated-file protection

Deletion requires proof that the object:

1. belongs to the DBlusBlus managed spill namespace; and
2. has no live owner.

Unrelated user/temp files remain untouched.

## Recommended D24-S6

**D24-S6 — Spill namespace isolation and crash-leftover reclamation**

### Exact proposed normative wording

> Every live spill resource has fresh exclusive temporary ownership sufficient to prevent collision with another live query, statement attempt, process/database instance sharing the managed temp location, or stale crash leftover.
>
> Spill creation MUST establish fresh ownership. A new owner MUST NOT adopt, trust, or blindly overwrite a pre-existing spill object merely because a generated path, query-local counter, run identifier, or partition identifier collides. On collision, the implementation establishes another fresh identity or fails safely; failure to establish required temporary storage for a query is `SpillIOError`.
>
> Normal query and abandoned-attempt teardown delete every spill resource still owned by that query or attempt. A restarted statement attempt receives fresh spill ownership and MUST NOT inherit abandoned-attempt files as live state.
>
> Process termination may leave spill files behind, but those files are garbage temporary resources only. They are never WAL-logged, crash-recovered, adopted as new query state, or interpreted as persistent database state.
>
> During continued healthy operation, the managed temporary-storage subsystem MUST eventually reclaim spill resources proven stale so known crash leftovers cannot accumulate without bound. Reclamation MAY occur during temp-manager initialization, startup maintenance, periodic maintenance, namespace-local allocation, or another safe schedule; complete synchronous deletion during every database open is not required.
>
> Reclamation may delete only objects proven to belong to the DBlusBlus managed spill namespace and proven not to have a live owner. Unrelated files are untouched.
>
> Filename syntax, random or deterministic identity generation, directory layout, metadata representation, and reclamation schedule remain implementation-specific. No WAL, fsync, durable spill identity, UUID, or crash-recovery format is required.

---

# Error and resource taxonomy

| Condition | Classification |
|---|---|
| Hard-gate denial with no exact progress-producing action | `OutOfMemory` |
| Catchable allocation failure for supported exact representable form | `OutOfMemory` |
| In-memory runtime size/address extent unsupported | `ExecutionError` with representability/resource cause |
| No exact retained-row/value representation | `ExecutionError` with representability/resource cause |
| Spill read/write failure | `SpillIOError` |
| Temp disk exhaustion/ENOSPC | `SpillIOError` |
| Spill encoding/file-offset domain unsupported | `SpillIOError` with range/addressability/resource cause |
| Malformed spill framing/CRC/count/offset/range | `SpillIOError` |
| Cancellation | `QueryCancelled` |
| Reservation double release/underflow | internal invariant violation |
| Blind stale spill adoption/overwrite | internal invariant violation |
| Fresh spill creation fails safely on collision/filesystem error | `SpillIOError` |
| OS kill or non-catchable process termination | external process termination; no controlled-return promise |
| Resource error transaction consequence | §39.1 first-publication boundary |

M24-5 is fully resolved by D24-S2 and D24-S5: ordinary catchable allocation denial is controlled OOM; unrepresentable extent is controlled representability/resource failure; non-catchable termination is outside the returned-error guarantee.

# Decision interactions

| Interaction | Result |
|---|---|
| D24-S1 ↔ D24-S2 | S1 defines what is charged; S2 defines continuous coverage |
| D24-S1 ↔ D24-S5 | accounting totals and charges use exact checked arithmetic |
| D24-S2 ↔ D24-S5 | extent representability precedes grant/allocation ownership |
| D24-S2 ↔ D24-S3 | denied attempts leak no provisional charge; retries make fresh coherent checks |
| D24-S3 ↔ D24-S6 | pressure-created spill uses fresh ownership and cleans failed-attempt files |
| D24-S4 ↔ D24-S5 | exact row size determines ordinary-form applicability |
| D24-S4 ↔ D23-S3 | exact scalar representation and exact retained-row representation are separate requirements |

## Dependency graph

```text
D24-S1 ──> D24-S2 ──> D24-S3
   │           │
   └────> D24-S5 <──── D24-S4 <──── D23-S3
                         │
D24-S6 <──── spill creation/retry under D24-S3
```

They should remain six separate decisions. Combining them would obscure distinct owners and make later Verification less precise.

# Alternatives

| Question | Alternative | Compatibility/safety | Freedom | Recommend? |
|---|---|---|---|---:|
| Q24-1 | Track only individually large allocations | permits aggregate bypass | high but unsafe | no |
| Q24-1 | Charge every object exactly | safe but impractical/double-count prone | low | no |
| Q24-1 | Conservative owner-region/capacity accounting | safe, measurable, no RSS dependency | high | **yes** |
| Q24-1 | Explicit bounded/separate-owner exemptions | compatible when proved | high | **yes, subordinate** |
| Q24-2 | Reservation equals allocation | conflates failure states | low | no |
| Q24-2 | Strict reserve-before-allocator call | safe but overconstrains some allocators | medium | not canonical |
| Q24-2 | Allocate first, account later without restrictions | transient bypass | high but unsafe | no |
| Q24-2 | Continuous charge before live ownership | exact and representation-neutral | high | **yes** |
| Q24-3 | Unlimited spill/retry | livelock | high but unsafe | no |
| Q24-3 | Fixed global retry count | finite but arbitrary and algorithm-coupled | low | no |
| Q24-3 | Well-founded state progress then terminal OOM | finite and exact | high | **yes** |
| Q24-4 | Ordinary block as hard row maximum | narrows SQL/runtime domain | high but incompatible | no |
| Q24-4 | Mandatory dedicated oversized block | exact but overprescriptive | low | no |
| Q24-4 | Any exact alternate or controlled failure | preserves domains and capability freedom | high | **yes** |
| Q24-5 | Native unchecked arithmetic | unsafe | high but invalid | no |
| Q24-5 | Check only selected boundaries | leaves gaps | medium | no |
| Q24-5 | Exact-before-use extent contract | complete and mechanism-neutral | high | **yes** |
| Q24-6 | Best-effort cleanup | unbounded stale accumulation | high but insufficient | no |
| Q24-6 | Mandatory synchronous startup deletion | safe but overconstraining | low | no |
| Q24-6 | Fresh ownership, non-adoption, eventual proven-managed reclamation | safe and flexible | high | **yes** |

# Required decision matrices

## D24-S2 lifecycle matrix

| Case | Required outcome |
|---|---|
| Request denied | no new charge; no new live memory |
| Grant acquired | charge live; allocation not yet proven |
| Exact allocation succeeds | memory becomes owned under charge |
| Allocation returns larger capacity | enlarge charge before exposure or discard excess/allocation |
| Catchable allocation fails | release unused grant; OOM |
| Address-domain failure | release unused grant; D24-S5 ExecutionError |
| Ownership transfer | continuous charge, optional conservative overlap |
| Release | exactly once |
| Double release | internal invariant |
| Cancellation/error | all owned charges released through cleanup |

## D24-S3 pressure matrix

| State | Outcome |
|---|---|
| Reclaim frees enough | retry and grant |
| Partial reclaim advances finite state | retry permitted |
| Spill writes data but frees no useful capacity | not sufficient progress by itself |
| No spillable owner/state | OOM |
| Same denial with unchanged state | retry forbidden |
| Exact local fallback reduces demand | proceed |
| Spill I/O/ENOSPC | SpillIOError |
| Cancellation | QueryCancelled |

## D24-S4 oversized-row matrix

| State | Outcome |
|---|---|
| Row below target | ordinary exact form |
| Row equal to target | ordinary form if representable |
| Row above target but supported by ordinary form | legal |
| Ordinary descriptor overflows | ordinary form inapplicable |
| Exact alternate available | use exact form |
| No exact alternate | ExecutionError representability/resource |
| Exact alternate allocation fails | OOM |
| `>UINT32_MAX` VARCHAR | retain only through exact scalar and retained-row representations |

## D24-S6 lifetime matrix

| Event/state | Required result |
|---|---|
| Live spill file | fresh exclusive owner |
| Query success | delete owned spill |
| Ordinary error | delete owned spill |
| OOM | delete owned spill |
| SpillIOError | delete remaining owned spill |
| Cancellation | delete owned spill |
| Pre-write retry | delete abandoned-attempt spill |
| Process crash | files may remain as garbage only |
| Proven stale managed leftover | eventual reclamation |
| Unrelated file | never delete |
| Generated-name collision | no adoption/overwrite; choose fresh or fail safely |

# Persistence, transaction, implementation freedom, timelessness

- Persistence impact: **none**.
- Transaction impact: **none**.
- Page, tuple, WAL, catalog, RID, SchemaVer, and persistent scalar formats: unchanged.
- Spill remains temporary, non-WAL, non-recovered, and not a durable compatibility ABI.
- Resource errors continue to use §39.1’s publication boundary.
- No new runtime row identity or persistent spill identity is introduced.
- Accounting granularity, allocator, reservation representation, row layout, checked-arithmetic mechanism, spill algorithm, naming method, and reclamation schedule remain implementation choices.
- All proposed wording is timeless and contains no implementation-progress narration.

# Future integration surface

| Decision | Smallest semantic integration surface |
|---|---|
| D24-S1 | §§24.3–24.5, 24.9, 24.11 |
| D24-S2 | §§24.4–24.6, 24.10–24.11 |
| D24-S3 | §§24.6, 24.10–24.11 |
| D24-S4 | §§24.1–24.2, 24.11; minimal §39.3 retained-row representability synchronization |
| D24-S5 | §§24.1–24.2, 24.4–24.5, 24.8–24.11; minimal §39.3 extent/spill-range taxonomy synchronization |
| D24-S6 | §§24.7, 24.10–24.11 |

Chapter 26 does not require semantic modification for D24-S3: its pipeline progress and cancellation rules are already compatible. Chapter 25 remains untouched.

# Future Verification families

| Decision | Required future methodology |
|---|---|
| D24-S1 | complete owner inventory; many-small allocation aggregation; metadata growth; parent-region coverage; query/global concurrent grant oracle |
| D24-S2 | request/grant/allocation/ownership state machine; rounding; post-grant failure; transfer; release/underflow; attempt cleanup |
| D24-S3 | finite pressure state machine; zero-progress spill; no victim; eventual OOM; SpillIO/cancellation distinction |
| D24-S4 | symbolic oversized rows and lengths; alternate exact form; ordinary-form inapplicability; heap-limit negative; OOM distinction |
| D24-S5 | boundary/overflow matrix; reservation arithmetic; malformed lengths/counts/offsets; pre-allocation/access counters; file-offset domain |
| D24-S6 | concurrent collision; stale non-adoption; crash leftovers; eventual managed cleanup; unrelated-file preservation |

No Verification file was edited.

# Reread answers 1–112

## Q24-1

1. Yes.
2. Yes.
3. Yes.
4. No.
5. Yes.
6. Yes.
7. Yes.
8. Yes.
9. Yes.
10. Yes.
11. Yes.
12. Yes—only independently bounded or separately owned exemptions.
13. No.
14. Yes.
15. Yes.
16. No.
17. Yes.
18. No.

## Q24-2

19. Yes.
20. No.
21. Yes, only while provisional storage remains private and is already charged or explicitly bounded; it is released on denial.
22. Yes.
23. Yes.
24. Yes.
25. Yes.
26. Yes.
27. Yes.
28. Yes.
29. Yes.
30. Yes.
31. Yes.
32. Yes.
33. Yes.

## Q24-3

34. Yes.
35. Yes.
36. No.
37. Yes.
38. Yes.
39. Yes.
40. Yes.
41. Yes.
42. No; a well-founded finite progress contract is used.
43. No.

## Q24-4

44. Yes.
45. No.
46. No.
47. Yes.
48. No.
49. Yes.
50. Yes.
51. Yes.
52. Yes; physical segmentation remains permitted.
53. Yes.
54. No.
55. Yes—controlled ExecutionError representability/resource.
56. Yes—OutOfMemory.
57. Yes.

## Q24-5

58. Yes.
59. Yes.
60. Yes.
61. Yes.
62. Yes.
63. Yes.
64. Yes.
65. Yes.
66. Yes.
67. Yes.
68. Yes.
69. Yes.
70. Yes.
71. Yes.
72. No.
73. Yes—ExecutionError for runtime size/address representability; SpillIOError for spill storage/addressability.
74. Yes.
75. Yes.
76. Yes—spill encoding/file-offset inability is SpillIOError; general in-memory address-domain inability is ExecutionError representability/resource.

## Q24-6

77. Yes.
78. No.
79. No.
80. Yes.
81. No.
82. No.
83. No.
84. Yes.
85. Yes.
86. Yes, during continued healthy managed-temp operation.
87. No.
88. Yes.
89. Yes.
90. Yes.
91. Yes, subject to eventuality.
92. No.

## Cross-cutting

93. Yes.
94. Yes.
95. Yes.
96. Yes.
97. Yes.
98. Yes—none introduced.
99. Yes.
100. Yes.
101. No.
102. No.
103. Yes: N24-1 remains `OPEN / DOCUMENT-ONLY`.
104. Yes: N24-2 remains `OPEN / DOCUMENT-ONLY`.
105. No Verification edit occurred.
106. No new semantic question was found.
107. Yes.
108. Yes.
109. Yes.
110. Yes, after architecture-owner approval.
111. Yes; Chapter 25 remains unreviewed.
112. Yes; Phase 2 remains unauthorized.

# Final status

- D24-S1 through D24-S6: **RECOMMENDED; PENDING ARCHITECTURE-OWNER APPROVAL**
- B24-1/B24-2/M24-1–M24-5: decision coverage complete, not closed until approval/integration
- N24-1: **OPEN / DOCUMENT-ONLY**
- N24-2: **OPEN / DOCUMENT-ONLY**
- New frozen Chapter-24 semantic question: **NONE**
- Chapter 24: **SEMANTIC DECISIONS PENDING ARCHITECTURE-OWNER APPROVAL; NOT CLEAN; NOT FULLY CLOSED**
- Recommended next action after approval: **CHAPTER-24 SEMANTIC INTEGRATION**
- Chapter 25 review: **NOT STARTED**
- Verification edits: none
- Implementation: none
- Build/test/benchmark: none
- Phase 2: **NOT STARTED / NOT AUTHORIZED**
