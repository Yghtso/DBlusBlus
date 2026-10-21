# 41. Verification Requirements

Every major subsystem requires focused verification appropriate to its invariants.

The verification strategy includes:

- unit tests for local contracts and boundary conditions,
- deterministic property/randomized tests for stateful data structures and storage algorithms,
- crash tests around WAL/data-write boundaries,
- differential SQL tests against a reference database where the supported semantics overlap,
- concurrency stress tests for lock/latch ordering, structural modifications, buffer eviction, and transaction conflicts.

Property/randomized testing is particularly important for:

- B+ tree operations,
- page compaction,
- tuple serialization,
- MVCC visibility,
- recovery.

Recovery correctness MUST be tested with simulated failures rather than inferred solely from normal shutdown/reopen behavior.

Tests MUST be able to use intentionally small resource limits, including tiny buffer pools, when required to force eviction and boundary behavior.

## 41.1 Storage verification obligations

Storage verification MUST exercise the architectural properties that cannot be established by simple encode/decode happy paths, including:

- heap-page fill/boundary behavior,
- rejection of overlapping NORMAL/NORMAL, NORMAL/DEAD, and DEAD/DEAD retained tuple ranges,
- complete retained-DEAD validation, canonical reclaimed-DEAD form, exact UNUSED-list membership/cycle rejection, and REDIRECT_RESERVED rejection,
- checked-arithmetic failures for adversarial persisted counts, offsets, and lengths,
- compaction with stable SlotIds and unchanged retained tuple bytes,
- invalid slot/page access,
- persisted tuple round-trips across scalar, NULL, fixed/varlen, empty-VARCHAR, and unaligned layouts,
- raw page-file extension/read/write/reopen behavior,
- injectable short/error I/O paths,
- BufferPool eviction under capacity far smaller than the working set,
- dirty writeback,
- pin protection and pin-leak detection,
- CLOCK victim/second-chance behavior,
- concurrent read-latch and exclusive write-latch behavior,
- heap scans after reopen across many pages,
- stale-FSM candidate repair.

Physical slot reuse tests MUST follow the Chapter-14 grace protocol and persisted free-slot list; immediate DEAD-slot reuse is not a valid test expectation.

Detailed test fixtures and benchmark procedures are maintained in `VERIFICATION.md`; this chapter states the architecture-level verification obligations.


## 41.2 B+ tree verification obligations

B+ tree verification MUST cover the architectural failure surfaces created by variable-length keys, structural changes, duplicate physical keys, persistence, and concurrency.

Required deterministic coverage includes:

- insertion without split,
- leaf/internal/cascading/root split,
- redistribution in both directions,
- leaf/internal merge,
- root contraction,
- tree-local free-page reuse,
- negative/positive signed key ordering,
- integer extremes,
- DATE/TIMESTAMP ordering,
- FLOAT64 infinities,
- `-0.0`/`+0.0` canonical equivalence,
- NaN canonicalization,
- empty VARCHAR,
- embedded zero bytes in VARCHAR,
- composite keys,
- NULL-containing keys,
- 1024-byte encoded-user-key boundary,
- oversized-key rejection.

Corruption coverage also injects overlapping entry ranges, unsorted local keys/separators, invalid child/sibling/free PageNos, malformed sibling reciprocity/boundaries, free-list cycles and duplicate membership, live/free overlap, wrong root/height/endpoints, zero-filled published pages, and traversal progress-limit violations. Local violations must fail ordinary access without relying on the full verifier; the verifier must exhaustively detect global reachability, orphan, free/live, separator, depth, and sibling-graph failures.

Duplicate-heavy verification MUST place one SQL user key across multiple leaves and establish that:

```text
equality scan returns every RID exactly once
lower bound begins at the first duplicate
upper bound stops after the final duplicate
Erase(K,RID) removes only that physical entry
tree invariants survive redistribution/merge
```

It also injects/rejects duplicate stored copies of one exact `(encoded_user_key,RID)`, distinguishes an authorized idempotent physical retry from an unproven executor duplicate, and verifies that recovery redo does not invoke SQL UNIQUE enforcement.

Randomized persistent testing MUST compare the tree against a sorted oracle of physical `(encoded_user_key,RID)` keys across operations such as insert, exact erase, point/range lookup, and close/reopen.

Random seeds must be reproducible when failures occur.

Concurrent verification MUST stress:

- readers with writers,
- disjoint-range writers,
- hot-range writers,
- duplicate-heavy insertion,
- simultaneous split boundaries,
- split/merge churn,
- forward range scans during writes,
- deliberately small BufferPools,
- deadlock/restart behavior.

The full-tree verifier in §8.28 SHOULD be run frequently during deterministic, randomized, reopen, and concurrency testing.

## 41.3 Transaction, recovery, and reclamation verification obligations

Verification MUST exercise failure boundaries rather than only clean shutdown/reopen behavior.

Deterministic crash injection SHOULD cover boundaries around:

```text
WAL append / incomplete append
fdatasync
data-page pwrite
heap insert before index MTR
B+ MTR construction and publication
TXN_COMMIT durability
checkpoint construction/install
vacuum index cleanup
NORMAL -> DEAD
DEAD -> UNUSED
```

Deterministic non-crash fault injection MUST separately cover the §12.12 outcomes for:

```text
reservation/resource failure before mutation
record construction/encoding failure after no-flush acquisition
known append failure after one and after all provisional page mutations
append-outcome uncertainty and the noncontinuable gate
valid append followed by post-append publication failure
valid append followed by WAL pwrite/fdatasync failure
failure while restoring/truncating an unpublished appended tail
copied-writeback winning and no-flush winning their reservation race
```

The checks compare exact pre-operation bytes and frame metadata for both initially clean and initially dirty pages, including generation, `page_lsn`, `rec_lsn`, DPT membership, and FPI state. Ordinary PAGE_INIT tests verify immediate serialized tail truncation, no published count/reference, and deterministic PageNo reuse after a known failure. B+ MTR tests additionally verify all-page rollback, new/reused-page disposition, and exact root/height/sibling/parent/free-list restoration. Checkpoint/flush observers must see only the old state or the fully published WAL-backed state, never provisional metadata.

Database-lifecycle verification MUST additionally cover §3.3's active
exclusivity and every state/failure gate, including:

```text
two-process and same-process alias opens (exactly one owner)
DATABASE_BUSY before control/WAL inspection or recovery
process exit/crash releasing the OS lock
unsupported forked-handle use and close-on-exec behavior
one-valid/two-valid/no-valid control-slot opens and legal fallback
missing required WAL/bootstrap/committed files
torn txn-status recovery before ordinary lookup
crash/failure at every open/shutdown boundary in §§3.3.3 and 3.3.6, with §3.3.7 lifecycle outcomes
no transaction/background admission before READY
failed-open worker/descriptor/BufferPool/lock cleanup
orphan classification without deletion of unknown names
READY -> DRAINING preventing all new admission
ACTIVE/MUST_ABORT/COMMITTING/ABORTING shutdown handling
BufferPool drain and final checkpoint before WAL service stop
flush/checkpoint/control/directory-sync shutdown failures never reporting success
NONCONTINUABLE preventing ordinary work while retaining exclusivity through teardown
durable COMMIT survival across failed open/shutdown and next recovery
```

Database-root publication/adoption verification MUST cover the ownership and
durability boundaries in §§3.3.2–3.3.7 and §4.7.8, including:

```text
two-process and same-process competing open between final-root rename and
external-parent fsync
offline-remover competition while the creator owns publication
DATABASE_BUSY before control-slot/WAL/recovery inspection by every losing actor
creator process death and machine crash between rename and parent fsync
independent-owner parent-entry verification/fsync/revalidation before inspection
creator parent-fsync failure and reconciliation without false success or READY
continuous claim/control-lock ownership for a returned handle
no-handle ownership release only after durable root publication
```

Whole-database removal verification MUST cover §4.7.9's offline ownership,
retirement publication, and contained reclamation boundaries, including:

```text
competing open, removal, create, same-process, and pathname-alias operations
retirement rename before and after external-parent fsync
parent-fsync failure and next-owner namespace reconciliation
process and machine crash at every removal publication/cleanup boundary
database.control inode/lock continuity and no content deletion before retirement durability
removal of recognizable damaged supported-v1 roots
refusal of missing, unrecognizable, and unsupported-future-format roots
unknown files/directories, symlinks, safely unlinkable special entries, and
mount/cross-filesystem containment
idempotent cleanup crash/retry and recreation while retired residue remains
retired-token collision handling and exact retired-name recognition
no ordinary open of a retired root and no deletion of external-parent siblings
```

Statement/transaction error verification MUST cover every §39.1.3 matrix row
on both sides of the first persistent statement-write boundary where
reachable. Required scenarios include ordinary user-DML
scalar/assignment/NOT NULL/UNIQUE/RETURNING
candidate closure before any write; INSERT SELECT complete demanded-source
staging; physical-order-independent ordinary winner and FA outcome; dynamic
row-5 failure after four published INSERT rows; dynamic partial UPDATE/DELETE;
pre-write read-only/OOM/spill failure; §12.12 exact local rollback with and
without an earlier statement write; empty-page/structural publication without
a transaction-owned logical row effect; CommandId nonreuse; fresh READ
COMMITTED snapshot and completely fresh closure state after FA/retry;
rejection of same-TxnId retry after publication; explicit-transaction
RETURNING failure before exposure; and autocommit RETURNING held through
implicit COMMIT.

DML result-publication verification MUST distinguish pre-`R` execution failure
from post-`R` delivery failure. It establishes that `R` waits for successful
statement execution and, for autocommit, C4–C5. Autocommit cases prove that
every required fallible result-publication operation completes before C0;
physical allocation cannot be replaced by a budget reservation; C1 validates
already-prepared readiness; C5 preserves result backing, accounting, spill
handles, and cleanup ownership; and no required allocation, validation,
registration, handle duplication, spill I/O, callback, or cursor construction
remains between successful C and `R`. Coverage includes DML with and without
RETURNING, zero-row DML, every existing COMMIT failure/uncertainty branch,
cancellation after the authorizing append, process crash and session loss after
durable COMMIT but before `R`, and rejection of abort or replay after COMMITTED.
Explicit post-`R` spill-read failure leaves a usable transaction `ACTIVE`;
autocommit remains `COMMITTED`; affected-count authority and any delivered
prefix survive; cursor error is not `FINISHED`; delivery cancellation and
session loss retain separate owners; no published DML is retried; and result-
spool accounting and cleanup transfer exactly to the result owner.

COMMIT/ABORT fault injection MUST cover every C0–C6 and A0–A4 boundary, including known versus uncertain terminal-record append, repeated WAL-flush failure, connection loss before/after the commit append and durable point, post-durable runtime terminal-cache failure, lock/cache cleanup failure, abort-record failure, and acknowledgement transport failure. Assertions distinguish durable transaction outcome, runtime/database health, and client-observed outcome; no post-durable path may produce ABORTED.

Recovery property tests compare reopened **logical committed contents** against a model containing only transactions whose commit became durable. Physical aborted garbage is allowed.

Table-driven MVCC verification covers creator/deleter committed, active, too-new, aborted, self-command, and frozen cases using exact `xmin/xmax/cmin/cmax` combinations.

Isolation verification establishes READ COMMITTED stable-per-statement snapshots and retry re-evaluation, REPEATABLE READ fixed-snapshot behavior and serialization failure on conflicting post-snapshot writes, and the fact that snapshot-isolation write skew remains possible.

Logical/gate verification establishes same-target serialization, disjoint-target
concurrency, unique-key serialization, no physical-latch retention during any
transaction-level wait, complete unified-graph edges for SchemaLock, both
TableWriterGate modes, both object-publication modes, TUPLE_WRITE, and
UNIQUE_KEY, deterministic cross-resource deadlock victims, terminal-only gate
release, wakeup revalidation, and safe waiter cleanup. It includes every
§11.13.7 adversarial timeline and synthetic cycles spanning three or more
resource families.

Table-driven §11.10 verification covers ordinary UNIQUE and PRIMARY KEY across NULL/composite/FLOAT64 canonical keys; committed/frozen/aborted/nonterminal creators; absent/committed/aborted/nonterminal deleters; same-transaction earlier/current-command creators; earlier-command self-delete reuse versus current-command other-row conflict; same-statement INSERT duplicates; exact same-key UPDATE exclusion; order-independent another-row current-command UPDATE collision; immediate key-swap rejection; post-wait full recheck; stale physical entries; protected RID identity; the §39.1 pre-write violation outcome and rejection of post-write ordinary-violation discovery; and COMMIT/ABORT lock release only after terminal publication.

Vacuum/reclamation verification includes exact index cleanup before retirement, persistent DEAD restart behavior, grace-delayed RID reuse, version-chain splicing, and long-running snapshot interaction.

## 41.4 Catalog, front-end, and logical-plan verification obligations

Parser verification includes positive syntax/AST-shape cases, negative syntax/error-span cases, precedence/associativity cases, quoted/unquoted identifier behavior, string/comment termination errors, and multi-statement synchronization.

Binder/type verification includes unknown/ambiguous names, aliases/self-joins, qualified references, wildcard expansion, type promotion/casts, NULL typing/3VL, aggregate/GROUP BY legality, ORDER BY alias/ordinal, LEFT JOIN nullability, DML target binding, and subquery scopes.

Subquery verification is table-driven over every §20.14.1 support-matrix row
and includes local-only name lookup; derived aliases/names; scalar 0/1/2-row
behavior and second-row error precedence; EXISTS projection irrelevance and
early stop; IN/NOT IN empty/NULL/duplicate/NaN cases; lazy skipped branches;
same snapshot/CommandId; global/grouped aggregate, DISTINCT, LIMIT/OFFSET;
DML ordinary-error closure before write and independently owned dynamic
boundary cases; exact-empty rewrites; and rejection of every
correlated/row/quantified/LATERAL/set/data-modifying form.

Type property tests MUST compare binder semantics with constant evaluation, vectorized execution, and index-key comparison where the same type participates.

Catalog verification includes:

```text
bootstrap/open
reopen persistence
normalized/quoted name lookup
stable non-reused object IDs
historical schema lookup
snapshot-aware cache behavior
transactional DDL visibility
CREATE abort orphan handling
DROP delayed retirement
descriptor invalidation without in-place mutation
```

Logical-plan tests assert canonical plan shapes from bound statements.

Every rewrite rule has an input logical plan and expected transformed plan and, once execution exists, differential semantic tests with NULL-rich data and volatility-sensitive cases.

Logical validators are exercised both before and after rewrite phases.

Fuzzing targets at least:

```text
lexer
parser
literal/type conversion
bound constant evaluator
```

with no crashes/out-of-bounds access and bounded failure behavior on arbitrary input.

## 41.5 Physical-execution verification obligations

Before execution, a physical-plan validator checks at least:

```text
child/output LogicalSlotId compatibility
physical expression types
required hidden target RID presence
join-key type compatibility
requested join-type support
ordering-property consistency
source/streaming/sink pipeline legality
breaker dependency legality
memory/spill capability declarations
required transaction/query context
empty-result/no-op operators carry approved semantic-empty proof provenance
and are not justified by numerical estimate/statistics provenance
every accepted expression subquery has one supported semantic mode, no
OuterRef, one occurrence-keyed lazy side plan or a proven §20.14.11 rewrite,
and declared scalar-two-row/EXISTS-one-row/IN-complete-build behavior
derived-table slot/name boundaries and any removal preserve the exact mapping
```

Validation occurs before data-changing side effects.

Execution verification includes synthetic operator/chunk tests, manual physical-pipeline tests, SQL end-to-end tests, differential tests where semantics align, forced-spill tests under tiny query budgets, cancellation tests, and single-worker/parallel equivalence tests.

Write-DML execution verification MUST establish that the complete semantically
demanded ordinary candidate domain closes before the first persistent
user-DML write. It covers INSERT VALUES and spill-staged INSERT SELECT;
authoritative post-wait UPDATE/DELETE old images; simultaneous assignment and
complete new-row construction; demanded pre-write RETURNING; statement-wide
canonical UNIQUE_KEY acquisition; exact collision-safe pending owners for
same-statement duplicates; exact old-RID UPDATE exclusion; immediate key-swap
rejection; and no new ordinary candidate after publication. Varying input,
target, vector, worker, hash, spill, lock, and mutation order MUST preserve the
ordinary winner and pre-write FA consequence. Dynamic conflict, deadlock,
resource, cancellation, storage, corruption, and invariant failures remain
tested at their actually reachable boundary side rather than being inserted
into the ordinary candidate ranking.

Result-interface verification covers atomic §31.9 `R` publication for DML with
and without RETURNING; zero-row DML; spool append/finalization and required
allocation failure before autocommit C0; C1 prepared-state validation; C5
lifetime survival; the absence of ordinary fallible required work between C
and `R`; first-read and later-read failures after `R`; non-retraction and
returned-chunk lifetime; authoritative count even when delivery fails; terminal
cursor error distinct from successful exhaustion; voluntary cursor abandonment;
pre-C0, COMMIT-owned, and post-`R` cancellation; independently owned crash and
session loss; continuous memory accounting; and cleanup of result-owned buffers
and spill resources without statement reexecution or transaction-lock release.

Vector kernels are exercised over:

```text
FLAT / CONSTANT / DICTIONARY
all-valid / all-NULL / mixed validity
identity and non-identity selection
nested dictionary normalization
empty and full-size chunks
```

String-lifetime tests deliberately unpin source pages and recycle source chunks before downstream blocking use.

Join tests cover:

```text
no/one/many matches
many-to-many duplicate keys
NULL/composite keys
forced hash collisions
residual predicates
LEFT unmatched rows
one probe row producing >1 chunk
Grace spill
skew/repartition fallback
```

Randomized small join results are compared with the nested-loop reference algorithm.

Aggregate verification covers empty input, global/grouped aggregation, NULL
grouping keys, all-NULL inputs, composite/VARCHAR/FLOAT64 keys, exact
integer/count and canonical MIN/MAX Combine, legal FLOAT64 partial trees,
canonical FLOAT64 result representation, and every §29.3.8 boundary vector.
For each one logical input it compares one/two/many workers, one-row and large
vectors, serial and varied Merge trees, no spill and forced spill, different
spill partition counts, hash aggregation, and capability-enabled ordered
aggregation. Exact aggregates, schemas, classes, canonical representatives,
special-input classification, and numerical errors must be identical.
FLOAT64 SUM/AVG results instead must each be independently reproducible by an
admitted §29.3.4 tree over exactly the demanded occurrences; tests do not
use one physical execution as another's oracle. Tests may inject synthetic
near-boundary exact integer/count states instead of materializing impossible
row counts.

Sort tests cover ASC/DESC, NULL order, multi-key ties, VARCHAR/FLOAT64 edges, in-memory/external runs, and multi-pass merge using the semantic comparator as oracle.

DML execution tests specifically prove Halloween protection and one-target-once behavior, including index-key-changing UPDATE, target-spool spill, concurrent revalidation, the Chapter-15 READ COMMITTED retry boundary, REPEATABLE READ conflict abort, unique-key updates, and RETURNING suppression on failed/restarted attempts.

Forced-spill/cancellation/error tests verify temporary resources are cleaned without corrupting the transaction/storage layer.

## 41.6 Statistics, estimator, and base-access verification obligations

Statistics verification covers at least:

```text
exact small-table NDV/frequencies
HLL error distribution
SpaceSaving heavy-hitter capture
reservoir/histogram boundary monotonicity
NULL exclusion from NDV
MCV + residual mass consistency
VARCHAR/FLOAT ordering consistency
stable immutable stats publication during concurrent ANALYZE/planning
aborted ANALYZE does not publish globally
statistics chunk missing/duplicate/reordered corruption
statistics payload CRC/version/scalar-codec validation
physical index entry pressure versus live-row baseline
the exact §34.14.6 probability endpoints/epsilon boundary, dyadic summation,
MCV uniqueness/order, histogram order/mass, degenerate representations, and
generation-atomic fallback vectors
```

Cardinality-estimator tests use synthetic distributions with analytically known answers or reference counts for:

```text
equality / MCV hits
ranges / histogram interpolation
IS NULL / IS NOT NULL
IN lists with and without NULL list members
exact primitive TRUE/FALSE/UNKNOWN triples
NOT / AND / OR under SQL 3VL, including independence formulas
same-column contradictions
unique-key joins
MCV-skewed joins
DISTINCT / GROUP BY NDV
LEFT JOIN lower bound
```

Estimator quality is measured with q-error or an equivalent multiplicative error metric, while exact-zero/provably-empty cases are tested separately.

Semantic-proof tests independently cover at least:

```text
estimated_rows == 0 while is_provably_empty == false
estimated TRUE fraction == 0 from stale statistics without proof
constant FALSE and constant UNKNOWN in WHERE/HAVING/INNER JOIN contexts
LEFT JOIN with empty right or exact never-TRUE ON still preserving left rows
zero-row LogicalValues and exact contradictory literal constraints
NOT NULL proof versus null_fraction values of 0 and 1
analyzed_live_row_count == 0 followed by committed visible insertion
stale min/max equality and range values outside the analyzed domain
MCV/histogram/HLL absence without semantic absence
statistically disjoint join domains followed by visible overlap
grouped versus global aggregate over a provably empty child
actual LIMIT 0 versus required_rows/FIRST_K_ROWS(0)
zero index live/physical-entry estimates without skipping runtime access
```

Base-access tests compare enumerated SeqScan/IndexScan alternatives across:

```text
selectivity
relation size
required column width
index/heap correlation
dead-version pressure
physical B+ garbage/invisible-entry pressure
cache configuration
```

and assert that no fixed selectivity threshold controls the decision.

Composite-bound tests compare B+ cursor results against the normalized SQL predicate for inclusive/exclusive, duplicate RID, NULL, and multi-column leftmost-prefix cases.

## 41.7 Join-search, properties, memo, and optimizer verification obligations

Optimizer verification includes controlled scenarios for:

```text
bushy join-order choice
large-join bounded heuristic transition
Cartesian-product legality
LEFT JOIN search constraints
both INNER hash-build orientations
INLJ selective/small-outer cases
interesting-order retention
index-order ORDER BY avoidance
Sort enforcement
Top-N alternatives
HashAggregate versus capability-enabled ordered aggregate
hash versus ordered DISTINCT
memory-budget-driven spill/plan changes
required_rows / LIMIT startup objective
missing-statistics fallback/confidence/provenance
deterministic ties and plan fingerprints
planning-memory fallback
estimated-zero versus semantic-empty memo/plan behavior
final validation rejection of statistics-derived empty replacement
```

Synthetic join-estimation suites include:

```text
unique-to-many
many-to-many
hot-key skew
disjoint domains
partial domain overlap
NULL-heavy keys
duplicate-heavy MCVs
```

and compare estimator q-error with known/reference results.

For small generated schemas/data, differential optimizer correctness tests:

1. generate a supported logical query,
2. execute the optimizer-selected physical plan,
3. execute a trusted simple reference plan using semantically equivalent scans/nested loops/straightforward operators,
4. compare results.

This separates rewrite/search correctness from cost quality.

Optimizer fuzzing covers:

```text
logical expression trees
predicate combinations
legal statistics values
join graphs
ordering requirements
memory budgets
```

and requires:

```text
no crash
no NaN/negative cost escaping
no invalid final physical plan
bounded planning behavior above the exhaustive threshold
```

Plan regression scenarios record:

```text
schema
statistics
query
important expected plan properties
plan fingerprint where stable
```

without asserting arbitrary exact floating cost numbers unless the cost formula itself is under test.
