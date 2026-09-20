# Chapter 39 initial Architecture review

## 1–8. Verdict, repository, and live chapter inventory

1. **Initial review verdict:** **NEEDS ARCHITECTURE FIX**.

2. **Initial HEAD:** `7d2c878a5809fe40879cc9982e9923079db67d54`
   Commit: `7d2c878 synced VERIFICATION after chapter 38 ARCHITECTURE fix`

3. **Initial worktree/index:** clean; no staged files.

4. **Final HEAD/status:** unchanged; worktree and index remain clean.

5. **Audit-created changes:** none.

6. **Live chapter:** [Chapter 39 — Error and Corruption Model](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:28946), lines 28946–29581. Chapter 40 begins at line 29582.

7. **Subsection inventory:**

   - §39.1 Transaction, durability, and recovery errors
     - §39.1.1 Runtime states and statement outcomes
     - §39.1.2 First persistent statement write and no-undo
     - §39.1.3 Normative statement-error matrix
     - §39.1.4 Completion, retry, and subsystem consequences
     - §39.1.5 COMMIT failure, durability, and acknowledgement
     - §39.1.6 ABORT failure and mandatory cleanup
     - §39.1.7 Connection loss, recovery, and precedence
     - §39.1.8 Locks, caches, and observability
   - §39.2 SQL front-end and logical-planning errors
   - §39.3 Execution errors
     - §39.3.1 Checked integer arithmetic
     - §39.3.2 FLOAT64 arithmetic/division
   - §39.4 Optimizer errors

   Chapter 39 has no final invariant subsection.

8. **Error/category inventory:**

   - Transaction/storage: `SerializationFailure`, `DeadlockVictim`, `UniqueViolation`, `TransactionAborted`, `TransactionMustAbort`, `CommitOutcomeUncertain`, `ConnectionFailure`, `LockCancelled`, `WalIOError`, `StorageNoncontinuable`, `RecoveryError`, `CorruptionError`.
   - Front end: `LexerError`, `ParserError`, `BindError`, `TypeError`, `CatalogError`, `ConstraintDefinitionError`, `UnsupportedFeature`, `CardinalityError`, `FrontEndResourceLimit`.
   - Execution: `ExecutionError`, `OutOfMemory`, `SpillIOError`, `QueryCancelled`, `CardinalityViolation`, `CastError`, `ArithmeticError`, `ConstraintViolation`, `TransactionConflict`.
   - Optimizer: `OptimizerError`, `OptimizerResourceLimit`.
   - Command outcomes: `SUCCESS`, `FAILED_TRANSACTION_REMAINS_ACTIVE`, `FAILED_TRANSACTION_MUST_ABORT`, `COMMIT_OUTCOME_UNCERTAIN`, `DATABASE_NONCONTINUABLE`.

## 9. Canonical owner matrix

| Concern | Canonical owner | Chapter-39 role |
|---|---|---|
| Database admission/noncontinuability | §§3.3.5–3.3.7 | Consumes and reports the gate |
| Transaction runtime states | §§9.4, 9.14 | Uses the frozen state machine |
| First logical write | §§12.12, 15.7, 39.1.2 | Defines statement consequence |
| WAL append/publication uncertainty | §12.12 | Preserves exact lower-layer cause |
| Recovery winner/loser outcome | Chapter 13 | Maps persisted evidence |
| COMMIT/ABORT stages | §§15.5–15.6 | Defines failure consequences |
| DML candidate closure and W/C/R | §§15.1.3, 21.16.1, 31.9 | Preserves pre-write closure |
| Front-end resources | §18.17 | Preserves guard versus allocation cause |
| Subquery errors | §20.14.12 | Preserves demand/precedence |
| Query memory/spill | Chapter 24 | Preserves OOM/representability/spill causes |
| Checked arithmetic | Chapters 17 and 29 | Runtime enforcement and mapping |
| Planning resources | §§38.21, 39.4 | Preserves configured bound versus backing denial |

The ownership graph is otherwise coherent and avoids duplicate durable transaction states.

## 10–19. Transaction and statement behavior

10. **Transaction states:** `ACTIVE`, `MUST_ABORT`, `COMMITTING`, `COMMITTED`, `ABORTING`, and `ABORTED` agree with Chapter 9. Only `ACTIVE` admits ordinary work or COMMIT. Locks and snapshots remain owned through terminal publication.

11. **Command outcomes:** the semantic/client distinction is sound, but the declared outcome algebra is incomplete. This is N39-1 below.

12. **First write boundary:** `current_statement_has_published_write` becomes true exactly at publication of a transaction-owned logical mutation backed by its publication-authorizing WAL record.

13. **Flag distinction:** `has_persistent_writes` is transaction-wide and can include structural WAL state; it does not make a later effect-free statement fatal. The current-statement flag never resets during that attempt.

14. **Statement-error matrix:**

| Failure class | Before W | After W | Assessment |
|---|---|---|---|
| Parse/lex | FA | MA if somehow delayed | COMPLETE |
| Bind/name/type/catalog/unsupported | FA | MA | COMPLETE |
| Planner resource | FA | MA | COMPLETE |
| Ordinary DML expression/assignment | FA | unreachable by closure | COMPLETE |
| Non-DML expression/arithmetic/cast | FA | MA where reachable | COMPLETE |
| Aggregate final overflow | FA | MA | COMPLETE |
| Ordinary DML constraints | FA | unreachable by closure | COMPLETE |
| RC conflict/stale target | retry/FA | MA | COMPLETE |
| RR serialization/deadlock | MA | MA | COMPLETE |
| Cancellation | FA | MA | COMPLETE |
| OOM | FA | MA | COMPLETE |
| Spill failure | FA | MA | COMPLETE |
| Persistent-page corruption | NC | NC | COMPLETE |
| Known flush/durability failure | FA | MA | COMPLETE |
| Restored known WAL append failure | FA | MA | COMPLETE |
| Restored provisional MTR failure | FA | MA | COMPLETE |
| Uncertain append/restoration | NC | NC | COMPLETE |
| Coherent raw I/O failure | FA | MA | COMPLETE |
| DDL namespace/publication failure | FA/MA, NC if incoherent | COMPLETE |
| ANALYZE failure | FA before first stats row; MA after | COMPLETE |
| Explicit ROLLBACK | AB | AB | COMPLETE |
| Internal invariant failure | NC | NC | COMPLETE |

15. **Overrides:** deadlock and RR serialization are transaction-fatal independently of W. Corruption, uncertain storage ownership, and internal invariants are database-noncontinuable. Ordinary user/resource errors follow W.

16. **Multirow DML:** the live Architecture does **not** permit an ordinary demanded row-five expression or constraint to arise after rows 1–4 publish. Chapter-31 candidate closure finds it before W, giving FA and no mutations. A dynamic row-five cancellation/OOM/I/O failure after rows 1–4 publish gives MA and aborted physical garbage. This corrects the stale premise in the request without reopening Chapter 31.

17. **Retry/CommandId/snapshot:** pre-write RC retries retain the logical CommandId but use a fresh attempt snapshot and fresh attempt-local state. Failed admitted statements consume their CommandId. Post-write retry is forbidden.

18. **RETURNING:** no failed statement publishes a prefix. Explicit `R` may precede later transaction COMMIT and is not a durability acknowledgement. Autocommit prepares all required result resources before C0 and publishes `R` only after C4–C5.

19. **ANALYZE/DDL:** temporary collection, private file construction, identifier gaps, and conservative orphan-capable artifacts do not cross W. First transaction-owned statistics/catalog publication does. Postcommit cache failure cannot revoke COMMIT.

## 20–31. COMMIT, ABORT, recovery, locks, and caches

20. **Persistent COMMIT C0–C6:**

| Stage/failure | Required outcome | Assessment |
|---|---|---|
| C0/C1 failure | no commit record; mandatory ABORT | COMPLETE |
| C2 known no-append | exact restoration; mandatory ABORT | COMPLETE |
| C2 append uncertain | NC; recovery decides | COMPLETE |
| Commit appended, status publication fails | NC; no redirect to ABORT | COMPLETE |
| C3 retryable durability failure | remain COMMITTING and retry | COMPLETE |
| C3 cannot safely continue | NC; uncertain client outcome | COMPLETE |
| Durable C3 then C4 failure | COMMITTED + NC | COMPLETE |
| C5 safe cache fallback | COMMITTED; may acknowledge | COMPLETE |
| C5 incoherent cleanup | COMMITTED + NC | COMPLETE |
| C6 acknowledgement | successful COMMIT | COMPLETE |
| C6 transport failure | COMMITTED; client uncertain | COMPLETE semantically, command-outcome gap N39-1 |

The table does not decide the C2/C3-elided read-only C4 failure window; N39-2.

21. **Durable commit:** for persistent transactions, `durable_lsn >= commit_lsn` is irreversible. Append and durability remain distinct. No later cleanup, cache, transport, or shutdown failure can create ABORTED.

22. **Terminal WAL headroom/group commit:** retained closure credit prevents expected terminal `WAL_POSITION_EXHAUSTED`. Contradiction of retained credit is NC. Group-flush failure cannot selectively abort appended commit waiters.

23. **Status-page/cache ordering:** C2 installs resident status bytes before C3, but active-registry lookup continues to yield `IN_PROGRESS` until C4 terminal-cache publication. C5 releases locks only afterward.

24. **Acknowledgement:** persistent COMMIT is acknowledged only after C4 and coherent C5. C6 transport failure leaves COMMITTED with client uncertainty.

25. **ABORT A0–A4:**

| Failure | Outcome | Assessment |
|---|---|---|
| No persistent state before A1 | publish ABORTED and release after publication | COMPLETE |
| Known abort no-append | remain ABORTING and retry | COMPLETE |
| Uncertain append/status publication | NC; recovery loser handling | COMPLETE |
| Retry/publication cannot finish | NC; preserve original cause | COMPLETE |
| A2 publication failure | NC | COMPLETE |
| A3 cleanup incoherent | ABORTED + NC | COMPLETE |
| A4 transport failure | ABORTED; close connection | Semantic outcome complete; command outcome GAP N39-1 |

26. **MUST_ABORT:** original cause is retained; COMMIT is forbidden; automatic A0–A4 runs; locks remain through ABORTED publication. Abort failure chains rather than erases the original error.

27. **Connection loss:** nonterminal persistent states are well defined. The table is incomplete for read-only COMMIT and already-terminal pre-cleanup states; N39-2.

28. **Crash recovery:**

| Precrash state | Recovery result | Assessment |
|---|---|---|
| ACTIVE/MUST_ABORT | loser → ABORTED | COMPLETE |
| COMMITTING without complete surviving commit | ABORTED | COMPLETE |
| COMMITTING with complete persisted commit | COMMITTED | COMPLETE |
| After durable commit | COMMITTED | COMPLETE |
| ABORTING with/without abort record | ABORTED | COMPLETE |
| COMMITTED | COMMITTED | COMPLETE |
| ABORTED | ABORTED | COMPLETE |

29. **Precedence:** durable COMMIT dominates later failure; mandatory noncommit remains noncommit; NC additionally governs server continuation; original cause remains primary with cleanup/fatal causes chained. It is neither first-error nor last-error precedence.

30. **Locks/gates:** TUPLE_WRITE, UNIQUE_KEY, SchemaLock, both TableWriterGate modes, STATS_PUBLISH, MANIFEST_CHANGE, snapshots, and registry ownership remain through terminal publication. Deadlock cycles are transaction-fatal, not corruption.

31. **Caches:** terminal outcome cache is authoritative over stale registry state. TXN_STATUS pages are not the runtime terminal cache. Catalog/statistics caches may install, invalidate, bypass, or retain valid old/missing statistics. Incoherence after COMMIT causes NC without changing COMMITTED.

## 32–44. Front end, execution, arithmetic, optimizer, and diagnostics

32. **Front-end/SourceSpan:** all nine front-end categories have appropriate source-stage ownership. Source-originating diagnostics retain canonical spans. Delayed discovery does not turn user input into an invariant failure.

33. **FrontEndResourceLimit versus OOM:** configured/safety refusal remains `FrontEndResourceLimit`; actual required-allocation denial remains `OutOfMemory`.

34. **Logical-plan validation:** malformed internally constructed logical plans are internal defects, not user SQL errors, and cannot execute.

35. **Execution categories:** lower-layer causes remain structured; command/transaction state is decided only by §39.1, not by individual operators.

36. **Representability:** unsupported exact runtime representation is controlled representability/resource `ExecutionError`; supported exact allocation denial or hard-gate exhaustion without progress is `OutOfMemory`; configured planning exhaustion is `OptimizerResourceLimit`.

37. **Spill/corruption:** temporary creation, ENOSPC, I/O, offset/addressability, framing, CRC, and range failures are `SpillIOError`. Persistent database corruption remains corruption/NC. Self-generated spill invariants remain internal defects.

38. **Query cleanup:** chunks, reservations, row collections, spill files, tasks, operator and subquery state unwind. Query cleanup cannot release transaction locks or erase W. No failed pipeline remains runnable.

39. **Subqueries:** scalar cardinality, demanded child errors, IN build completion, and EXISTS early stop agree with §20.14.12 and use the universal statement matrix.

40. **Integer arithmetic:** checked `+`, `-`, `*`, unary minimum negation, `MIN/-1`, division/remainder by zero, and final SUM/COUNT conversion are complete. Exact aggregate intermediate state does not spuriously overflow.

41. **FLOAT64:** binary64 rounding, NaN, signed zero, infinity, and aggregate-tree semantics are coherent. Division by signed zero follows IEEE behavior. Unsafe compiler modes are prohibited.

42. **Optimizer errors:** malformed configuration/invalid raw cost remain `OptimizerError`; valid saturation is metadata; unavailable algorithms are ineligible; configured bounded-planning exhaustion is `OptimizerResourceLimit`; below-budget backing denial is `OutOfMemory`; invalid final plans are internal and cannot execute.

43. **Chapter-38 Fix-B regression:** all five cases retain their closed outcomes. No Chapter-38 contradiction was found.

44. **Diagnostics:** Chapter 40 exposes states, current stage, W flag, WAL/durability, lifecycle, failures, and uncertainty. Structured causes and terminal outcomes remain separable.

## 45. Statement-atomicity adversarial matrix

Abbreviations: `FA` = failed, transaction active; `MA` = mandatory abort; `NC` = database noncontinuable.

| Case | Required result | Assessment |
|---|---|---|
| A failed SELECT, W=0 | original error, FA, query cleanup | COMPLETE |
| B prior INSERT; later SELECT W=0 | FA; prior transaction work remains | COMPLETE |
| C reserved LSN only | W=0; cause-specific FA/NC | COMPLETE |
| D exact provisional restoration | W=0; FA | COMPLETE |
| E structural page allocation only | statement W=0; transaction-wide bit may be set | COMPLETE |
| F first heap version | W=1 | COMPLETE |
| G first old xmax/cmax | W=1 | COMPLETE |
| H logical index entry | W=1 | COMPLETE |
| I catalog row | W=1 | COMPLETE |
| J statistics row | W=1 | COMPLETE |
| K five-row ordinary error | closure gives FA/W=0; dynamic row-five failure gives MA/W=1 | COMPLETE after applying live owner |
| L UPDATE before W | FA | COMPLETE |
| M UPDATE after W | MA → ABORTED | COMPLETE |
| N DELETE after xmax | MA → ABORTED | COMPLETE |
| O RC conflict before W | retry or FA | COMPLETE |
| P RC conflict after W | MA | COMPLETE |
| Q RR serialization before W | MA | COMPLETE |
| R deadlock before W | MA; locks through A2 | COMPLETE |
| S cancellation before W | FA | COMPLETE |
| T cancellation after W | MA | COMPLETE |
| U OOM before W | FA | COMPLETE |
| V OOM after W | MA | COMPLETE |
| W spill error before W | FA | COMPLETE |
| X spill error after W | MA | COMPLETE |
| Y persistent corruption | NC | COMPLETE |
| Z internal invariant | NC | COMPLETE |
| AA prior structural WAL; current W=0 | current recoverable failure FA | COMPLETE |
| AB current W published but invisible | MA; cannot commit | COMPLETE |
| AC retry after W | forbidden | COMPLETE |
| AD pre-write retry reuses stale snapshot | forbidden; fresh snapshot required | COMPLETE |
| AE failed RETURNING prefix | no exposure | COMPLETE |
| AF autocommit RETURNING before C4–C5 | forbidden | COMPLETE |

## 46. COMMIT/ABORT adversarial matrix

| Case | Required result | Assessment |
|---|---|---|
| AG C0 failure | mandatory ABORT | COMPLETE |
| AH C2 preparation failure | restore then ABORT | COMPLETE |
| AI known commit no-append | ABORT, no same-TxnId retry | COMPLETE |
| AJ uncertain append | NC + commit uncertainty | COMPLETE |
| AK append then cancellation | continue COMMIT | COMPLETE |
| AL group flush failure | all remain COMMITTING/retry or NC | COMPLETE |
| AM durable C3; C4 failure | COMMITTED + NC | COMPLETE |
| AN C5 safe invalidation | COMMITTED; success possible | COMPLETE |
| AO C5 incoherent ownership | COMMITTED + NC | COMPLETE |
| AP C6 socket failure | COMMITTED, client uncertain | **GAP: N39-1 command outcome** |
| AQ client repeats after uncertainty | unsafe; application reconciliation required | COMPLETE |
| AR known abort no-append | remain ABORTING/retry | COMPLETE |
| AS uncertain abort append | NC; recovery loser | COMPLETE |
| AT abort terminal publication failure | NC | COMPLETE |
| AU cleanup after ABORTED fails | ABORTED + NC | COMPLETE |
| AV abort acknowledgement failure | ABORTED, close connection | **GAP: N39-1 command outcome** |
| AW COMMIT in MUST_ABORT | illegal; join/run abort | COMPLETE |
| AX early lock release in MUST_ABORT | forbidden | COMPLETE |
| AY deadlock victim releases early | forbidden | COMPLETE |
| AZ closure-credit contradiction | internal/NC | COMPLETE |
| BA unrelated background flush error | no retroactive failure | COMPLETE |
| BB required operation observes known WAL failure | cause + W matrix | COMPLETE |
| BC restoration impossible | NC | COMPLETE |
| BD postcommit cache failure | COMMITTED; safe fallback or NC | COMPLETE |

## 47. Recovery/ownership adversarial matrix

| Case | Required result | Assessment |
|---|---|---|
| BE disconnect ACTIVE | ABORT | COMPLETE |
| BF disconnect MUST_ABORT | continue ABORT | COMPLETE |
| BG disconnect before commit append | cancel commit and ABORT | COMPLETE for persistent commit |
| BH disconnect after append | uncancellable COMMIT or NC | COMPLETE |
| BI durable commit before ack | COMMITTED; client uncertain | COMPLETE |
| BJ disconnect ABORTING | continue abort | COMPLETE |
| BK crash COMMITTING/no commit | ABORTED loser | COMPLETE |
| BL complete persisted commit | COMMITTED | COMPLETE |
| BM crash ABORTING/no abort | ABORTED loser | COMPLETE |
| BN torn WAL suffix | no complete commit; truncate valid tail | COMPLETE |
| BO UNIQUE then abort I/O | UNIQUE primary + noncommit + NC | COMPLETE |
| BP COMMITTED then transport failure | COMMITTED + client uncertainty | COMPLETE |
| BQ terminal cache contradiction | internal/NC | COMPLETE |
| BR catalog install fails, fallback safe | COMMITTED and continue | COMPLETE |
| BS catalog ownership incoherent | COMMITTED + NC | COMPLETE |
| BT older stats descriptor | valid advisory fallback | COMPLETE |
| BU missing wait-for edge | invariant/NC | COMPLETE |
| BV ordinary deadlock called corruption | forbidden; deadlock victim MA | COMPLETE |
| BW physical OOM called NC | forbidden; FA/MA by W | COMPLETE |
| BX temporary spill CRC called corruption | forbidden; SpillIOError | COMPLETE |
| BY persistent page corruption called spill error | forbidden; corruption/NC | COMPLETE |

Additional uncovered live fixture: read-only COMMIT disconnect/failure at C4 and terminal COMMITTED/ABORTED disconnect during C5/A3 cleanup — **GAP N39-2**.

## 48. Front-end/execution adversarial matrix

| Case | Required result | Assessment |
|---|---|---|
| BZ parser error without span | reject; canonical SourceSpan required | COMPLETE |
| CA unsupported feature → invariant | forbidden | COMPLETE |
| CB configured front-end guard → OOM | forbidden | COMPLETE |
| CC physical allocation → front-end limit | forbidden | COMPLETE |
| CD unrepresentable VARCHAR truncated | forbidden; ExecutionError | COMPLETE |
| CE supported form denied | OutOfMemory | COMPLETE |
| CF runtime hard gate no progress | OutOfMemory | COMPLETE |
| CG spill ENOSPC | SpillIOError | COMPLETE |
| CH unrepresentable spill offset | SpillIOError | COMPLETE |
| CI self-generated spill invariant | internal defect | COMPLETE |
| CJ worker remains runnable | forbidden; quiesce | COMPLETE |
| CK query cleanup releases locks | forbidden | COMPLETE |
| CL scalar second row ignored | CardinalityViolation/CardinalityError | COMPLETE |
| CM demanded child error suppressed | forbidden | COMPLETE |
| CN IN build skipped by first-K | forbidden | COMPLETE |
| CO integer MIN/-1 | ArithmeticError/NUMERIC_OVERFLOW | COMPLETE |
| CP integer division by zero | ArithmeticError | COMPLETE |
| CQ integer remainder by zero | ArithmeticError | COMPLETE |
| CR exact SUM intermediate outside result domain, final fits | no premature error | COMPLETE |
| CS COUNT final conversion overflow | ArithmeticError/NUMERIC_OVERFLOW | COMPLETE |
| CT FLOAT finite/±0 | signed infinity | COMPLETE |
| CU FLOAT ±0/±0 | NaN | COMPLETE |
| CV infinity/infinity | NaN | COMPLETE |
| CW exact-zero FLOAT aggregate | canonical `+0.0` | COMPLETE |
| CX malformed epsilon | OptimizerError; normally FA/W=0 | COMPLETE |
| CY valid saturated cost | ordinary metadata | COMPLETE |
| CZ configured planning exhaustion | OptimizerResourceLimit | COMPLETE |
| DA below-budget backing denial | OutOfMemory | COMPLETE |
| DB high cost → resource error | forbidden | COMPLETE |
| DC invalid final physical plan executes | forbidden; internal/NC before execution | COMPLETE |
| DD cleanup erases resource cause | forbidden; cause chained | COMPLETE |
| DE background error retroactively fails statement | forbidden | COMPLETE |

## 49. Normative-table review

| Live table | Rows | Result |
|---|---:|---|
| Command outcomes | 5 | **GAP N39-1** |
| Statement errors | 22 | COMPLETE |
| Persistent COMMIT failures | 11 | **GAP N39-2 for read-only C4 path** |
| ABORT failures | 7 | Complete semantics; A4 command mapping affected by N39-1 |
| Connection loss | 6 | **GAP N39-2** |
| Crash recovery | 7 | COMPLETE |

No other Chapter-39 normative table was omitted.

## 50. Forbidden implementations

The live list contains **20**, not 14. All have operative owners and observable counterexamples:

1. commit partial failed DML — complete
2. retry after W — complete
3. reuse CommandId/snapshot — complete
4. treat restored provisional state as garbage — complete
5. durable COMMIT → ABORTED — complete
6. acknowledge before C4/C5 — complete
7. release locks before terminal publication — complete
8. ordinary work/COMMIT in MUST_ABORT — complete
9. effect-free SELECT error becomes fatal — complete
10. collapse all resource failures — complete
11. expose failed RETURNING prefix — complete
12. cache failure changes COMMIT — complete
13. omit gate dependencies from wait graph — complete
14. release deadlock-victim gates early — complete
15. post-R delivery failure reopens statement — complete
16. failed cursor reported FINISHED — complete
17. draining cursor required for semantic success — complete
18. enter C0 with fallible result preparation outstanding — complete
19. accounting grant treated as allocation success — complete
20. fallible work between C4–C5 and R or COMMITTED redirected to ABORT — complete

The list is coherent; it does not repair N39-1/N39-2 by itself.

## 51. Chapters 31–38 regression assessment

- Chapter 31: W/C/R, candidate closure, RETURNING, retry, and no-prefix rules preserved.
- Chapter 32: terminal query failure leaves no runnable worker; source/replay rules preserved.
- Chapter 33: retained planning inputs and final validation preserved.
- Chapter 34: complete generations, transaction-owned statistics rows, and cache fallback preserved.
- Chapter 35: estimates never become semantic proof.
- Chapter 36: valid saturation remains metadata; invalid raw values remain errors.
- Chapter 37: subquery demand and bounded search preserved.
- Chapter 38: configured planning exhaustion, backing-allocation denial, mitigation, and mixed-cause ownership preserved.

No frozen semantic contradiction was found.

## 52. Global contradiction search

Results:

- `MUST_ABORT`, FA/MA, W flags: consistent across Chapters 9, 15, 31, 39, and 41.
- `TXN_COMMIT`, `commit_lsn`, `durable_lsn`: valid append/durability distinction.
- `TXN_ABORT`: valid loser-resolution distinction.
- Terminal publication/lock release: consistent.
- OOM/front-end/planning/spill categories: valid resource distinctions.
- Cardinality and arithmetic mappings: valid error mappings.
- Final validation: consistently pre-execution.
- Historical references: none treated as normative.
- True defects found: N39-1 and N39-2 only.

## 53. Existing Verification reuse inventory

Applicable live procedures/suites include:

- Numeric Exhaustion and Terminal-Boundary Verification
- Transaction identity, snapshot, and status verification
- Durable prefix, group commit, COMMIT, and WAL-before-data
- Non-Crash WAL/MTR Failure Injection
- Statement Failure and Transaction-State Tests
- COMMIT Fault-Injection Tests
- ABORT Fault-Injection Tests
- Recovery Property Tests
- Locking and Gate Tests
- Front-End Error and Source-Span Tests
- V20-12–V20-17 and V20-24
- V21-2, V21-3, V21-13, V21-14, V21-23, V21-26
- V22-K
- V24-D, V24-H, V24-J–V24-M
- V25-H–V25-K, V25-M–V25-O
- V26-H–V26-L
- V29-F, V29-I–V29-M, V29-Q
- V31-A–V31-N
- V32-H–V32-M
- V33-E–V33-F
- V34-D, V34-H–V34-J
- V36-C, V36-I
- V37-I
- V38-052–V38-067

These are applicable component oracles, not yet a synchronized V39 integration section.

## 54. Future V39 obligations

Future V39 synchronization must add:

- every live normative table row;
- all 20 forbidden behaviors;
- ordinary DML closure versus dynamic post-write failure;
- command-outcome coverage after N39-1 repair;
- read-only COMMIT and terminal disconnect coverage after N39-2 repair;
- C0–C6 and A0–A4 fault injection;
- append-known/uncertain/durable distinctions;
- automatic-abort failure and chained causes;
- client uncertainty versus actual outcome;
- crash winner/loser evidence;
- terminal-cache/lock ordering;
- front-end/OOM/representability/spill distinctions;
- subquery and arithmetic precedence;
- final-plan rejection and no-execution;
- nonvacuous cleanup/quiescence evidence.

Verification must not be synchronized until Architecture is repaired and independently closed.

## 55. Complexity and document role

| Contract | Classification |
|---|---|
| Structured error categories | CORE |
| Command outcomes | CORE; currently incomplete |
| Statement W flag/no undo | CORE |
| COMMIT/ABORT failure tables | JUSTIFIED ADVANCED |
| Connection/crash classification | JUSTIFIED ADVANCED |
| Error precedence | CORE |
| Terminal cache/lock ownership | JUSTIFIED ADVANCED |
| Resource/representability distinctions | CORE |
| Checked integer/FLOAT semantics | CORE |
| Optimizer error ownership | CORE |

No unnecessary exception hierarchy, private C++ API, persisted extra state, chronology, or implementation-progress claim was found. The detail is proportionate to the durability and no-undo model.

## 56–61. Finding counts

56. **BLOCKING:** 0.

57. **MAJOR:** 2.

58. **MINOR:** 0.

59. **EDITORIAL:** 0.

60. **DESIGN-SCOPE QUESTIONS:** 0.

61. **FROZEN SEMANTIC QUESTIONS:** 0.

### N39-1 — MAJOR: command outcome set is not total for successful control requests and transport failure

- **Location:** §39.1.1, lines 29008–29018; §39.1.5 C6 rows, lines 29233–29234; §39.1.6 A4 row, line 29271.
- **Owner:** §§15.5–15.6 and §39.1.1.
- **Defect:** Chapter 39 says every statement or transaction-control request has exactly one listed outcome, but `SUCCESS` is defined as leaving an explicit transaction `ACTIVE` or sending autocommit toward COMMIT. That cannot describe successful explicit COMMIT (`COMMITTED`) or ROLLBACK (`ABORTED`). A4 acknowledgement loss likewise has no unambiguous listed command outcome.
- **Fixture:** explicit `ROLLBACK` completes A0–A3 and either successfully acknowledges or loses the A4 response.
- **Consequence:** conforming implementations can disagree whether the command outcome is `SUCCESS`, `ConnectionFailure`, an unlisted aborted-but-unacknowledged result, or another listed outcome.
- **Smallest repair:** clarify §39.1.1’s `SUCCESS` semantics for ordinary statements, explicit COMMIT, and explicit ROLLBACK, and define how a completed control operation combines with transport failure without adding a durable transaction state.

### N39-2 — MAJOR: read-only COMMIT and terminal-state connection-loss windows are missing

- **Location:** §39.1.5, lines 29210–29256; §39.1.7, lines 29275–29294.
- **Owner:** §15.5’s rule that read-only transactions elide C2–C3, plus §9.14 terminal publication.
- **Defect:** the COMMIT failure matrix anchors C4 failure to a durable C3 outcome, while read-only transactions have no C2/C3. The connection-loss matrix similarly classifies pre/post commit append and post-durable commit, but not a read-only COMMIT at C4 or already-terminal COMMITTED/ABORTED states still completing C5/A3 before acknowledgement.
- **Fixture:** explicit read-only transaction enters COMMITTING, elides C2–C3, then C4 terminal publication fails; separately, C4 succeeds and the connection disappears before C5/C6.
- **Consequence:** implementations can disagree whether to retry C4, abort, treat the transaction as committed, enter NC, or release cleanup ownership.
- **Smallest repair:** add read-only C4 failure and post-C4 connection-loss rules to §39.1.5/§39.1.7, and cover terminal ABORTED/COMMITTED cleanup windows without changing persistent transaction semantics.

## 62–65. Required action and final state

62. **Exact next Architecture action:** one minimal Chapter-39 Fix A limited to N39-1 and N39-2:

   - complete the command-outcome mapping;
   - define read-only COMMIT C4 failure;
   - define terminal/post-publication connection loss through C5/A3 and acknowledgement.

63. **Recommended next authorized documentation task:**
   **CHAPTER 39 — ARCHITECTURE FIX A**
   followed by a separate focused independent read-only Architecture closure audit. Do not synchronize Verification yet.

64. **`git diff --check`:** passed with no output.

65. **Final repository confirmation:**

   - HEAD unchanged.
   - Worktree clean.
   - Index clean.
   - Architecture not modified.
   - Verification not modified.
   - Development and Project State not modified.
   - Historical artifacts not modified.
   - No implementation, build, test, sanitizer, benchmark, staging, or commit.
   - Audit-created changes: none.

```text
CHAPTERS 31–38 ARCHITECTURE:
    CLOSED / UNMODIFIED

CHAPTERS 31–38 VERIFICATION:
    CLOSED / UNMODIFIED

CHAPTER 39 ARCHITECTURE:
    NEEDS ARCHITECTURE FIX

CHAPTER 39 VERIFICATION:
    NOT SYNCHRONIZED

CHAPTER 40 REVIEW:
    NOT STARTED

IMPLEMENTATION:
    NOT STARTED

BUILD/TEST/SANITIZER/BENCHMARK:
    NOT RUN

AUDIT-CREATED CHANGES:
    NONE
```

END CHAPTER-39 INITIAL READ-ONLY
ARCHITECTURE REVIEW.