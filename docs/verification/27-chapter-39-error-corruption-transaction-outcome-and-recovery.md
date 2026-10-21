# Chapter 39 — Error, Corruption, Transaction-Outcome, and Recovery Verification

This section composes the existing storage, transaction, SQL, execution, optimizer, and
diagnostic procedures at Chapter 39's error-ownership boundaries. It verifies causal
identity, semantic outcome, cleanup, and client observation independently. A conforming
harness may use structured events, controlled fault hooks, persisted-prefix construction,
and metadata inspection; no private class, exception hierarchy, trace encoding, or C++ API
is required.

## V39-A — Evidence, independent models, and nonvacuity

Maintain one evidence ledger keyed by request, transaction, statement, CommandId, and
attempt. Correlate runtime state, both write flags, original cause and SourceSpan, WAL
reservation/append/durability, applicable `commit_lsn`/`durable_lsn`, C0–C6/A0–A4 stage,
terminal publication, registry/cache state, locks/gates, cleanup, semantic outcome,
database continuation, exactly one command outcome, chained errors, response delivery,
client knowledge, and recovery evidence. A final error string or terminal state alone is
never a sufficient oracle.

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-001 | Run one successful explicit statement, one autocommit DML request, one failed pre-W statement, one MA statement, explicit COMMIT, and explicit ROLLBACK through the ledger. | Every stage preserves correlated identities and separately records server completion, command outcome, transaction state, database health, delivery, cleanup, and client knowledge. |
| V39-002 | Independently model the six-state transition graph, per-statement W flag, statement matrix, five-outcome mapping, C/A stages, connection matrix, persisted-prefix recovery, and causal precedence. | Production observations equal the models; no implementation error string, status cache, or response substitutes for model input evidence. |
| V39-003 | Suppress each essential evidence class independently, including W, WAL append/durability, terminal linearization, lock ownership, cleanup, command outcome, and delivery. | Every dependent result is `NOT VERIFIED / TEST INFRASTRUCTURE INCOMPLETE`, never PASS from a plausible final state. |
| V39-004 | Arm every later injection with fixture identity, target stage, reached-boundary proof, actual trigger, and independently observed consequence. | An untriggered hook, wrong request, unvisited stage, absent denial, or unobserved cleanup is failed setup. |

All later V39 procedures inherit V39-003 and V39-004 whenever they require evidence or
fault injection.

## V39-B — Runtime states, command outcomes, and acknowledgement

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-005 | Exercise legal admission and transition edges among ACTIVE, MUST_ABORT, COMMITTING, COMMITTED, ABORTING, and ABORTED; attempt every forbidden reversal/admission. | Only canonical monotonic edges occur; terminal states never revert and non-ACTIVE states admit no ordinary statement/COMMIT. |
| V39-006 | Produce each of SUCCESS, FAILED_TRANSACTION_REMAINS_ACTIVE, FAILED_TRANSACTION_MUST_ABORT, COMMIT_OUTCOME_UNCERTAIN, and DATABASE_NONCONTINUABLE. | Exactly one command outcome is recorded; transaction state, continuation, causes, and delivery remain separate fields. |
| V39-007 | Complete explicit SELECT/DML, autocommit SELECT/DML, persistent/read-only COMMIT, and ROLLBACK successfully. | SUCCESS is not restricted to ACTIVE: results are respectively ACTIVE, COMMITTED after C4–C5, COMMITTED, and ABORTED. |
| V39-008 | Fail persistent and read-only C6 transmission after authoritative COMMITTED publication and completed C5. | Outcome is COMMIT_OUTCOME_UNCERTAIN plus ConnectionFailure, state remains COMMITTED, success was not delivered, cleanup is complete, and no replay is authorized. |
| V39-009 | Fail explicit-ROLLBACK A4 transmission after A2/A3. | Server outcome is SUCCESS, state ABORTED, ConnectionFailure is separate, no success was delivered, and COMMIT_OUTCOME_UNCERTAIN is absent. |
| V39-010 | Fail A4 while returning an automatic-ABORT result. | Original FAILED_TRANSACTION_MUST_ABORT and causal error remain primary; ABORTED and ConnectionFailure are correlated without fabricated ROLLBACK success. |
| V39-011 | Compose original error, mandatory noncommit, cleanup failure, noncontinuability, irreversible COMMIT/ABORTED, and transport failure in pairwise/sequential cases. | Semantic outcome, primary/chained causes, continuation, and client knowledge follow §39.1.7 rather than first- or last-error precedence. |
| V39-012 | Independently evaluate every row in the command-outcome ledger below, including noncontinuability as primary outcome versus additional continuation state. | Each request has one command outcome and no implicit sixth outcome; terminal truth is never overwritten by client observation. |

## V39-C — First write, statement errors, retry, and result exposure

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-013 | Publish a heap version, old-version xmax/cmax, logical B+ entry, catalog row, and statistics row in separate identified statements. | `current_statement_has_published_write` changes false→true exactly at the first transaction-owned logical publication and sets `has_persistent_writes`. |
| V39-014 | Reserve an LSN/ID, exactly restore provisional mutation, allocate an empty page, perform pure structural maintenance, build a private DDL file, and use locks/snapshots/temp rows/spill. | None alone sets the per-statement W flag; an otherwise identical logical publication positive control does. |
| V39-015 | Perform an earlier successful INSERT, then fail an effect-free SELECT; separately retain only transaction-wide structural WAL and fail the current statement. | Current-statement W controls FA/MA; transaction history does not make the later effect-free failure fatal. |
| V39-016 | Cross W, hide the physical version from ordinary visibility, then inject a dynamic failure. | W never resets, invisibility does not erase publication, and the result is MA with mandatory ABORT. |
| V39-017 | Execute every live §39.1.3 row with before-W and reachable after-W fixtures using the statement-table ledger below. | FA, MA, NC, unreachable, retry, and explicit ABORT results match the exact row and owner; “all errors abort” fails. |
| V39-018 | Put an ordinary demanded row-five expression/constraint error into five-row INSERT closure. | It is detected before W, publishes no mutation/count/RETURNING prefix, and follows the pre-W result. |
| V39-019 | After rows 1–4 publish, inject cancellation, supported-allocation denial, and a legal I/O failure during row 5; repeat UPDATE new-version/old-xmax and DELETE xmax/cmax boundaries. | Statement fails MA, COMMIT is forbidden, automatic ABORT runs, and physical residue is logically aborted garbage without statement undo. |
| V39-020 | Force READ COMMITTED pre-W conflict/revalidation retry and mutate committed input between attempts. | One CommandId is retained, the old snapshot and all attempt-local state are discarded, and a fresh snapshot drives the replacement attempt. |
| V39-021 | Force the same conflict after W, plus RR serialization and deadlock-victim cases. | No same-TxnId statement retry occurs; independent transaction-fatal owners drive MUST_ABORT and preserve locks through ABORT cleanup. |
| V39-022 | Fail an admitted effect-free SELECT and cancel a read-only statement before W. | CommandId is consumed, snapshot cleanup is observed, and the explicit transaction remains ACTIVE absent an independent fatal cause. |
| V39-023 | Fail DML after producing internal RETURNING rows, and inject pre-C0 result allocation/spill/finalization failures. | No prefix/count/R is published; pre-C0 preparation is complete before admission and W determines FA/MA. |
| V39-024 | Inject failures after explicit R, and for autocommit pause through C4, C5, R, and C6. | Post-R delivery cannot reopen success; autocommit exposure waits for C4–C5; cursor error is not FINISHED and draining is not required for semantic success. |
| V39-025 | Run the complete first-write/no-undo/retry/result suite under reordered workers, batching, and physical publication order. | Semantic winner, W classification, result envelope, and cleanup are invariant under permitted physical scheduling. |

## V39-D — Persistent and read-only COMMIT

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-026 | Instrument persistent COMMIT C0–C6 and record append end, commit_lsn, durable_lsn, resident status, terminal cache, registry, locks, caches, cleanup, and response. | Every successful boundary occurs in order; append, durability, terminal publication, cleanup, and acknowledgement remain distinct. |
| V39-027 | Inject C0/C1 failure and C2 preparatory/known-no-append failure with exact restoration. | No terminal COMMIT exists; the transaction takes mandatory ABORT and no same-transaction external COMMIT retry is exposed. |
| V39-028 | Inject C2 append uncertainty and valid append followed by status-frame publication failure. | Both preserve uncertainty/ownership and NC; known no-append is not inferred and ABORT is forbidden after a valid authorizing append. |
| V39-029 | Fail C3 once/repeatedly with exact append bytes, then make safe continuation impossible. | Retryable cases remain COMMITTING; irrecoverable uncertainty becomes NC/recovery-owned and never selectively ABORTS group waiters. |
| V39-030 | Establish `durable_lsn >= commit_lsn`, then fail C4, safe/incoherent C5, disconnect, and C6. | Semantic COMMITTED is irreversible; safe fallback may continue, incoherence yields NC, and unacknowledged clients receive uncertainty. |
| V39-031 | Exercise multiple group-commit waiters under shared flush failure and retry. | Each validly appended waiter retains the same canonical fate; acknowledgement timing cannot turn one into ABORTED. |
| V39-032 | Exhaust ordinary WAL space while retaining terminal closure credit, then inject an impossible terminal-capacity contradiction. | Expected terminal record fits retained credit; contradiction is internal/noncontinuable, not ordinary retry or stranded writer. |
| V39-033 | Run an explicitly read-only COMMIT and observe C0→C1→C4→C5→C6. | No TXN_COMMIT, commit_lsn, read-only durable_lsn event, status entry, or new state exists; C4 is authoritative. |
| V39-034 | For one identified read-only COMMIT, prove C0/C1 completion, C2/C3 elision, a reached C4 attempt, exact coherent nonpublication with no COMMITTED cache/state/registry publication, and V39-033's absent COMMIT WAL/status evidence. Separately keep continuation safe and prove it impossible; observe ownership, A0–A3, command outcome, and response delivery. | Safe continuation retains ownership and retries C4 without success or required ABORT. Impossible continuation requires MUST_ABORT, mandatory ABORT, and server-side `FAILED_TRANSACTION_MUST_ABORT`, never SUCCESS, FA, CU, or NC absent a distinct cause; locks/gates and active membership remain through ABORTING, A2 atomically publishes ABORTED and ends active membership, and A3 releases retained resources, leaving ABORTED after successful cleanup. No `TXN_COMMIT`, `commit_lsn`, read-only `durable_lsn` event, persisted terminal status, or successful COMMIT response is fabricated; delivery is recorded separately. Missing branch/stage evidence is NOT VERIFIED. |
| V39-035 | Confirm C4's atomic COMMITTED linearization, then inject a failure immediately afterward and during C5. | State remains COMMITTED, C5 continues or NC applies, ABORT is forbidden, and no acknowledgement precedes cleanup. |
| V39-036 | Make C4 publication/nonpublication or registry/cache coherence indeterminate. | Database becomes noncontinuable, outcome is COMMIT_OUTCOME_UNCERTAIN, ownership remains retained, and no WAL evidence is fabricated. |
| V39-037 | Disconnect persistent preappend/postappend/post-C3 and read-only pre-C4/in-C4/post-C4 in paired fixtures. | Each pair follows its distinct authorizing evidence, cancellation legality, continuation, cleanup, and client-knowledge rule. |
| V39-038 | Compare persistent/read-only C5 disconnect and C6 loss after both paths are COMMITTED. | Shared terminal cleanup/delivery rules apply without weakening persistent durability or inventing read-only WAL. |
| V39-039 | Independently evaluate all 14 live COMMIT-table rows and every persistent/read-only contrast row below. | Every branch reaches its exact stage and supplies a unique expected state/outcome/cleanup; the read-only exact-nonpublication row must distinguish safe retry from impossible continuation requiring mandatory ABORT, `FAILED_TRANSACTION_MUST_ABORT`, and A2/A3 evidence. A final COMMITTED bit alone cannot PASS. |

## V39-E — ABORT, connection loss, recovery, and terminal ownership

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-040 | Instrument explicit and automatic A0–A4 with/without persistent WAL-visible state. | ABORT publishes semantic outcome without physical user-data undo, and explicit versus automatic command results remain distinct. |
| V39-041 | Inject known A1 no-append, uncertain append/status publication, and a known-failure retry that cannot finish. | Known failure retains ABORTING and retries; uncertainty/inability becomes NC; no path restores ACTIVE/COMMIT eligibility. |
| V39-042 | Fail A2 publication, then separately publish A2 and fail A3 coherently/incoherently. | No false ABORTED/lock release before A2; after A2 the state stays ABORTED and incoherent A3 cleanup yields NC. |
| V39-043 | Exercise A4 delivery/loss for explicit ROLLBACK and automatic ABORT. | V39-009/V39-010 outcomes hold and A3 completion is independently observed before any response attempt. |
| V39-044 | Enter MUST_ABORT via post-W failure, RR serialization, deadlock, and another fatal cause; inject failures before/during/after A2, A3, and A4. | Original cause remains primary, COMMIT/work stay forbidden, ownership persists to the canonical release point, and cleanup causes are chained. |
| V39-045 | Execute all 14 live connection-loss rows. | Continued COMMIT/ABORT, state, lock lifetime, cleanup, continuation, and client knowledge match the row; C5/A3 are never abandoned. |
| V39-046 | Disconnect during an identified read-only C4 attempt; separately prove exact coherent nonpublication with safe continuation, exact coherent nonpublication with continuation impossible, authoritative publication, and indeterminate state. Observe retained ownership, V39-033's absent COMMIT WAL, ABORT A2/A3 where applicable, and response delivery. | Safe continuation retries under retained ownership. Impossible continuation requires mandatory ABORT and server-side `FAILED_TRANSACTION_MUST_ABORT`, ownership through A2/A3, and ABORTED after successful cleanup; no COMMIT success/error response is fabricated on the lost connection. The client cannot determine the outcome from that connection under §39.1.7, separately from the server's known failed-COMMIT outcome. Publication remains COMMITTED and continues C5; indeterminacy yields NC+CU without default ABORT. Blind ABORT, early release, or invented read-only COMMIT WAL fails. |
| V39-047 | Disconnect after COMMITTED publication before/after C5 and after C6, then after ABORTED publication before/after A3 and A4. | Terminal state never changes; outstanding cleanup continues and no undelivered acknowledgement is fabricated. |
| V39-048 | Construct every valid/torn/incomplete persisted WAL prefix for the seven recovery rows. | Winner/loser classification uses complete persisted evidence only; runtime cache and missing acknowledgement cannot override it. |
| V39-049 | Crash read-only transactions before, during, and after runtime C4. | No read-only persistent evidence is invented; lack of persistent references makes recovery safe while precrash client observation remains separately recorded. |
| V39-050 | Observe §9.14 terminal-cache/state/active-removal linearization with TUPLE_WRITE, UNIQUE_KEY, TableWriterGate, SchemaLock, STATS_PUBLISH, MANIFEST_CHANGE, and snapshots. | No lock/gate becomes acquirable before terminal publication; C5/A3 release each retained owner exactly once. |
| V39-051 | Disconnect/cancel deadlock victims and nonterminal transactions while workers/tasks remain active. | Workers quiesce, wait-for edges remain complete, transaction ownership survives until terminal cleanup, and an ordinary deadlock is not corruption. |
| V39-052 | Independently evaluate all eight ABORT rows, 14 connection rows, and seven recovery rows below. | Every row has a reached-stage fixture, persisted/runtime evidence, cleanup, client/database result, and nonvacuous oracle. |

## V39-F — Caches, precedence, diagnostics, and subsystem cleanup

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-053 | After COMMITTED publication fail descriptor installation; separately use safe invalidation/bypass, retain valid-old/missing statistics, and create incoherent ownership. | Safe fallback preserves continuation; incoherence yields NC; transaction remains COMMITTED and stale cache is never authority. |
| V39-054 | Force terminal-cache/registry/status-page disagreement before and after atomic publication. | Runtime terminal linearization is authoritative in-process; incoherence is controlled/NC, never guessed lock release or false state reversal. |
| V39-055 | Compose UNIQUE/demanded SQL errors with ABORT I/O, durable COMMIT with C5/transport failure, read-only indeterminate C4 with disconnect, and completed ROLLBACK with A4 loss. | Primary cause, semantic result, noncommit, NC, chained causes, transport, and client knowledge agree with the precedence model. |
| V39-056 | Expose diagnostics for request/Txn/CommandId/W, C/A stage, WAL evidence, terminal publication, cleanup, outcome, continuation, delivery, uncertainty, and recovery. | Diagnostics agree with authoritative evidence and cannot fabricate COMMIT, ABORT, durability, delivered response, or optimizer proof. |
| V39-057 | Fail pipeline execution, materialization, allocation, spill, scalar subquery, and cancellation while workers are active. | Query-owned state/resources unwind and tasks quiesce; transaction locks remain transaction-owned and published W is not erased. |
| V39-058 | Fail cleanup itself after each V39-057 cause. | Original cause remains primary, cleanup cause is chained, no runnable orphan remains, and transaction consequence still comes from §39.1. |
| V39-059 | Run CREATE/DROP/ANALYZE failures before private/prepublication work, at first catalog/statistics W, and after committed cache publication. | W/FA/MA, orphan ownership, complete-generation filtering, cache fallback, and irreversible COMMIT retain their canonical owners. |
| V39-060 | Run database lifecycle shutdown/noncontinuable handling with ACTIVE, MUST_ABORT, COMMITTING, ABORTING, and terminal cleanup in progress. | Admission closes, terminal protocols finish or recovery owns them, exclusivity is retained, and no clean stop or successful acknowledgement is fabricated. |
| V39-061 | Validate SourceSpan and original/chained structured causes through command, transaction, connection, and recovery reporting. | Applicable source ownership survives; delayed user-error discovery stays a user error and transport/cleanup cannot erase it. |
| V39-062 | Evaluate all 20 forbidden-implementation counterexamples in the ledger below. | Each forbidden behavior is actively attempted and independently rejected; quotation or final output equality alone cannot PASS. |

## V39-G — Front-end, execution, numeric, spill, and optimizer ownership

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-063 | Produce every §39.2 category at its owning stage with applicable SourceSpan, plus delayed user error and malformed internal logical plan. | User/unsupported/configured-limit causes retain their category; malformed internal plans are invariant failures and never execute. |
| V39-064 | Contrast configured FrontEndResourceLimit, supported AST allocation denial, malformed source, unrepresentable input, and binder invariant failure. | Configured safety bound, OutOfMemory, language error, representability owner, and internal cause remain distinct. |
| V39-065 | Produce every §39.3 category and correlate operator propagation/cleanup with the §39.1 outcome matrix. | Execution operators preserve causes and never invent transaction rollback/retry policy. |
| V39-066 | Contrast unsupported exact value/row/address representation, supported allocation denial, runtime hard gate, configured optimizer bound, and guard-clear optimizer backing denial. | Results are controlled representability ExecutionError, OutOfMemory, OptimizerResourceLimit, or applicable internal/config error without truncation/wrap. |
| V39-067 | Inject temp creation/ENOSPC/read/write/offset/framing/checksum/range failures, self-generated spill invariant defect, and persistent-page corruption. | SpillIOError, internal defect, and persistent corruption/NC remain distinct and partial temp resources are reclaimed. |
| V39-068 | Exercise integer add/subtract/multiply/unary-minus overflow, MIN/-1, divide/remainder by zero, exact SUM intermediate, and SUM/COUNT final conversion. | Checked ArithmeticError/NUMERIC_OVERFLOW occurs only at the canonical scalar/final boundary; no C++ overflow or premature aggregate error occurs. |
| V39-069 | Exercise FLOAT64 finite/±0, ±0/±0, infinity/infinity, NaN, signed zero, exact-zero aggregate, rounding, and scalar versus reduction-tree accumulation. | Required IEEE results and canonical exposed NaN/+0 follow Chapters 17/29; integer zero-divisor errors and unsafe compiler reassociation fail. |
| V39-070 | Exercise malformed optimizer config/epsilon/raw cost, valid saturation, unavailable capability, invalid final plan, configured guard/exhaustion, guard-clear backing denial, recovery, mixed causes, and high legal cost. | OptimizerError, OptimizerResourceLimit, OutOfMemory, ineligibility, invariant failure, success, and ordinary metadata follow §§39.3–39.4 and V38 evidence. |

## V39-H — Subqueries and frozen-owner integration

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-071 | Exercise scalar zero/one/second row, demanded second-row error, child failure, IN/NOT IN full RHS with NULL/duplicates, and EXISTS lawful stop. | CardinalityViolation maps correctly, demanded errors/builds remain observable, and §39.1—not subquery planning—owns transaction consequence. |
| V39-072 | Compose V39 outcomes with Chapters 31–32 DML/result/worker procedures. | Candidate closure, W/C/R, retry, publication, cancellation, task quiescence, no partial success, and transaction-owned locks remain frozen. |
| V39-073 | Compose statistics/cache/proof/cost/search/final-validation and Chapter-38 causal-resource procedures. | Chapters 33–38 remain unchanged: estimates/cost cannot prove SQL facts, invalid plans do not execute, and configured versus physical resource causes stay distinct. |
| V39-074 | Apply Chapter-40 observability and Chapter-41 verification handoffs to the complete V39 ledger. | Required diagnostic dimensions are observable without adding Chapter-40 policy, benchmark thresholds, or claims of execution. |

## V39-I — Normative tables and adversarial inventories

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-075 | Parse live Chapter 39 and execute every row in the command, statement, COMMIT, ABORT, connection, and recovery ledgers below. | Counts and row identities equal the live source; each row has direct V39 integration plus applicable reused component evidence. The read-only C4 nonpublication row reaches both continuation conditions and requires V39-034's mandatory ABORT, `FAILED_TRANSACTION_MUST_ABORT`, A2/A3, and absent COMMIT WAL when continuation is impossible. |
| V39-076 | Execute every transaction-outcome adversarial case A–DE below. | All 109 identifiers resolve to a controlled fixture and final Architecture outcome; stale post-W ordinary-DML premises are rejected by closure and replaced by legal dynamic controls. |
| V39-077 | Execute every acknowledgement and terminal-cleanup adversarial case A–AN below. | All 40 identifiers directly test successful controls, acknowledgement, read-only C4, terminal cleanup, precedence, and persistent/read-only separation. Case K fails if its impossible-continuation fixture lacks exact nonpublication, mandatory ABORT, `FAILED_TRANSACTION_MUST_ABORT`, A2/A3 ownership, or absent read-only COMMIT WAL, regardless of an ABORT/no-success label. |

## V39-J — Static integrity and document role

| ID | Controlled procedure | Independent observation and required result |
|---|---|---|
| V39-078 | Parse the live V39 section, Architecture tables, forbidden list, adversarial inventories, external references, named headings, and essential-evidence clauses. | Exactly one V39-A–J and V39-001–078 definition exists; IDs are contiguous/unique, rows are nonorphaned, SET A=SET B, headings resolve, 5/22/14/8/14/7 tables, 20 prohibitions, the transaction-outcome and acknowledgement/terminal-cleanup cases are substantively covered. Compare the actual V39-034 fixture, COMMIT/connection rows, and acknowledgement and terminal-cleanup case K oracle against §39.1.5: exact nonpublication plus impossible continuation must require mandatory ABORT, `FAILED_TRANSACTION_MUST_ABORT`, A2/A3, and no read-only COMMIT WAL; labels or row presence alone fail. Missing evidence/vacuous faults/invented semantics cannot PASS. |

## V39 reuse keys

The ledgers below use these exact reusable-oracle keys:

- `SEM` = V20-12 through V20-17, V20-24; V25-H through V25-K, V25-M
  through V25-O; V29-F, V29-I through V29-M, V29-Q.
- `TXN` = V21-2, V21-3, V21-13, V21-14, V21-23, V21-26; V31-A
  through V31-N.
- `MEM` = V24-D, V24-H, V24-J through V24-M; V26-H through V26-L;
  V32-H through V32-M.
- `PLAN` = V22-K; V33-E, V33-F; V34-D, V34-H through V34-J; V36-C,
  V36-I; V37-I; V38-052 through V38-067.

Named reusable suites are `Numeric Exhaustion and Terminal-Boundary Verification`,
`Transaction identity, snapshot, and status verification`, `Durable prefix, group commit,
COMMIT, and WAL-before-data`, `Non-Crash WAL/MTR Failure Injection`, `Statement Failure and
Transaction-State Tests`, `CommandId, snapshot, retry, and RETURNING`, `COMMIT
Fault-Injection Tests`, `ABORT Fault-Injection Tests`, `Recovery Property Tests`, `Locking
and Gate Tests`, `Front-End Error and Source-Span Tests`, and `Affected-row and
result-envelope verification`.

## V39 normative-table coverage ledger

Every row below inherits V39-001–004. `Fixture/result` states the controlled trigger and
independently expected state/outcome; `Reuse` identifies component evidence, never a
replacement for the V39 integration procedure.

### Command outcomes — 5 rows

| Architecture row | V39 procedure | Reuse | Fixture/result |
|---|---|---|---|
| SUCCESS | V39-006/V39-007 | TXN | Explicit statement ACTIVE; autocommit/COMMIT COMMITTED; ROLLBACK ABORTED; delivery separate. |
| FAILED_TRANSACTION_REMAINS_ACTIVE | V39-006/V39-017 | TXN/SEM | Recoverable pre-W failure cleans statement and leaves ACTIVE/COMMIT-eligible. |
| FAILED_TRANSACTION_MUST_ABORT | V39-006/V39-019/V39-034/V39-044 | TXN/MEM | Post-W or independently fatal cause enters MUST_ABORT and mandatory ABORT; a failed read-only COMMIT with proven C4 nonpublication and impossible continuation takes mandatory ABORT with this exact server-side command outcome. |
| COMMIT_OUTCOME_UNCERTAIN | V39-008/V39-028–V39-030/V39-036 | TXN | Uncancellable unacknowledged COMMIT retains canonical server/recovery truth. |
| DATABASE_NONCONTINUABLE | V39-011/V39-055/V39-060 | TXN/MEM | Primary fatal result or additional continuation gate closes ordinary database activity. |

### Statement-error matrix — 22 rows

| Architecture row | V39 procedure | Reuse | Before/after-W fixture and expected result |
|---|---|---|---|
| Parse/lex | V39-017/V39-063 | SEM | FA before; delayed reachable after-W uses MA. |
| Bind/name/type/catalog | V39-017/V39-063 | SEM | FA before; delayed reachable after-W uses MA. |
| Planner/optimizer resource | V39-017/V39-070 | PLAN | FA/MA by W; final-plan invariant is NC. |
| Ordinary user-DML scalar/subquery/final-row expression/cast/assignment | V39-017–V39-019 | TXN/SEM | Candidate closure gives FA before W; ordinary after-W case is rejected as unreachable. |
| Non-DML expression/arithmetic/cast | V39-017/V39-065/V39-068 | SEM | FA before; MA after where owning statement can reach W first. |
| Aggregate COUNT/integer-SUM final overflow | V39-017/V39-068 | SEM | Deterministic Finalize error gives FA/MA by W. |
| User-DML NOT NULL/UNIQUE/PRIMARY KEY | V39-017–V39-019 | TXN | Closure gives FA before W; ordinary after-W discovery is invariant failure. |
| READ COMMITTED conflict/revalidation | V39-017/V39-020/V39-021 | TXN | Retry/FA before W; MA after W. |
| RR serialization/deadlock victim | V39-017/V39-021/V39-044 | TXN | MA on both sides; ownership retained through A3. |
| Cancellation/lock cancellation | V39-017/V39-019/V39-022 | TXN/MEM | FA before W; MA after W absent stronger owner. |
| OutOfMemory | V39-017/V39-066 | MEM/PLAN | FA before W; MA after W; cause remains OOM. |
| SpillIOError/temp capacity/corruption | V39-017/V39-067 | MEM | FA before W; MA after W; temporary owner retained. |
| Persistent-page corruption | V39-017/V39-067 | MEM | NC regardless of W. |
| Known BufferPool flush/WAL durability failure | V39-017/V39-027–V39-030 | TXN/MEM | FA/MA by W; terminal protocol uses COMMIT/ABORT matrices. |
| Known WAL append failure with exact restoration | V39-014/V39-017 | TXN | Failing primitive does not set W; earlier statement W still controls. |
| MTR provisional failure with exact restoration | V39-014/V39-017 | MEM | Exact pre-MTR state; FA/MA from independent W. |
| MTR/append/restoration noncontinuable | V39-017/V39-060 | MEM | NC regardless of W. |
| Known raw page/file I/O with coherent state | V39-017/V39-067 | MEM | FA/MA by W; uncertainty instead NC. |
| DDL namespace synchronization/publication | V39-017/V39-059 | TXN | Precatalog coherent orphan → FA; postcatalog W → MA; incoherence → NC. |
| ANALYZE collection/encoding | V39-017/V39-059 | TXN/PLAN | FA before first statistics row; MA after W. |
| Explicit user ROLLBACK | V39-007/V39-040/V39-043 | TXN | Semantic ABORT, not statement failure; S only after A3/A4 delivery. |
| Internal invariant failure | V39-017/V39-054/V39-065 | SEM/PLAN | NC regardless of W; assertion alone is not policy. |

### COMMIT matrix — 14 rows

| Architecture row | V39 procedure | Reuse | Fixture/result |
|---|---|---|---|
| C0/C1 failure | V39-027 | TXN/MEM | No record; mandatory ABORT; never success. |
| C2 preparation/known no-append | V39-027 | TXN | Exact restoration then ABORT; no external same-TxnId retry. |
| C2 append uncertain | V39-028 | TXN | NC, no ABORT/publication/release; CU/recovery. |
| Valid append, status publication incomplete | V39-028 | TXN/MEM | NC, uncancellable, CU; recovery may commit. |
| C3 retryable write/sync failure | V39-029 | TXN | Remain COMMITTING and retry; no acknowledgement. |
| C3 continuation impossible | V39-029 | TXN | NC; persisted prefix decides; CU. |
| Persistent post-C3/pre-during-C4 failure | V39-030 | TXN | Irrevocably COMMITTED; NC; never ABORTED. |
| Read-only exact pre-C4/nonpublication | V39-034/V39-039 | TXN | Coherent nonpublication with safe continuation retains ownership and retries without success. Prove continuation impossible in a separate fixture: mandatory ABORT, `FAILED_TRANSACTION_MUST_ABORT`, ownership through A2/A3, then ABORTED after successful cleanup; no COMMIT success or invented read-only COMMIT WAL/status. |
| Read-only C4 indeterminate | V39-036 | TXN | NC+CU; ownership retained; no fake WAL. |
| Read-only C4 published | V39-035 | TXN | COMMITTED; shared C5/C6; never ABORT. |
| C5 safe cache fallback | V39-030/V39-053 | TXN/PLAN | COMMITTED; installed/invalidate/bypass safe; success may follow. |
| C5 incoherent cleanup | V39-030/V39-053 | TXN | COMMITTED+NC; retain unsafe ownership; CU pre-C6. |
| C6 delivered | V39-007/V39-026 | TXN | COMMITTED, cleanup complete, S delivered. |
| C6 transport failure | V39-008 | TXN | COMMITTED, CU+ConnectionFailure, no delivered success. |

### ABORT matrix — 8 rows

| Architecture row | V39 procedure | Reuse | Fixture/result |
|---|---|---|---|
| Before A1, no persistent state | V39-040 | TXN | Publish ABORTED, then release/acknowledge. |
| Known preparation/TXN_ABORT no-append | V39-041 | TXN | Retain ABORTING/locks and retry; never ACTIVE. |
| Append/status publication uncertain | V39-041 | TXN | NC; no ordinary release; recovery. |
| Known-failure retry cannot finish | V39-041 | TXN | NC; original cause retained; loser recovery. |
| A2 publication failure | V39-042 | TXN | NC; no false cache/state/release. |
| A3 failure after ABORTED | V39-042/V39-053 | TXN | Remain ABORTED; ownership incoherence → NC. |
| A4 delivered | V39-007/V39-043 | TXN | Explicit ROLLBACK S; automatic ABORT returns original error. |
| A4 transport failure | V39-009/V39-010/V39-043 | TXN | ABORTED; ConnectionFailure separate; no CU. |

### Connection-loss matrix — 14 rows

| Architecture row | V39 procedure | Reuse | Fixture/result |
|---|---|---|---|
| ACTIVE | V39-045 | TXN | Automatic ABORT and cleanup. |
| MUST_ABORT | V39-044/V39-045 | TXN | Continue/join mandatory ABORT. |
| Persistent preappend COMMITTING | V39-037/V39-045 | TXN | Cancel COMMIT and ABORT; not CU. |
| Persistent postappend/pre-C3 | V39-037/V39-045 | TXN | Uncancellable; continue or NC/recovery; client uncertain. |
| Persistent post-C3/pre-C4 | V39-030/V39-045 | TXN | Semantically COMMITTED; finish C4–C5 or NC. |
| Read-only before C4 | V39-037/V39-045 | TXN | Cancel and ABORT; no fake WAL. |
| Read-only during C4 | V39-046 | TXN | Exact nonpublication with safe retry versus impossible continuation is distinguished; the latter requires server-side `FAILED_TRANSACTION_MUST_ABORT`, mandatory ABORT, A2/A3 ownership, and ABORTED after cleanup, while no result is delivered on the lost connection and the client cannot determine its outcome under §39.1.7. Published COMMITTED and indeterminate NC+CU remain separate; no COMMIT WAL is invented. |
| COMMITTED, C5 incomplete | V39-047/V39-050 | TXN | Continue C5, retain ownership, never ABORT. |
| COMMITTED, C5 complete/C6 pending | V39-008/V39-047 | TXN | Close; remain COMMITTED; client uncertain. |
| COMMITTED after C6 | V39-047 | TXN | Close; state/cleanup unchanged. |
| ABORTING before A2 | V39-044/V39-045 | TXN | Continue publication/cleanup; never revive. |
| ABORTED, A3 incomplete | V39-042/V39-047 | TXN | Continue A3, retain locks until release. |
| ABORTED, A3 complete/A4 pending | V39-009/V39-047 | TXN | Close; remain ABORTED; ConnectionFailure, not CU. |
| ABORTED after A4 | V39-047 | TXN | Close; terminal state/cleanup unchanged. |

### Crash-recovery matrix — 7 rows

| Architecture row | V39 procedure | Reuse | Fixture/result |
|---|---|---|---|
| ACTIVE or MUST_ABORT | V39-048 | TXN | No complete commit; loser→ABORTED. |
| COMMITTING without surviving complete COMMIT | V39-048 | TXN | Torn/incomplete suffix ignored; loser→ABORTED. |
| COMMITTING with complete persisted COMMIT | V39-048 | TXN | Winner→COMMITTED even without ack. |
| COMMITTING after durable commit | V39-048 | TXN | COMMITTED. |
| ABORTING with/without abort record | V39-048 | TXN | ABORTED directly or by loser resolution. |
| COMMITTED | V39-048/V39-049 | TXN | Remains COMMITTED; runtime cache loss irrelevant. |
| ABORTED | V39-048 | TXN | Remains ABORTED. |

## V39 forbidden-implementation ledger

| §39.1.8 item and forbidden behavior | V39 procedure | Reuse | Controlled counterexample and required observation |
|---|---|---|---|
| 1 — commit partial DML after post-W failure | V39-019/V39-062 | TXN | Dynamic row-5 failure; MA/ABORT and no committed prefix. |
| 2 — retry after any transaction-owned W | V39-021/V39-062 | TXN | Replacement attempt is forbidden and no same-TxnId success occurs. |
| 3 — reuse failed CommandId/snapshot | V39-020/V39-022/V39-062 | TXN | CommandId consumption and fresh RC snapshot are observed. |
| 4 — treat exactly restored provisional mutation as garbage | V39-014/V39-062 | TXN/MEM | Before/after bytes and W prove exact restoration is not publication. |
| 5 — rewrite durable COMMITTED as ABORTED | V39-030/V39-062 | TXN | Post-C3 failures preserve COMMITTED in runtime and recovery. |
| 6 — acknowledge COMMIT before C4/C5 | V39-026/V39-035/V39-062 | TXN | Early response attempt is rejected for both paths. |
| 7 — release transaction locks before terminal publication | V39-050/V39-062 | TXN | Competing acquisition remains blocked until C4/A2. |
| 8 — admit work/COMMIT in MUST_ABORT | V39-005/V39-044/V39-062 | TXN | Admission is rejected while cleanup proceeds. |
| 9 — make later effect-free SELECT error fatal from prior writes | V39-015/V39-062 | TXN | Current W=false gives FA/ACTIVE despite transaction history. |
| 10 — classify all OOM/spill/disk-full identically | V39-017/V39-066/V39-067 | MEM/PLAN | Stage/W/cause produce distinct error and transaction results. |
| 11 — expose failed RETURNING prefix or pre-C4/C5 autocommit result | V39-023/V39-024/V39-062 | TXN | No prefix/R before complete success. |
| 12 — let cache failure redefine durable outcome | V39-030/V39-053/V39-062 | TXN/PLAN | COMMITTED persists; fallback or NC is separate. |
| 13 — omit gate dependencies from wait-for graph | V39-050/V39-051/V39-062 | TXN | Cross-resource cycle is present and deterministic victim handling remains. |
| 14 — release deadlock-victim gates before ABORTED publication | V39-044/V39-050/V39-062 | TXN | Competitor remains blocked through A2 and release occurs at A3. |
| 15 — make post-R cursor error retroactive/retryable | V39-024/V39-062 | TXN/MEM | Statement/count/state survive and no retry begins. |
| 16 — report FINISHED after failed remaining delivery | V39-024/V39-062 | TXN/MEM | Cursor exposes terminal error distinct from EOS. |
| 17 — require cursor drain to preserve success | V39-024/V39-062 | TXN | Abandonment cleans result owner without changing success. |
| 18 — admit autocommit C0 with fallible result preparation incomplete | V39-023/V39-062 | TXN/MEM | Every required preparation event precedes C0; injected failure prevents C0. |
| 19 — treat accounting grant as allocation success | V39-023/V39-066/V39-062 | MEM | Grant succeeds, physical allocation fails, C0 remains unentered. |
| 20 — perform fallible C-to-R work or redirect COMMITTED to ABORT | V39-024/V39-062 | TXN/MEM | Required C-to-R operation count is zero; injected invariant failure preserves COMMITTED. |

## V39 transaction-outcome adversarial matrix — A through DE

| Case | Canonical owner | V39 procedure / reuse | Controlled fixture and independent expected result |
|---|---|---|---|
| A | §§39.1.2–.3 | V39-015/V39-017; TXN | Failed SELECT, W=false → FA/ACTIVE. |
| B | §§39.1.2–.3 | V39-015; TXN | Earlier INSERT does not make later effect-free SELECT fatal. |
| C | §§12.12,39.1.2 | V39-014; TXN | LSN reservation alone leaves W=false. |
| D | §§12.12,39.1.2 | V39-014; MEM | Exact provisional restoration leaves W=false and no garbage. |
| E | §39.1.2 | V39-014; TXN | Empty structural page allocation alone leaves W=false. |
| F | §39.1.2 | V39-013; TXN | First heap version publication sets W. |
| G | §39.1.2 | V39-013; TXN | First old-version xmax/cmax sets W. |
| H | §39.1.2 | V39-013; TXN | First transaction-owned logical index entry sets W. |
| I | §39.1.2 | V39-013/V39-059; TXN | First transaction-owned catalog row sets W. |
| J | §39.1.2 | V39-013/V39-059; TXN | First statistics row sets W. |
| K | §§21.16.1,31,39.1.4 | V39-018; TXN/SEM | Ordinary row-five error closes before W; no partial publication/RETURNING. |
| L | §§31,39.1.3 | V39-018/V39-019; TXN | UPDATE failure before W gives prewrite result. |
| M | §§31,39.1.4 | V39-019; TXN | Dynamic UPDATE failure after version/xmax W gives MA/ABORT. |
| N | §§31,39.1.4 | V39-019; TXN | Dynamic DELETE failure after xmax/cmax gives MA/ABORT. |
| O | §§15.7,39.1.3 | V39-020; TXN | RC conflict before W permits clean retry/FA. |
| P | §§15.7,39.1.3 | V39-021; TXN | RC conflict after W forbids retry and gives MA. |
| Q | §39.1.3 | V39-021; TXN | RR serialization is MA even before W. |
| R | §§11.13,39.1.3 | V39-021/V39-044; TXN | Deadlock victim is MA; ownership survives to A3. |
| S | §39.1.3 | V39-017/V39-022; TXN | Cancellation before W gives FA absent stronger owner. |
| T | §39.1.3 | V39-019; TXN | Cancellation after W gives MA. |
| U | §§39.1.3,39.3 | V39-017/V39-066; MEM | OOM before W gives FA, cause OOM. |
| V | §§39.1.3,39.3 | V39-019/V39-066; MEM | OOM after W gives MA, cause OOM. |
| W | §§39.1.3,39.3 | V39-017/V39-067; MEM | SpillIOError before W gives FA. |
| X | §§39.1.3,39.3 | V39-019/V39-067; MEM | SpillIOError after W gives MA. |
| Y | §§3.3,39.1.3 | V39-017/V39-067; MEM | Persistent-page corruption is NC regardless of W. |
| Z | §§39.1.3,39.2–.4 | V39-017/V39-054/V39-065; SEM/PLAN | Internal invariant failure is NC, not user error. |
| AA | §39.1.2 | V39-015; TXN | Earlier structural WAL/transaction-wide bit does not substitute for current W. |
| AB | §39.1.2 | V39-016; TXN | Physical invisibility does not erase crossed W. |
| AC | §§15.7,39.1.4 | V39-021; TXN | Post-W same-TxnId retry is rejected. |
| AD | §§9.9,15.7,39.1.4 | V39-020; TXN | Pre-W retry gets fresh snapshot and state. |
| AE | §§31.9,39.1.4 | V39-023; TXN | Failed DML exposes no RETURNING prefix. |
| AF | §§15.5,31.9,39.1.4 | V39-024; TXN | Autocommit RETURNING waits for C4–C5. |
| AG | §§15.5,39.1.5 | V39-027; TXN | C0 precondition failure has no commit and mandatory ABORT. |
| AH | §§12.10.5,39.1.5 | V39-027; TXN | C2 preparatory failure restores exact state and ABORTS. |
| AI | §§12.12,39.1.5 | V39-027; TXN | Known commit no-append differs from uncertainty. |
| AJ | §§12.12,39.1.5 | V39-028; TXN | Uncertain append yields NC+CU/recovery. |
| AK | §§15.5,39.1.5 | V39-028/V39-037; TXN | Postappend cancellation cannot redirect to ABORT. |
| AL | §§12.15,39.1.5 | V39-029/V39-031; TXN | Group flush failure preserves all appended waiters. |
| AM | §§15.5,39.1.5 | V39-030; TXN | Durable C3 then C4 failure remains COMMITTED+NC. |
| AN | §§39.1.5,.8 | V39-030/V39-053; PLAN | Safe C5 invalidation/bypass preserves COMMITTED/continuation. |
| AO | §§39.1.5,.8 | V39-030/V39-053; TXN | Incoherent C5 keeps COMMITTED and makes database NC. |
| AP | §§39.1.1,.5 | V39-008; TXN | C6 socket failure gives COMMITTED, CU+ConnectionFailure. |
| AQ | §39.1.5 | V39-008; TXN | Client uncertainty does not authorize blind replay. |
| AR | §39.1.6 | V39-041; TXN | Known TXN_ABORT no-append retries ABORTING. |
| AS | §39.1.6 | V39-041; TXN | Uncertain abort append yields NC/recovery. |
| AT | §§9.14,39.1.6 | V39-042; TXN | A2 publication failure gives NC and no false release. |
| AU | §39.1.6 | V39-042/V39-053; TXN | A3 failure after ABORTED keeps ABORTED+NC. |
| AV | §§39.1.1,.6 | V39-009/V39-043; TXN | A4 loss keeps ABORTED, server S, separate transport cause. |
| AW | §§9.4,39.1.1 | V39-005/V39-044; TXN | MUST_ABORT rejects COMMIT. |
| AX | §§9.14,39.1.7–.8 | V39-044/V39-050; TXN | MUST_ABORT cannot release locks before A2/A3. |
| AY | §§11.13,39.1.8 | V39-044/V39-050; TXN | Deadlock victim retains gates through terminal publication. |
| AZ | §§4.3.2.4,39.1.5 | V39-032; TXN | Terminal-capacity contradiction is internal/NC. |
| BA | §§7.10,39.1.3 | V39-017; MEM | Unobserved background flush error does not retroactively fail statement. |
| BB | §§12.12,39.1.3 | V39-017/V39-029; TXN | Required WAL operation observes its actual known failure. |
| BC | §§12.12.4,39.1 | V39-014/V39-060; MEM | Impossible restoration gives NC. |
| BD | §§39.1.5,.8 | V39-030/V39-053; PLAN | Later cache failure cannot revoke COMMITTED. |
| BE | §39.1.7 | V39-045; TXN | Disconnect ACTIVE drives ABORT. |
| BF | §39.1.7 | V39-044/V39-045; TXN | Disconnect MUST_ABORT continues mandatory cleanup. |
| BG | §39.1.7 | V39-037/V39-045; TXN | Persistent preappend loss cancels/ABORTS. |
| BH | §39.1.7 | V39-037/V39-045; TXN | Persistent postappend loss continues COMMIT/recovery. |
| BI | §39.1.7 | V39-030/V39-047; TXN | Postdurable/preack loss remains COMMITTED/client uncertain. |
| BJ | §39.1.7 | V39-045; TXN | Disconnect ABORTING continues ABORT. |
| BK | §§13.20,39.1.7 | V39-048; TXN | COMMITTING without surviving complete commit is loser. |
| BL | §§13.20,39.1.7 | V39-048; TXN | Complete persisted commit is winner. |
| BM | §§13.20,39.1.7 | V39-048; TXN | ABORTING without surviving abort is loser→ABORTED. |
| BN | §§12.5,13.20,39.1.7 | V39-048; TXN | Torn/incomplete WAL suffix gives no commit. |
| BO | §39.1.7 | V39-055; TXN | UNIQUE remains primary; fatal abort I/O is chained NC. |
| BP | §§39.1.5,.7 | V39-008/V39-055; TXN | COMMITTED plus transport loss remains COMMITTED/CU. |
| BQ | §§9.14,39.1.8 | V39-054; TXN | Terminal cache disagreement is controlled/NC, not guessed. |
| BR | §§34,39.1.8 | V39-053; PLAN | Safe catalog fallback preserves continuation. |
| BS | §§39.1.5,.8 | V39-053; TXN | Postcommit ownership incoherence gives COMMITTED+NC. |
| BT | §§34,39.1.8 | V39-053; PLAN | Valid older statistics descriptor may remain. |
| BU | §§11.13,39.1.8 | V39-050/V39-051; TXN | Missing gate dependency is detected by wait-for oracle. |
| BV | §§11.13,39.1.8 | V39-051; TXN | Ordinary deadlock is MA, not corruption/NC. |
| BW | §§39.1.3,39.3 | V39-017/V39-066; MEM | Physical OOM is not automatically database NC. |
| BX | §39.3 | V39-067; MEM | Temporary spill checksum defect is SpillIOError, not persistent corruption. |
| BY | §§4–8,39.3 | V39-067; MEM | Persistent-page corruption is not downgraded to spill I/O. |
| BZ | §§18.8,39.2 | V39-061/V39-063; SEM | Parser error carries canonical SourceSpan. |
| CA | §39.2 | V39-063; SEM | Unsupported feature remains user-facing, not invariant failure. |
| CB | §§18.17,39.2 | V39-064; SEM | Configured front-end bound is FrontEndResourceLimit. |
| CC | §§18.17,39.2 | V39-064; MEM | Physical AST allocation denial is OutOfMemory. |
| CD | §§23.9,39.3 | V39-066; MEM | Unrepresentable VARCHAR is controlled ExecutionError, never truncation. |
| CE | §§24,39.3 | V39-066; MEM | Supported exact allocation denial is OutOfMemory. |
| CF | §§24,39.3 | V39-066; MEM | Runtime hard gate without progress is OutOfMemory. |
| CG | §39.3 | V39-067; MEM | Spill ENOSPC remains SpillIOError, not permanent corruption. |
| CH | §§24,39.3 | V39-067; MEM | Unsupported spill offset/range is SpillIOError. |
| CI | §39.3 | V39-067; MEM | Self-generated spill invariant defect stays internal. |
| CJ | §§26,32,39.3 | V39-057; MEM | Failed query leaves no runnable worker/task. |
| CK | §§11,39.3 | V39-057; TXN/MEM | Query cleanup does not release transaction locks. |
| CL | §§20.14.12,39.3 | V39-071; SEM | Scalar second final row is checked. |
| CM | §§20.14.12,39.3 | V39-071; SEM | Demanded child error is preserved. |
| CN | §§20.14.12,39.3 | V39-071; SEM | IN RHS build cannot be skipped by first-K. |
| CO | §§17.6,39.3.1 | V39-068; SEM | MIN_INT/-1 raises NUMERIC_OVERFLOW safely. |
| CP | §§17.6,39.3.1 | V39-068; SEM | Integer division by zero raises ArithmeticError. |
| CQ | §§17.6,39.3.1 | V39-068; SEM | Integer remainder by zero raises ArithmeticError. |
| CR | §§29.3,39.3.1 | V39-068; SEM | Exact SUM intermediate may leave result domain when final fits. |
| CS | §§29.3,39.3.1 | V39-068; SEM | COUNT final conversion overflow occurs at Finalize. |
| CT | §§17.6.2,39.3.2 | V39-069; SEM | Finite/±0 yields correctly signed infinity. |
| CU | §§17.6.2,39.3.2 | V39-069; SEM | ±0/±0 yields NaN. |
| CV | §§17.6.2,39.3.2 | V39-069; SEM | Infinity/infinity yields NaN. |
| CW | §§29.3,39.3.2 | V39-069; SEM | Exact-zero aggregate exposes canonical +0.0. |
| CX | §§38.4,39.4 | V39-070; PLAN | Malformed epsilon is OptimizerError before comparison. |
| CY | §§36,39.4 | V39-070; PLAN | Valid cost saturation is ordinary metadata. |
| CZ | §§38.21,39.4 | V39-070; PLAN | Configured bounded exhaustion is OptimizerResourceLimit. |
| DA | §§38.21,39.3 | V39-066/V39-070; PLAN | Guard-clear backing denial is OutOfMemory. |
| DB | §§36,39.4 | V39-070; PLAN | High legal cost is not a resource failure. |
| DC | §§22,38.24,39.4 | V39-070; PLAN | Invalid final plan is rejected before execution. |
| DD | §39.1.7 | V39-055/V39-058; TXN/MEM | Cleanup cannot erase original resource cause. |
| DE | §39.1.3 | V39-017; TXN | Background error cannot retroactively fail unrelated completed statement. |

## V39 acknowledgement and terminal-cleanup adversarial matrix — A through AN

| Case | Final owner | V39 procedure / reuse | Controlled event and independent expected observation |
|---|---|---|---|
| A | §§39.1.1,.5 | V39-007; TXN | Explicit COMMIT C6 delivered → S/COMMITTED. |
| B | §§39.1.1,.6 | V39-007/V39-043; TXN | Explicit ROLLBACK A4 delivered → S/ABORTED. |
| C | §39.1.1 | V39-007; TXN | Explicit statement success → S/ACTIVE. |
| D | §§15.5,39.1.1 | V39-007/V39-024; TXN | Autocommit S only after C4–C5 and prepared result publication. |
| E | §39.1.5 | V39-008; TXN | Persistent C6 loss → COMMITTED, CU+ConnectionFailure. |
| F | §39.1.5 | V39-008/V39-038; TXN | Read-only C6 loss has the same terminal/delivery result without WAL. |
| G | §§39.1.1,.6 | V39-009/V39-043; TXN | Explicit A4 loss → server S/ABORTED, no delivered success. |
| H | §§39.1.1,.6 | V39-010/V39-043; TXN | Automatic A4 loss retains original MA and chains transport. |
| I | §39.1.5 | V39-028; TXN | Commit append uncertainty → CU+NC/recovery. |
| J | §§39.1.5,.8 | V39-030/V39-053; TXN | Durable COMMIT plus cleanup failure remains COMMITTED. |
| K | §39.1.5 | V39-034/V39-039; TXN | Read-only C4 exact coherent nonpublication with impossible continuation requires mandatory ABORT, server-side `FAILED_TRANSACTION_MUST_ABORT`, ownership through A2/A3, ABORTED after cleanup, no COMMIT success, and no invented read-only COMMIT WAL; a separate safe-continuation fixture retains/retries. |
| L | §39.1.5 | V39-035; TXN | Failure after authoritative read-only C4 is C5-owned and remains COMMITTED. |
| M | §39.1.7 | V39-037/V39-045; TXN | Read-only disconnect before C4 cancels and ABORTS without WAL. |
| N | §39.1.7 | V39-036/V39-046; TXN | Disconnect during C4 requires three-way causal classification. |
| O | §39.1.7 | V39-035/V39-047; TXN | Read-only post-C4 disconnect remains COMMITTED and continues C5. |
| P | §39.1.7 | V39-047/V39-050; TXN | COMMITTED pre-C5 completion retains cleanup ownership. |
| Q | §§39.1.5,.7 | V39-008/V39-047; TXN | COMMITTED pre-C6 stays terminal and client is uncertain. |
| R | §§39.1.6,.7 | V39-042/V39-047; TXN | ABORTED pre-A3 completion continues cleanup. |
| S | §§39.1.6,.7 | V39-009/V39-047; TXN | ABORTED pre-A4 remains terminal; no fake response/CU. |
| T | §§15.5,39.1.7 | V39-037/V39-045; TXN | Persistent postappend disconnect cannot ABORT. |
| U | §§15.5,39.1.7 | V39-030/V39-045; TXN | Persistent post-C3 disconnect remains semantically COMMITTED. |
| V | §§15.6,39.1.7 | V39-045; TXN | ABORTING disconnect continues ABORT. |
| W | §§9.4,39.1.1 | V39-044; TXN | Failed transaction enters MUST_ABORT and rejects work/COMMIT. |
| X | §39.1.6 | V39-041/V39-044; TXN | Automatic ABORT pre-A2 failure retains noncommit and retries/NC. |
| Y | §39.1.6 | V39-042/V39-044; TXN | Automatic ABORT post-A2 failure remains ABORTED. |
| Z | §39.1.7 | V39-055; TXN | Original error remains primary; cleanup failure is chained. |
| AA | §39.1.7 | V39-055/V39-060; TXN | Original error remains while NC additionally governs database continuation. |
| AB | §§15.5,39.1.5 | V39-033; TXN | Read-only C4 has no commit_lsn/durable_lsn event. |
| AC | §§39.1.5,.7 | V39-008/V39-055; TXN | Acknowledgement loss cannot produce false ABORT. |
| AD | §§9.14,39.1.7–.8 | V39-047/V39-050; TXN | Connection closure cannot cause early lock release. |
| AE | §39.1.7 | V39-008–V39-010/V39-055; TXN | Transport status cannot overwrite COMMITTED/ABORTED. |
| AF | §§21.16.1,31,39.1.4 | V39-018; TXN/SEM | Ordinary row-five semantic error is prevented after W by closure. |
| AG | §39.1.4 | V39-019; TXN | Legal dynamic cancellation after W gives MA/ABORT. |
| AH | §§15.5,31.9,39.1.4 | V39-024; TXN | Autocommit RETURNING cannot escape before C4–C5. |
| AI | §39.1.5 | V39-030/V39-035; TXN | Persistent C4 failure retains C3 durability and does not adopt prepublication read-only outcome. |
| AJ | §39.1.5 | V39-033–V39-036; TXN | Read-only C4 never depends on persistent C3. |
| AK | §§9.15,39.1.5 | V39-033/V39-049; TXN | Read-only COMMIT gains no new persisted status. |
| AL | §39.1.1 | V39-007/V39-043; TXN | Successful ROLLBACK maps to S, not FA. |
| AM | §§39.1.1,.6 | V39-009; TXN | A4 transport failure is not COMMIT_OUTCOME_UNCERTAIN. |
| AN | §39.1.7 | V39-047/V39-050; TXN | Terminal transaction cannot abandon C5/A3 on disconnect. |

## V39 reuse inventory and static-integrity contract

SET A is the exact expansion of `SEM`, `TXN`, `MEM`, and `PLAN` wherever those keys appear
in V39. SET B, the declared external-ID inventory, is:

V20-12, V20-13, V20-14, V20-15, V20-16, V20-17, V20-24; V21-2, V21-3,
V21-13, V21-14, V21-23, V21-26; V22-K; V24-D, V24-H, V24-J, V24-K,
V24-L, V24-M; V25-H, V25-I, V25-J, V25-K, V25-M, V25-N, V25-O; V26-H,
V26-I, V26-J, V26-K, V26-L; V29-F, V29-I, V29-J, V29-K, V29-L, V29-M,
V29-Q; V31-A, V31-B, V31-C, V31-D, V31-E, V31-F, V31-G, V31-H, V31-I,
V31-J, V31-K, V31-L, V31-M, V31-N; V32-H, V32-I, V32-J, V32-K, V32-L,
V32-M; V33-E, V33-F; V34-D, V34-H, V34-I, V34-J; V36-C, V36-I; V37-I;
V38-052, V38-053, V38-054, V38-055, V38-056, V38-057, V38-058, V38-059,
V38-060, V38-061, V38-062, V38-063, V38-064, V38-065, V38-066, V38-067.

After exact range expansion, SET A minus SET B and SET B minus SET A must both be empty;
duplicates are forbidden and every ID must have exactly one live definition. The named
heading inventory is exactly the 12 headings declared under `V39 reuse keys`, each resolving
once to an applicable procedure body.

V39-078 derives membership and reference validity from the live procedure definitions. It fails if any essential evidence can be suppressed, any fault remains untriggered,
any optional capability becomes mandatory, or a verification procedure
introduces a transaction state, WAL record, persisted status, SQL semantic, retry/undo
policy, error category, command outcome, lock-release rule, acknowledgement guarantee,
optimizer policy, private tracing API, progress claim, or claimed test execution.
