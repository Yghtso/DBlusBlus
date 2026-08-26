# Coverage audit result

1. Overall verdict: [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md) provides strong high-level coverage, but it does not yet operationalize much of the detailed contract in [ARCHITECTURE.md §41.3–§41.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:24083). The largest gaps concern lifecycle failure handling, transaction terminal-state matrices, subqueries, plan validation, statistics publication, and semantic emptiness.

Across 208 separately mapped obligations:

- COMPLETE: 48
- PARTIAL: 40
- MISSING: 120
- INDIRECT: 0
- CONTRADICTORY: 0

These counts reflect deliberately fine-grained architecture obligations, not an implementation-completeness score.

## 2–6. Counts by architecture section

| Architecture section | Complete | Partial | Missing | Indirect | Contradictory |
|---|---:|---:|---:|---:|---:|
| §41.3 Transaction, recovery, reclamation | 15 | 8 | 48 | 0 | 0 |
| §41.4 Catalog, front end, logical plan | 13 | 5 | 14 | 0 | 0 |
| §41.5 Physical execution | 7 | 8 | 20 | 0 | 0 |
| §41.6 Statistics, estimator, base access | 3 | 10 | 24 | 0 | 0 |
| §41.7 Join search, properties, memo, optimizer | 10 | 9 | 14 | 0 | 0 |

## 7. Master obligation matrix

Locations below refer to headings in [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md). Priorities apply only to PARTIAL/MISSING rows.

Documentation styles:

- **Explicit procedure**: describe setup, injection/input, and assertions.
- **Checklist + §xref**: enumerate cases and point to the architecture matrix.
- **Cross-reference only**: require direct execution of an architecture-owned exhaustive matrix.

### §41.3 — Transaction, recovery, and reclamation

| ID | Architecture | Capability | Obligation | VERIFICATION location | Status | Missing detail / priority | Later style |
|---|---|---|---|---|---|---|---|
| T-001 | §41.3 | Crash injection | WAL append and incomplete append | Crash Injection Framework | COMPLETE | — | — |
| T-002 | §41.3 | Crash injection | `fdatasync` boundaries | Crash Injection Framework | COMPLETE | — | — |
| T-003 | §41.3 | Crash injection | Data-page `pwrite` boundaries | Crash Injection Framework | COMPLETE | — | — |
| T-004 | §41.3 | Crash injection | Heap insert before index MTR | Crash Injection Framework | COMPLETE | — | — |
| T-005 | §41.3 | Crash injection | B+ MTR construction/publication | Crash Injection Framework | COMPLETE | — | — |
| T-006 | §41.3 | Crash injection | TXN_COMMIT durability boundary | Crash Injection Framework | COMPLETE | — | — |
| T-007 | §41.3 | Crash injection | Checkpoint construction versus installation | Crash Injection Framework | PARTIAL | MEDIUM — only generic “during checkpoint” | Checklist + §xref |
| T-008 | §41.3 | Crash injection | Vacuum cleanup and both retirement transitions | Crash Injection Framework | COMPLETE | — | — |
| T-009 | §41.3, §12.12 | Non-crash faults | Reservation/resource failure before mutation | None | MISSING | HIGH | Explicit procedure |
| T-010 | §41.3, §12.12 | Non-crash faults | Encoding failure after no-flush acquisition | None | MISSING | HIGH | Explicit procedure |
| T-011 | §41.3, §12.12 | Non-crash faults | Known append failure after one/all provisional mutations | None | MISSING | HIGH | Explicit procedure |
| T-012 | §41.3, §12.12 | Non-crash faults | Append uncertainty and NONCONTINUABLE transition | None | MISSING | HIGH | Explicit procedure |
| T-013 | §41.3, §12.12 | Non-crash faults | Valid append followed by publication failure | None | MISSING | HIGH | Explicit procedure |
| T-014 | §41.3, §12.12 | Non-crash faults | Valid append followed by WAL write/sync failure | None | MISSING | HIGH | Explicit procedure |
| T-015 | §41.3, §12.12 | Non-crash faults | Tail restoration/truncation failures | None | MISSING | HIGH | Explicit procedure |
| T-016 | §41.3, §12.12 | Non-crash faults | Copied-writeback/no-flush reservation race | None | MISSING | HIGH | Explicit procedure |
| T-017 | §41.3 | Rollback state | Exact page bytes and clean/dirty frame metadata | None | MISSING | HIGH | Explicit procedure |
| T-018 | §41.3 | PAGE_INIT rollback | Truncation, references/counts, deterministic PageNo reuse | None | MISSING | HIGH | Explicit procedure |
| T-019 | §41.3 | B+ MTR rollback | All pages, disposition, tree/root/sibling/parent/free-list state | None | MISSING | HIGH | Explicit procedure |
| T-020 | §41.3 | Publication atomicity | Observers see old or fully published state only | None | MISSING | HIGH | Explicit procedure |
| T-021 | §41.3, §3.3 | Lifecycle | Competing process/alias opens and early DATABASE_BUSY | None | MISSING | HIGH | Explicit procedure |
| T-022 | §41.3, §3.3 | Lifecycle | Crash lock release, fork restrictions, close-on-exec | None | MISSING | HIGH | Explicit procedure |
| T-023 | §41.3, §3.3 | Lifecycle | One/two/no-valid control slots and fallback | None | MISSING | HIGH | Explicit procedure |
| T-024 | §41.3, §3.3 | Lifecycle | Missing files/WAL and torn txn-status recovery | None | MISSING | HIGH | Explicit procedure |
| T-025 | §41.3, §3.3.7 | Lifecycle | Failure/crash at every open/shutdown transition | None | MISSING | HIGH | Cross-reference only |
| T-026 | §41.3, §3.3 | Lifecycle | No transaction/background admission before READY | None | MISSING | HIGH | Explicit procedure |
| T-027 | §41.3, §3.3 | Lifecycle | Failed-open cleanup of workers, descriptors, pool, locks | None | MISSING | HIGH | Explicit procedure |
| T-028 | §41.3, §3.3 | Lifecycle | Orphan classification; unknown names not deleted | None | MISSING | HIGH | Explicit procedure |
| T-029 | §41.3, §3.3 | Shutdown | Draining and all transaction-state handling | None | MISSING | HIGH | Checklist + §xref |
| T-030 | §41.3, §3.3 | Shutdown | Pool drain, checkpoint, WAL-stop ordering | None | MISSING | HIGH | Explicit procedure |
| T-031 | §41.3, §3.3 | Shutdown | Flush/checkpoint/control/directory-sync failures | None | MISSING | HIGH | Explicit procedure |
| T-032 | §41.3, §3.3 | Lifecycle | NONCONTINUABLE work gate and retained exclusivity | None | MISSING | HIGH | Explicit procedure |
| T-033 | §41.3, §3.3 | Lifecycle | Durable commit survives failed lifecycle and recovery | Recovery Property Tests | PARTIAL | HIGH — generic recovery model lacks lifecycle failures | Explicit procedure |
| T-034 | §41.3, §39.1 | Statement errors | Full before/after-first-published-write matrix | None | MISSING | HIGH | Cross-reference only |
| T-035 | §41.3, §39.1 | Statement errors | Read/cast/constraint/OOM/spill/resource failures | None | MISSING | HIGH | Checklist + §xref |
| T-036 | §41.3, §39.1 | Statement errors | Row-5 INSERT and partial UPDATE/DELETE | None | MISSING | HIGH | Explicit procedure |
| T-037 | §41.3, §12.12 | Statement errors | Exact local rollback, with/without earlier statement write | None | MISSING | HIGH | Explicit procedure |
| T-038 | §41.3, §39.1 | Statement errors | Structural publication without logical row effect | None | MISSING | HIGH | Explicit procedure |
| T-039 | §41.3, §39.1 | Statement retry | CommandId, fresh snapshot, no same-TxnId retry | Isolation Tests | PARTIAL | HIGH — re-evaluation only broadly mentioned | Checklist + §xref |
| T-040 | §41.3, §39.1 | RETURNING | Explicit/autocommit exposure boundaries | DML Execution Tests | PARTIAL | HIGH — buffering mentioned without transaction matrix | Explicit procedure |
| T-041 | §41.3, §15.5 | COMMIT | C0/C1 pre-terminal-record states | None | MISSING | HIGH | Cross-reference only |
| T-042 | §41.3, §15.5 | COMMIT | C2 known/uncertain append and publication | None | MISSING | HIGH | Explicit procedure |
| T-043 | §41.3, §15.5 | COMMIT | C3 repeated flush failure/COMMITTING retention | None | MISSING | HIGH | Explicit procedure |
| T-044 | §41.3, §15.5 | COMMIT | Connection loss around append and durable point | None | MISSING | HIGH | Explicit procedure |
| T-045 | §41.3, §15.5 | COMMIT | C4/C5 post-durable cache and cleanup failure | None | MISSING | HIGH | Explicit procedure |
| T-046 | §41.3, §15.5 | COMMIT | C6 acknowledgement transport failure | None | MISSING | HIGH | Explicit procedure |
| T-047 | §41.3, §15.5 | COMMIT | Durable/runtime/client outcomes; never post-durable ABORTED | None | MISSING | HIGH | Checklist + §xref |
| T-048 | §41.3, §15.6 | ABORT | A0 construction boundary | None | MISSING | HIGH | Cross-reference only |
| T-049 | §41.3, §15.6 | ABORT | A1 append boundary | None | MISSING | HIGH | Explicit procedure |
| T-050 | §41.3, §15.6 | ABORT | A2/A3 WAL failure and terminal publication | None | MISSING | HIGH | Explicit procedure |
| T-051 | §41.3, §15.6 | ABORT | A4 cleanup, lock release, acknowledgement | None | MISSING | HIGH | Explicit procedure |
| T-052 | §41.3 | Recovery properties | Random work, crash, reopen, durable-only model | Recovery Property Tests | COMPLETE | — | — |
| T-053 | §41.3 | MVCC | Creator state matrix | MVCC Visibility Tests | COMPLETE | — | — |
| T-054 | §41.3 | MVCC | Deleter and exact header combinations | MVCC Visibility Tests | COMPLETE | — | — |
| T-055 | §41.3 | Isolation | READ COMMITTED snapshots/re-evaluation | Isolation Tests | COMPLETE | — | — |
| T-056 | §41.3 | Isolation | REPEATABLE READ fixed snapshot/conflict | Isolation Tests | COMPLETE | — | — |
| T-057 | §41.3 | Isolation | Snapshot-isolation write skew | Isolation Tests | COMPLETE | — | — |
| T-058 | §41.3 | Locks | Same/disjoint targets, unique keys, no latch wait | Locking Tests | COMPLETE | — | — |
| T-059 | §41.3, §11.13 | Gate graph | All Schema/TableWriter/object/TUPLE/UNIQUE edges | Locking Tests | MISSING | HIGH — generic deadlock tests are insufficient | Checklist + §xref |
| T-060 | §41.3, §11.13 | Gate graph | Cross-resource victim, terminal release, revalidation | Locking Tests | PARTIAL | MEDIUM — victim/release covered only generically | Checklist + §xref |
| T-061 | §41.3 | Locks | Cancelled waiter cleanup | Locking Tests | COMPLETE | — | — |
| T-062 | §41.3, §11.13.7 | Deadlocks | All adversarial timelines and 3-family cycles | Locking Tests | MISSING | HIGH | Cross-reference only |
| T-063 | §41.3, §11.10 | UNIQUE | NULL/composite/FLOAT64 key domains | None | MISSING | HIGH | Checklist + §xref |
| T-064 | §41.3, §11.10 | UNIQUE | Creator/deleter status matrix | None | MISSING | HIGH | Cross-reference only |
| T-065 | §41.3, §11.10 | UNIQUE | Command visibility and same-statement duplicates | None | MISSING | HIGH | Checklist + §xref |
| T-066 | §41.3, §11.10 | UNIQUE UPDATE | Same-key exclusion, collisions, key swap | None | MISSING | HIGH | Explicit procedure |
| T-067 | §41.3, §11.10 | UNIQUE recheck | Wait/recheck, stale entries, protected RID | None | MISSING | HIGH | Explicit procedure |
| T-068 | §41.3, §11.10 | UNIQUE failures | Before/after-write outcomes and terminal release | Locking/DML Tests | PARTIAL | HIGH — no UNIQUE-specific matrix | Checklist + §xref |
| T-069 | §41.3 | Vacuum | Index cleanup before retirement; DEAD survives restart | Crash/Vacuum sections | PARTIAL | HIGH — transition points named, assertions absent | Explicit procedure |
| T-070 | §41.3 | Reclamation | Grace-delayed reuse and both state transitions | Storage/Crash sections | PARTIAL | HIGH — transition tests lack reuse/grace protocol | Explicit procedure |
| T-071 | §41.3 | Vacuum | Version-chain splice and long-running snapshots | None | MISSING | HIGH | Explicit procedure |

### §41.4 — Catalog, front end, and logical plan

| ID | Architecture | Capability | Obligation | VERIFICATION location | Status | Missing detail / priority | Later style |
|---|---|---|---|---|---|---|---|
| C-001 | §41.4 | Parser | Positive syntax and AST shapes | SQL Grammar Testing | COMPLETE | — | — |
| C-002 | §41.4 | Parser | Negative syntax and source spans | SQL Grammar Testing | COMPLETE | — | — |
| C-003 | §41.4 | Parser | Precedence and associativity | SQL Grammar Testing | PARTIAL | LOW — associativity not explicit | Checklist + §xref |
| C-004 | §41.4 | Parser | Quoted/unquoted identifier behavior | SQL Grammar Testing | PARTIAL | LOW — only broad identifier coverage | Checklist + §xref |
| C-005 | §41.4 | Lexer/parser | String and comment termination | None | MISSING | MEDIUM | Explicit procedure |
| C-006 | §41.4 | Parser | Multi-statement error synchronization | None | MISSING | MEDIUM | Explicit procedure |
| C-007 | §41.4 | Binder | Unknown and ambiguous names | Binder Tests | COMPLETE | — | — |
| C-008 | §41.4 | Binder | Aliases, self-joins, qualification, wildcards | Binder Tests | COMPLETE | — | — |
| C-009 | §41.4 | Binder/types | Promotion, casts, NULL, 3VL | Binder/Type Tests | COMPLETE | — | — |
| C-010 | §41.4 | Binder | Aggregate/GROUP BY/ORDER BY legality | Binder Tests | COMPLETE | — | — |
| C-011 | §41.4 | Binder | LEFT JOIN nullability, DML targets, scopes | Binder Tests | COMPLETE | — | — |
| C-012 | §41.4 | Types | Cross-layer binder/evaluator/vector/index property | Type-System Property Tests | COMPLETE | — | — |
| C-013 | §41.4, §20.14 | Subqueries | Local-only lookup and correlation rejection basis | Binder Tests | MISSING | HIGH | Checklist + §xref |
| C-014 | §41.4, §20.14 | Subqueries | Derived aliases and derived-column names | None | MISSING | HIGH | Checklist + §xref |
| C-015 | §41.4, §20.14 | Scalar subquery | 0/1/2 rows, CardinalityError, error precedence | None | MISSING | HIGH | Explicit procedure |
| C-016 | §41.4, §20.14 | EXISTS | Projection irrelevance and early stop | None | MISSING | HIGH | Explicit procedure |
| C-017 | §41.4, §20.14 | IN/NOT IN | Empty RHS, NULL, duplicates, NaN | None | MISSING | HIGH | Checklist + §xref |
| C-018 | §41.4, §20.14 | Subqueries | Lazy branches, shared snapshot and CommandId | None | MISSING | HIGH | Explicit procedure |
| C-019 | §41.4, §20.14 | Subqueries | Aggregate/grouped/DISTINCT/LIMIT/OFFSET | None | MISSING | HIGH | Checklist + §xref |
| C-020 | §41.4, §20.14 | Subquery DML | Error before/after published write | None | MISSING | HIGH | Explicit procedure |
| C-021 | §41.4, §20.14 | Subquery rewrites | Exact semantic emptiness | None | MISSING | HIGH | Checklist + §xref |
| C-022 | §41.4, §20.14 | Unsupported forms | Correlated, row, quantified, LATERAL, set, DML | None | MISSING | HIGH | Cross-reference only |
| C-023 | §41.4 | Catalog | Bootstrap/open and reopen persistence | Catalog Tests | COMPLETE | — | — |
| C-024 | §41.4 | Catalog | Normalized and quoted lookup | Catalog Tests | COMPLETE | — | — |
| C-025 | §41.4 | Catalog | Stable and non-reused object IDs | Catalog Tests | PARTIAL | LOW — non-reuse not explicit | Checklist + §xref |
| C-026 | §41.4 | Catalog | Historical schema and snapshot-aware cache | None | MISSING | MEDIUM | Explicit procedure |
| C-027 | §41.4 | Catalog | Transactional DDL visibility | Catalog Tests | COMPLETE | — | — |
| C-028 | §41.4 | Catalog lifecycle | CREATE orphan, DROP retirement, immutable invalidation | Catalog Tests | PARTIAL | MEDIUM — only broad DDL abort/drop coverage | Checklist + §xref |
| C-029 | §41.4 | Logical plan | Canonical plan shapes | Logical Planner Tests | COMPLETE | — | — |
| C-030 | §41.4 | Logical validator | Validation before and after rewrites | None | MISSING | HIGH | Explicit procedure |
| C-031 | §41.4 | Rewrites | Inputs/outputs, differential semantics, NULL, volatility | Logical Rewrite Tests | PARTIAL | MEDIUM — volatility-sensitive cases absent | Checklist + §xref |
| C-032 | §41.4 | Fuzzing | Lexer/parser/conversion/evaluator and bounded failure | Front-End Fuzzing | COMPLETE | — | — |

### §41.5 — Physical execution

| ID | Architecture | Capability | Obligation | VERIFICATION location | Status | Missing detail / priority | Later style |
|---|---|---|---|---|---|---|---|
| E-001 | §41.5 | Physical validator | Child/output slot validity and uniqueness | None | MISSING | HIGH | Checklist + §xref |
| E-002 | §41.5 | Physical validator | Physical expression type compatibility | None | MISSING | HIGH | Checklist + §xref |
| E-003 | §41.5 | Physical validator | Required hidden RID propagation | None | MISSING | HIGH | Explicit procedure |
| E-004 | §41.5 | Physical validator | Join-key type compatibility | None | MISSING | HIGH | Checklist + §xref |
| E-005 | §41.5 | Physical validator | Supported join types only | None | MISSING | HIGH | Checklist + §xref |
| E-006 | §41.5 | Physical validator | Ordering keys/properties consistent with schema | None | MISSING | HIGH | Checklist + §xref |
| E-007 | §41.5 | Physical validator | Pipeline and blocking-operator legality | Execution Testing Strategy | PARTIAL | HIGH — execution tests do not define rejection matrix | Explicit procedure |
| E-008 | §41.5 | Physical validator | Memory and spill declarations | None | MISSING | HIGH | Checklist + §xref |
| E-009 | §41.5 | Physical validator | Transaction/query-context requirements | None | MISSING | HIGH | Checklist + §xref |
| E-010 | §41.5 | Physical validator | Semantic-proof provenance | None | MISSING | HIGH | Checklist + §xref |
| E-011 | §41.5 | Physical validator | Subquery modes, OuterRefs, side-plan behavior | None | MISSING | HIGH | Checklist + §xref |
| E-012 | §41.5 | Physical validator | Derived slots/name mapping | None | MISSING | HIGH | Checklist + §xref |
| E-013 | §41.5 | Physical validator | Reject invalid plan before DML effects | None | MISSING | HIGH | Explicit procedure |
| E-014 | §41.5 | Execution strategy | Manual, end-to-end, differential tests | Execution Testing Strategy | COMPLETE | — | — |
| E-015 | §41.5 | Execution strategy | Forced spill and cancellation | Execution Testing Strategy | COMPLETE | — | — |
| E-016 | §41.5 | Parallel execution | Single-thread/parallel semantic equivalence | None | MISSING | MEDIUM | Explicit procedure |
| E-017 | §41.5 | Vectors | FLAT, CONSTANT, DICTIONARY | Vector Correctness Tests | COMPLETE | — | — |
| E-018 | §41.5 | Vectors | Validity, selection, nesting, empty/full chunks | Vector Correctness Tests | COMPLETE | — | — |
| E-019 | §41.5, §39.3 | Expressions | Checked arithmetic, division, MIN_INT/-1 | None | MISSING | HIGH | Checklist + §xref |
| E-020 | §41.5 | Expressions | BOOLEAN/3VL, casts, FLOAT64, hash/comparison | Vector/Type Tests | PARTIAL | HIGH — execution-specific cross-representation matrix absent | Checklist + §xref |
| E-021 | §41.5 | Strings | Unpin/recycle/blocking ownership and poisoning | String Lifetime Tests | COMPLETE | — | — |
| E-022 | §41.5 | Pipelines | Dependency finalization before consumers | Execution Testing Strategy | PARTIAL | HIGH — blocking-state publication not explicit | Explicit procedure |
| E-023 | §41.5 | Cancellation | Task/memory/spill/operator-state cleanup | Execution Testing Strategy | PARTIAL | HIGH — required resources not individually asserted | Explicit procedure |
| E-024 | §41.5 | Parallel state | Local/global state and snapshot semantics | None | MISSING | HIGH | Explicit procedure |
| E-025 | §41.5 | Operators | Scan/filter/project/LIMIT semantic invariants | Execution Testing Strategy | PARTIAL | MEDIUM — only broad operator comparisons | Checklist + §xref |
| E-026 | §41.5 | Hash join | Shapes, NULL/composite/collision/residual/LEFT/spill/skew | Hash Join Tests | COMPLETE | — | — |
| E-027 | §41.5 | Aggregation | Basic shapes, spill, partial combine | Aggregate Tests | PARTIAL | HIGH — aggregate functions and edge semantics abbreviated | Checklist + §xref |
| E-028 | §41.5, §29.3 | Aggregation | Exact numeric/worker/merge/spill boundary semantics | None | MISSING | HIGH | Cross-reference only |
| E-029 | §41.5 | Sort | Direction, NULLs, keys, edges, external merge | Sort Tests | COMPLETE | — | — |
| E-030 | §41.5 | DML | Halloween, retry/conflict, uniqueness, RETURNING | DML Execution Tests | PARTIAL | HIGH — transaction-boundary assertions incomplete | Explicit procedure |
| E-031 | §41.5 | Failure cleanup | No transaction/storage corruption after execution failure | Execution Strategy/DML | PARTIAL | HIGH — broad expectation lacks resource/state matrix | Explicit procedure |
| E-032 | §41.5 | Control operators | DDL, VACUUM, ANALYZE execution paths | None | MISSING | MEDIUM | Checklist + §xref |
| E-033 | §41.5, §40 | EXPLAIN ANALYZE | Actual counters are execution results | None | MISSING | MEDIUM | Explicit procedure |
| E-034 | §41.5, §40 | Profiling | Pipeline/task/operator profiling | None | MISSING | MEDIUM | Checklist + §xref |
| E-035 | §41.5, §40 | Diagnostics | Physical/optimizer information and proof provenance | None | MISSING | MEDIUM | Checklist + §xref |

### §41.6 — Statistics, estimator, and base access

| ID | Architecture | Capability | Obligation | VERIFICATION location | Status | Missing detail / priority | Later style |
|---|---|---|---|---|---|---|---|
| S-001 | §41.6 | Statistics | Exact small-domain NDV/frequency behavior | Statistics Tests | PARTIAL | MEDIUM — broad distributions only | Explicit procedure |
| S-002 | §41.6 | HLL | Error-distribution testing | None | MISSING | MEDIUM | Explicit procedure |
| S-003 | §41.6 | SpaceSaving | Heavy-hitter accuracy | Statistics Tests | PARTIAL | MEDIUM — MCV validation not algorithm-specific | Explicit procedure |
| S-004 | §41.6 | Histograms | Reservoir/histogram monotonicity | Statistics Tests | PARTIAL | MEDIUM — boundary construction omitted | Explicit procedure |
| S-005 | §41.6 | Statistics | NULL exclusion and residual MCV population | None | MISSING | MEDIUM | Checklist + §xref |
| S-006 | §41.6 | Ordering | VARCHAR/FLOAT64 statistics ordering | Statistics Tests | PARTIAL | MEDIUM — edge ordering not explicit | Checklist + §xref |
| S-007 | §41.6, §34 | ANALYZE | Stable visibility snapshot and immutable publication | None | MISSING | HIGH | Explicit procedure |
| S-008 | §41.6, §34 | Stats versions | Identity, one transaction, complete manifest | None | MISSING | HIGH | Checklist + §xref |
| S-009 | §41.6, §34 | Publication | Publish only after COMMITTED; abort invisible | None | MISSING | HIGH | Explicit procedure |
| S-010 | §41.6, §34 | Persistence | Reject corrupt/incomplete chunks and payloads | None | MISSING | HIGH | Explicit procedure |
| S-011 | §41.6, §34 | Cache/load | Generation normalization and old/missing fallback | None | MISSING | HIGH | Checklist + §xref |
| S-012 | §41.6, §34 | Version selection | Historical selection and one stable optimizer snapshot | None | MISSING | HIGH | Explicit procedure |
| S-013 | §41.6, §34 | Statistics | Physical index pressure versus live population | None | MISSING | HIGH | Explicit procedure |
| S-014 | §41.6 | Fixtures | Epsilon/dyadic/boundary-sensitive fixtures | None | MISSING | MEDIUM | Checklist + §xref |
| S-015 | §41.6 | Validation | MCV/hist ordering, mass, uniqueness, degeneracy | None | MISSING | MEDIUM | Checklist + §xref |
| S-016 | §41.6 | Statistics | Controlled skew/equal/distinct/bimodal/monotonic data | Statistics Tests | COMPLETE | — | — |
| S-017 | §41.6 | Selectivity | Equality, ranges, MCV and histogram boundaries | Selectivity Tests | COMPLETE | — | — |
| S-018 | §41.6 | Selectivity | IS NULL/IS NOT NULL/IN with NULL | Selectivity Tests | PARTIAL | MEDIUM — complete NULL matrix absent | Checklist + §xref |
| S-019 | §41.6 | Selectivity | Exact AND/OR/NOT 3VL formulas | Selectivity Tests | PARTIAL | MEDIUM — generic combinations only | Checklist + §xref |
| S-020 | §41.6 | Selectivity | Same-column range interaction/contradiction | Selectivity Tests | PARTIAL | MEDIUM — intersection cases abbreviated | Explicit procedure |
| S-021 | §41.6 | Estimation | Join, DISTINCT/GROUP, LEFT lower bound | Join Estimation Tests | PARTIAL | MEDIUM — LEFT lower-bound assertion absent | Checklist + §xref |
| S-022 | §41.6 | Estimation quality | Q-error kept separate from exact-zero semantics | Selectivity Tests | PARTIAL | HIGH — q-error exists; semantic distinction does not | Checklist + §xref |
| S-023 | §41.6, §35 | Semantic emptiness | Numerical zero never establishes proof | None | MISSING | HIGH | Explicit procedure |
| S-024 | §41.6, §35 | Semantic emptiness | Stale statistics and `TRUE` predicate | None | MISSING | HIGH | Explicit procedure |
| S-025 | §41.6, §35 | Semantic emptiness | Constant FALSE/UNKNOWN by SQL context | None | MISSING | HIGH | Checklist + §xref |
| S-026 | §41.6, §35 | Semantic emptiness | LEFT JOIN empty-side preservation | None | MISSING | HIGH | Explicit procedure |
| S-027 | §41.6, §35 | Semantic emptiness | LogicalValues and constraint-derived proof | None | MISSING | HIGH | Checklist + §xref |
| S-028 | §41.6, §35 | Semantic emptiness | NOT NULL versus estimated null fraction | None | MISSING | HIGH | Explicit procedure |
| S-029 | §41.6, §35 | Semantic emptiness | Analyzed empty table followed by visible insert | None | MISSING | HIGH | Explicit procedure |
| S-030 | §41.6, §35 | Semantic emptiness | Stale min/max/disjoint-domain overlap | None | MISSING | HIGH | Explicit procedure |
| S-031 | §41.6, §35 | Semantic emptiness | Grouped versus global aggregate on empty input | None | MISSING | HIGH | Explicit procedure |
| S-032 | §41.6, §35 | Semantic emptiness | LIMIT 0/objective 0 and zero index estimates | None | MISSING | HIGH | Checklist + §xref |
| S-033 | §41.6, §36 | Base access | Selectivity, size, payload width | Access Path Tests | PARTIAL | MEDIUM — width/cost inputs not individually checked | Checklist + §xref |
| S-034 | §41.6, §36 | Base access | Correlation-driven scan choice | Access Path Tests | COMPLETE | — | — |
| S-035 | §41.6, §36 | Base access | Dead/invisible pressure and cache configuration | None | MISSING | MEDIUM | Explicit procedure |
| S-036 | §41.6, §36 | Base access | No fixed selectivity threshold | None | MISSING | MEDIUM | Explicit procedure |
| S-037 | §41.6, §36 | Base access | Composite-index bound semantics | None | MISSING | MEDIUM | Checklist + §xref |

### §41.7 — Join search, properties, memo, optimizer

| ID | Architecture | Capability | Obligation | VERIFICATION location | Status | Missing detail / priority | Later style |
|---|---|---|---|---|---|---|---|
| O-001 | §41.7 | Join search | Bushy plans considered | Join-Order Tests | COMPLETE | — | — |
| O-002 | §41.7 | Join search | Bounded-DP transition behavior | Join-Order Tests | COMPLETE | — | — |
| O-003 | §41.7 | Join legality | Cartesian products only when semantically legal | None | MISSING | HIGH | Checklist + §xref |
| O-004 | §41.7 | Join legality | LEFT JOIN reordering constraints | None | MISSING | HIGH | Checklist + §xref |
| O-005 | §41.7 | Hash join choice | Both build/probe orientations considered | None | MISSING | MEDIUM | Explicit procedure |
| O-006 | §41.7 | INLJ choice | Selective/small-outer cases | None | MISSING | MEDIUM | Explicit procedure |
| O-007 | §41.7 | Properties | Interesting orders retained | Interesting-Order Tests | COMPLETE | — | — |
| O-008 | §41.7 | Access/order | Index ordering avoids sort | Interesting-Order Tests | COMPLETE | — | — |
| O-009 | §41.7 | Enforcement | Sort enforcement | Interesting-Order Tests | PARTIAL | MEDIUM — enforcement cost/property assertions incomplete | Checklist + §xref |
| O-010 | §41.7 | Enforcement | Top-N alternatives | Access/Order Tests | PARTIAL | MEDIUM — alternatives not explicit | Checklist + §xref |
| O-011 | §41.7 | Aggregation choice | Hash versus ordered aggregate | Interesting-Order Tests | PARTIAL | MEDIUM — ordered aggregate comparison abbreviated | Explicit procedure |
| O-012 | §41.7 | DISTINCT choice | Hash versus ordered DISTINCT | None | MISSING | MEDIUM | Explicit procedure |
| O-013 | §41.7 | Memory planning | Budget changes plan choice | Memory/Spill Plan Tests | COMPLETE | — | — |
| O-014 | §41.7 | Objectives | Full-result versus first-K/startup objectives | None | MISSING | MEDIUM | Explicit procedure |
| O-015 | §41.7 | Statistics confidence | Missing/stale stats affect confidence and fallback | None | MISSING | MEDIUM | Checklist + §xref |
| O-016 | §41.7 | Determinism | Stable ties and plan fingerprint | Plan Regression Suite | PARTIAL | MEDIUM — deterministic tie matrix not explicit | Explicit procedure |
| O-017 | §41.7 | Planning limits | Bounded heuristic fallback | Optimizer Performance | PARTIAL | MEDIUM — bounded behavior not asserted | Explicit procedure |
| O-018 | §41.7 | Semantic emptiness | Estimated-zero memo alternatives retained | None | MISSING | HIGH | Explicit procedure |
| O-019 | §41.7 | Final validation | Reject stats-derived emptiness proof | None | MISSING | HIGH | Explicit procedure |
| O-020 | §41.7 | Join estimation | Uniform/skew/disjoint/overlap/NULL-heavy cases | Join Estimation Tests | COMPLETE | — | — |
| O-021 | §41.7 | Estimate quality | Q-error against known/reference cardinality | Join Estimation Tests | PARTIAL | MEDIUM — regression thresholds not fully defined | Checklist + §xref |
| O-022 | §41.7 | Semantic safety | Optimized/unoptimized differential execution | Differential Correctness Tests | COMPLETE | — | — |
| O-023 | §41.7 | Fuzzing | Plan/metadata/property fuzzing and bounded failure | Optimizer Fuzzing | COMPLETE | — | — |
| O-024 | §41.7 | Regression | Stable plan-shape regressions | Plan Regression Suite | COMPLETE | — | — |
| O-025 | §41.7 | Cost model | Finite and nonnegative costs | Cost Model Benchmarks | COMPLETE | — | — |
| O-026 | §41.7 | Determinism | No pointer/hash-iteration nondeterminism | None | MISSING | MEDIUM | Explicit procedure |
| O-027 | §41.7 | Cost model | Pruned widths, memory assignments, spill sensitivity | Memory/Spill Tests | PARTIAL | MEDIUM — components not isolated | Checklist + §xref |
| O-028 | §41.7 | Cost model | Sort/Top-N/aggregate/DISTINCT enforcement costs | None | MISSING | MEDIUM | Checklist + §xref |
| O-029 | §41.7 | Statistics consistency | One stable statistics snapshot per optimization | None | MISSING | MEDIUM | Explicit procedure |
| O-030 | §41.7 | Final validation | Invalid final plan never executes | Differential/Plan Tests | PARTIAL | HIGH — no malformed-final-plan procedure | Explicit procedure |
| O-031 | §41.7 | Memo safety | Dominance/pruning never derives semantic emptiness | None | MISSING | HIGH | Explicit procedure |
| O-032 | §41.7 | Properties | Final physical properties are correct | Interesting-Order Tests | PARTIAL | HIGH — property-validator assertions incomplete | Explicit procedure |
| O-033 | §41.7, §40 | Observability | EXPLAIN/trace proof, cost, and actual diagnostics | None | MISSING | MEDIUM | Checklist + §xref |

## 8–10. Gap priorities

### HIGH

- Transaction/recovery: T-009–T-051, T-059, T-062–T-071.
- Front end: C-013–C-022 and C-030.
- Execution: E-001–E-013, E-019–E-024, E-027–E-028, E-030–E-031.
- Statistics: S-007–S-013 and S-022–S-032.
- Optimizer: O-003–O-004, O-018–O-019, O-030–O-032.

These gaps could permit incorrect durability, transaction state, SQL semantics, data-changing execution, or semantic-elimination behavior to appear verified.

### MEDIUM

- T-007 and T-060.
- C-005–C-006, C-026, C-028, C-031.
- E-016, E-025, E-032–E-035.
- S-001–S-006, S-014–S-015, S-018–S-021, S-033, S-035–S-037.
- O-005–O-006, O-009–O-017, O-021, O-026–O-029, O-033.

### LOW

- C-003, C-004, C-025.

## 11–22. Transaction/recovery assessments

11. **Database lifecycle:** MISSING apart from generic reopen/recovery concepts. None of the §3.3 ownership, admission, cleanup, control-slot, orphan, draining, or shutdown-failure matrices is operationalized.

12. **Non-crash §12.12 injection:** MISSING. The crash framework cannot substitute for known failure, uncertain outcome, rollback, or post-append publication semantics.

13. **PAGE_INIT/MTR rollback state:** MISSING. No procedure checks exact bytes, frame metadata, DPT/FPI state, tail truncation, PageNo reuse, or complete B+ structural restoration.

14. **Statement-error matrix:** MISSING, with partial incidental coverage for retry and RETURNING. It lacks both sides of the first-published-write boundary and the required client/transaction outcomes.

15. **COMMIT C0–C6:** MISSING. The existing crash point around TXN_COMMIT flush covers only crash durability, not the complete fault/outcome matrix.

16. **ABORT A0–A4:** MISSING.

17. **Recovery property testing:** COMPLETE. It specifies randomized workloads, instrumented crashes, reopen/recovery, durable-commit model comparison, and allowance for physical aborted garbage.

18. **MVCC:** COMPLETE at the §41.3 obligation level.

19. **Isolation:** COMPLETE for READ COMMITTED, REPEATABLE READ/snapshot isolation, retry/re-evaluation, serialization failure, and write skew.

20. **Locking/deadlock/gates:** PARTIAL. Basic tuple/key serialization, deadlock victim policy, latch discipline, release, and waiter cleanup are covered. Unified gate edges and §11.13.7 adversarial timelines are not.

21. **UNIQUE/PRIMARY KEY:** Mostly MISSING. Generic unique-key serialization is present, but the architecture’s extensive creator/deleter/CommandId/update/recheck truth table is absent.

22. **Vacuum/reclamation:** PARTIAL. Crash points and broad benchmarks exist; exact restart, retirement, grace, RID-reuse, version-chain, and long-snapshot procedures do not.

## 23–29. Catalog/front-end assessments

23. **Parser:** PARTIAL. Positive/negative/AST/span/precedence coverage is strong; termination, synchronization, and explicit associativity/identifier cases need expansion.

24. **Binder/type system:** Largely COMPLETE. The cross-layer type property is explicitly present.

25. **Subquery support matrix:** MISSING except for a generic “subquery scopes” binder item. The §20.14 matrix is not operationalized.

26. **Catalog:** PARTIAL. Bootstrap, reopen, lookup, stable IDs, and transactional DDL appear; historical descriptors, cache semantics, ID non-reuse, orphan/drop/invalidation details are abbreviated.

27. **Logical-plan validator:** MISSING.

28. **Rewrite testing:** PARTIAL but strong. Rule inputs/outputs, NULL-rich cases, and differential execution exist; volatility-sensitive and validator-before/after obligations are incomplete.

29. **Front-end fuzzing:** COMPLETE for the required targets and bounded failure behavior.

## 30–38. Physical-execution assessments

30. **Physical-plan validator:** MISSING. No malformed-plan rejection matrix or pre-DML validation assertion exists.

31. **Vector/expression:** Vector representations are COMPLETE; arithmetic, overflow, FLOAT64, 3VL, casting, hashing, and comparison consistency are PARTIAL/MISSING.

32. **String lifetime:** COMPLETE, including unpin/recycle/blocking ownership and poisoned-memory techniques.

33. **Pipelines/cancellation/resources:** PARTIAL. Forced cancellation/spill exists, but dependency finalization and exhaustive task/memory/spill/operator cleanup assertions do not.

34. **Hash join:** COMPLETE, including multiplicity, NULL/composite/collision/residual/LEFT JOIN, multi-chunk expansion, spill, skew, and reference comparison.

35. **Aggregation:** PARTIAL. Basic shapes, grouping, spill, and partial combine are covered; exact §29.3 numeric, worker, merge, and boundary-vector semantics are not.

36. **Sort:** COMPLETE.

37. **DML/Halloween/RETURNING:** PARTIAL. Halloween protection and buffering are present; transaction-error boundaries, exposure timing, update revalidation, and isolation-specific outcomes need explicit procedures.

38. **EXPLAIN ANALYZE/profiling:** MISSING.

## 39–47. Statistics/optimizer assessments

39. **Statistics:** PARTIAL. Controlled distributions are strong, but algorithm-specific accuracy, validation, ordering, corruption, and persistence behavior are incomplete.

40. **Statistics version/publication:** MISSING.

41. **Estimated zero versus semantic emptiness:** MISSING. No test protects against converting numerical estimates into semantic proofs.

42. **Selectivity:** PARTIAL. Basic operators and MCV/histogram boundaries exist; full NULL/3VL/same-column interaction matrices do not.

43. **Join estimation:** Broadly COMPLETE for core distributions, with PARTIAL q-error/regression expectations and LEFT-join lower-bound coverage.

44. **Access paths:** PARTIAL. SeqScan/IndexScan, selectivity, ordering, and correlation exist; width, pressure, cache, threshold, and composite-bound requirements are incomplete.

45. **Join order/properties:** PARTIAL. Bushy plans and interesting orders are covered; join-legality restrictions, orientations, INLJ, enforcement alternatives, and final property validation need work.

46. **Cost/memory/fallback:** PARTIAL. Finite costs, memory-sensitive choices, and benchmark structure exist; objectives, deterministic ties, enforcement costs, stable stats snapshots, and bounded fallback are incomplete.

47. **Final optimizer validation:** PARTIAL/MISSING. Differential semantic testing is strong, but malformed final plans, semantic-emptiness provenance, memo pruning, and final physical-property validation are not operationalized.

48. **§41.7 overall:** 10 COMPLETE, 9 PARTIAL, 14 MISSING. It is materially better covered than §41.6, but correctness-sensitive legality, semantic-emptiness, memo, and final-validation gaps remain.

## 49. Re-evaluation of the six previously reported gaps

All six prior audit findings are confirmed:

| Prior finding | Result |
|---|---|
| Database-lifecycle fault testing | Confirmed — MISSING |
| Statement/transaction error matrices | Confirmed — MISSING |
| COMMIT/ABORT boundary injection | Confirmed — MISSING |
| Detailed subquery verification | Confirmed — MISSING |
| Physical-plan validation | Confirmed — MISSING |
| Semantic-empty versus estimated-zero testing | Confirmed — MISSING |

## 50. Additional gaps discovered

Beyond those six:

- deterministic non-crash §12.12 failure semantics;
- exact PAGE_INIT/MTR rollback-state assertions;
- unified logical gate/deadlock graph testing;
- detailed UNIQUE/PRIMARY KEY truth-table testing;
- version-chain and long-snapshot vacuum testing;
- logical-plan validator testing;
- parser termination and multi-statement synchronization;
- checked execution arithmetic and aggregate boundary semantics;
- pipeline finalization and exhaustive resource cleanup;
- EXPLAIN ANALYZE/profiling correctness;
- statistics generation/publication/cache/corruption behavior;
- base-access pressure and composite-bound testing;
- join-legality constraints and deterministic optimizer behavior.

## 51. Additional valid procedures

VERIFICATION contains useful procedures beyond this audit slice:

- storage and B+ tree verification corresponding to §41.1–§41.2;
- deterministic barrier and randomized-seed guidance;
- benchmark anti-gaming rules;
- parser/AST memory benchmarking;
- extensive subsystem microbenchmark and end-to-end benchmark guidance.

Nothing should be removed merely because it is outside §41.3–§41.7.

## 52–53. Architecture and implementation follow-ups

52. **Architecture questions:** None. The referenced architecture obligations were detailed enough to map without resolving an internal contradiction.

53. **Possible implementation follow-ups:** None established by this documentation-only audit. Missing verification prose does not prove missing tests or source defects, and source/tests were intentionally not audited.

## 54–56. Recommended documentation update strategy

54. Every PARTIAL/MISSING row has a recommended style in the matrix. The governing pattern should be:

- Use **cross-reference only** for exhaustive architecture-owned truth tables such as C0–C6, A0–A4, §11.10, §11.13.7, and §20.14.
- Use **compact checklist + architecture cross-reference** when the guide must identify cases but should not duplicate semantics.
- Use an **explicit procedure** for injection setup, state observation, cleanup, and expected results.

55. Recommended update order:

1. §41.3 lifecycle, §12.12, statement, COMMIT/ABORT, and UNIQUE/gate coverage.
2. §41.5 physical-plan validation, execution failure cleanup, aggregate semantics, and RETURNING.
3. §41.6–§41.7 statistics publication, semantic emptiness, memo, and final validation.
4. §41.4 subqueries, logical validator, parser edge cases, and catalog lifecycle.

56. Best execution model: **B — separate §41.3 / §41.4 / §41.5 / §41.6–§41.7 tasks.**

Reason: §41.3 alone contains 71 mapped obligations and several architecture-owned matrices. Splitting the work keeps each edit reviewable, prevents accidental architecture duplication, and allows the high-risk durability and semantic-elimination gaps to receive focused treatment.

## 57–60. Read-only integrity

57. Files modified: **NONE**.

58. `git diff --check`: **passed with no output**.

59. Git working tree and index were unchanged:

- Initial status: clean
- Final status: clean
- Initial staged diff: empty
- Final staged diff: empty
- HEAD before/after: `9f19a6e5dd7a0657fc80daa9c541d3d76a8e7583`

60. No source, tests, benchmarks, build configuration, documentation, or devlog files were modified.