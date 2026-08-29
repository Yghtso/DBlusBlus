# Pass C report

1. **Pass-C verdict: PASS C — TARGETED DOCUMENT FIXES RECOMMENDED**

2. **Primary scope read:** `docs/ARCHITECTURE.md` front matter, Chapters 1–2 as context, and Chapter 3 §§3.1–3.3.7 in full.

3. **Context-only architecture consulted:** §§4.7.1–4.7.8, 7.1–7.5, 7.12.5–7.12.7, 9.14, 12.10.5, 12.12.4, 13.2, 13.5, 13.11–13.19, 14.17.1, 15.5–15.6, 16.5, 16.9, 39.1, and 41.3.

4. **Other live documents consulted:** `AGENTS.md`; the purpose/platform material in `DEVELOPMENT.md`; the purpose/current-state boundary in `PROJECT_STATE.md`; and lifecycle procedures in `VERIFICATION.md`.

5. **BLOCKING:** 0
6. **MAJOR:** 1
7. **MINOR:** 3
8. **EDITORIAL:** 0

## 9. Section-by-section review

| Section | Architectural role | Timelessness | Ownership | State clarity | Ordering clarity | Failure clarity | Normative clarity | Terminology | Cross-refs | Analytical sufficiency | Semantic consistency | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| §3.1 Supported platform | Defines supported OS, CPU, process, and threading baseline | Noncanonical “initial/later” wording | Appropriate | N/A | Clear | N/A | Clear | Uses “initial architecture” instead of `v1` | None needed | Sufficient | Consistent with §4.7 | FINDING |
| §3.2 Implementation language | Defines C++20 architectural baseline and rationale | Mostly timeless | Appropriate | N/A | N/A | N/A | Revision exception is imprecise | Consistent otherwise | None | Sufficient | Consistent with Chapter 1 | FINDING |
| §3.3 Database process lifecycle | Owns system-level lifecycle orchestration | Timeless | Explicit canonical owner | Clear | Clear | Clear | Clear | Consistent | Correct | Strong | Consistent | CLEAN |
| §3.3.1 Runtime states and legal transitions | Defines lifecycle states and normative transitions | Timeless | Process-local database owner | One late-shutdown edge is ambiguous | Transition point into `CLOSING` not tied explicitly to shutdown steps | Safe behavior is stated elsewhere | Mostly strong | Consistent | Correct | Strong | Operational outcome consistent | FINDING |
| §3.3.2 Active process exclusivity | Defines root identity, owner lock, aliases, fork/exec, and release order | Timeless | Unambiguous | Clear | Precise | Precise | Strong | Consistent | Correct | Strong | Consistent with §4.7 | CLEAN |
| §3.3.3 Ordered open protocol | Defines 12-step open/recovery entry | Timeless | Lifecycle owner coordinates detailed owners | Clear | Exact | Exact through general cleanup classification | Strong | Consistent | Precise | Strong | Consistent with Chapter 13 | CLEAN |
| §3.3.4 READY publication and failed-open cleanup | Defines READY gate and failed-open unwinding | Timeless | Clear | Clear | Clear | “any failure” overstates clean unwinding | One local contradiction | Consistent | Correct | Strong | Intended outcome consistent | FINDING |
| §3.3.5 Database-noncontinuable behavior | Defines admission closure and non-clean teardown | Timeless | Lifecycle owner | Clear | Clear | Clear | Strong | Consistent | Correct | Strong | Consistent with §§12.12.4 and 39.1 | CLEAN |
| §3.3.6 Controlled shutdown protocol | Defines eight-step shutdown order | Timeless runtime ordering | Lifecycle owner | Operational behavior clear; state-table edge ambiguous | Exact | Exact | Strong | Consistent | Precise | Strong | Consistent with subsystem owners | FINDING |
| §3.3.7 Create, removal, crash, and lifecycle errors | Defines cross-operation outcomes and crash cases | Timeless | Clear | Clear | Sufficient | Sufficient, with removal intentionally summarized | Strong | Generic/family-specific errors remain distinguishable | Correct | Sufficient | Consistent | CLEAN WITH NOTE |

## Core assessments

10. **Platform assumptions:** Linux, x86-64/ARM64, POSIX file APIs, exclusive database ownership, and multithreading agree with the detailed file-locking and durability contracts. The only issue is noncanonical version/roadmap wording.

11. **C++ baseline:** C++20 matches Chapter 1. No compiler versions, flags, IDE requirements, or local machine evidence are present. The exception for compiler extensions should refer explicitly to an architecture revision.

12. **DatabaseInstance concept:** Chapter 3 deliberately calls it the “process-local database owner” rather than prescribing a `DatabaseInstance` class. It owns lifecycle state, admission, subsystem construction, shutdown orchestration, and final lock release.

13. **Lifecycle authority:** Unambiguous. DiskManager, BufferPool, WAL, recovery, TransactionManager, and catalog services own their local protocols; the process-local database owner coordinates their lifecycle.

14. **Lifecycle states:** `CLOSED`, `OPENING`, `RECOVERING`, `READY`, `DRAINING`, `CLOSING`, and `NONCONTINUABLE`. `OPEN_FAILED` and `SHUTDOWN_FAILED` are results, not states.

15. **State names:** Consistent throughout the architecture. Transaction states such as `ACTIVE` and `COMMITTING` remain a separate state machine.

16. **Transition completeness:** All operational outcomes are defined, but the exact `DRAINING -> CLOSING` point and the corresponding late-durability failure edge need clarification.

17. **Illegal transitions:** Structurally excluded. Only `READY` admits ordinary work; neither `DRAINING`, `CLOSING`, nor `NONCONTINUABLE` can readmit it.

18. **Ownership/exclusivity:** One process-local owner and one process-associated POSIX owner lock per actual database root.

19. **Database identity/aliases:** Stable identity of the opened root/control inode—not path spelling—controls same-process ownership. No-follow, retained directory descriptors, and the OS lock cover cross-process aliases.

20. **Competing-open ordering:** Correct. Only root/control opening and type checks precede lock acquisition; control-slot selection, WAL inventory, recovery, and mutation are forbidden before success.

21. **Same-process ownership:** A synchronized registry rejects a second owner with `DATABASE_BUSY`; handle coalescing is not supported.

22. **Fork/exec:** Copied handles in a fork child are unsupported; the child must discard them or `exec`. Record-lock ownership is not inherited, and close-on-exec prevents successful-exec descriptor inheritance.

23. **Owner-lock failures:** Contention is distinguished from inability to establish reliable locking. The latter is an I/O/platform failure, not `DATABASE_BUSY`.

24. **DATABASE_BUSY:** Nonwaiting, returned before database inspection or mutation, and retryable only after the existing owner releases exclusivity.

25. **Open preconditions:** Exact final root, safe object types, `database.control`, complete bootstrap, reliable locking, valid control/recovery inputs.

26. **Ordered open:** Complete 12-step protocol with recovery-private services and one READY publication.

27. **Recovery entry:** Occurs only after exclusive ownership and sufficient bootstrap/control/WAL validation.

28. **Pre-READY services:** Recovery-scoped DiskManager/file registry, WAL, BufferPool/page reconstruction, checkpoint, status, and catalog-bootstrap services. Normal background services remain stopped or gated.

29. **READY publication:** Atomic only after recovery, status repair, catalog reconstruction, required-file validation, checkpoint installation, orphan classification, and coherent runtime-service construction.

30. **Admission boundary:** Exactly `RECOVERING -> READY`; no subsystem can admit work independently.

31. **Failed-open cleanup:** Known, fully quiesceable failures unwind to `CLOSED`; uncertain ownership or incomplete quiescence enters `NONCONTINUABLE`. One sentence overstates the former as applying to “any failure.”

32. **Missing/corrupt durable components:** Required bootstrap/catalog-owned files prevent READY. Unowned managed files are orphan input; unknown names remain untouched.

33. **Control-slot fallback:** Independent validation, highest usable generation, legal older-generation fallback only with a complete retained recovery range, and no fallback across unsupported future format dispatch.

34. **Opened-database invariants:** All required ownership, recovery, metadata, file, service, and admission invariants are established before READY.

35. **READY invariants:** Distributed across clear owners but atomically published by the lifecycle owner.

36. **Runtime failures:** Statement-local, transaction-fatal, database-noncontinuable, and process-crash scopes remain distinct.

37. **NONCONTINUABLE:** Closes admission, forbids ordinary storage activity, preserves protected uncertain state, permits only safe quiescence/non-clean teardown, and requires fresh recovery.

38. **NONCONTINUABLE versus corruption:** Correctly distinguished. It means the running owner cannot continue safely; it does not by itself assert permanent on-disk corruption.

39. **NONCONTINUABLE entries:** Consistent with storage uncertainty, post-durable publication failures, abort/cleanup ownership failures, shutdown failures, and required catalog corruption discovered after READY.

40. **Durable COMMIT:** Once `TXN_COMMIT` is durable, no open, shutdown, cache, cleanup, transport, or recovery failure can convert it to ABORTED.

41. **Controlled shutdown:** Eight ordered steps stop admission, settle transactions, stop producers, drain BufferPool, install a checkpoint, finish namespace work, stop WAL last, then release ownership.

42. **DRAINING:** Immediately rejects new work. ACTIVE/MUST_ABORT work is canceled and aborted; already-terminalizing transactions finish.

43. **Transaction-state shutdown:** Consistent with §§9.14, 15.5, 15.6, and 39.1.

44. **In-flight COMMIT:** New COMMIT requests are rejected; already `COMMITTING` work finishes, and post-append COMMIT is uncancellable.

45. **In-flight ABORT:** Already `ABORTING` work finishes; ACTIVE/MUST_ABORT transactions are driven through ABORT.

46. **BufferPool drain:** Occurs after transaction/producer quiescence and before final checkpoint or WAL shutdown. Dirty required pages cannot be discarded.

47. **Checkpoint position:** After an empty DPT and no active writer; before namespace completion and WAL-service shutdown.

48. **WAL shutdown:** WAL durability outlives terminal protocols, page flushes, checkpoint/control publication, and optional recycling.

49. **Service teardown:** BufferPool helpers stop after drain; WAL stops only after no remaining consumer; managers/descriptors close while retaining the lock descriptor.

50. **Owner-lock release:** Last externally visible teardown action, followed by process-claim removal and `CLOSED` publication.

51. **Shutdown failure:** Never reports success, enters `NONCONTINUABLE`, preserves durable facts, and retains ownership until safe teardown or process exit.

52. **Lifecycle outcomes:** Busy, non-database, unsupported, corrupt, recovery, I/O/durability, noncontinuable, and shutdown failures remain distinct.

53. **Section ownership:** §3.3.3 owns open order; §3.3.6 owns shutdown order; §3.3.7 owns create/removal/crash/error outcomes. No protocol is independently redefined in §3.3.7.

54. **Create lifecycle:** Durable staging-root publication precedes success. An opened result still passes ordinary ownership/recovery/READY gates.

55. **Removal lifecycle:** Offline-only, requires `CLOSED` plus the same exclusive control lock, and delegates namespace durability to §4.7. This is concise but not contradictory.

56. **Orphans:** Exact managed names proven unowned may be cleaned; unknown names cannot be guessed or deleted.

57. **Error categories:** Consistent. `UNSUPPORTED_FORMAT` is the generic family while open paths preserve specific `UNSUPPORTED_DATABASE_FORMAT` and `UNSUPPORTED_CATALOG_SCHEMA` results.

58. **Error locality:** Clear across statement, transaction, database-owner, durable database, and process scopes.

59. **Crash/reopen:** Process or machine crash releases OS ownership; every subsequent open reacquires exclusivity and performs full recovery.

60. **One-owner invariant:** Preserved across open, create-with-handle, controlled close, non-clean close, crash, and offline removal.

## 61. Resource ownership table

| Resource/service | Created/opened by | Authoritative owner | States | Released by | Kind | Failure consequence | Status |
|---|---|---|---|---|---|---|---|
| Root/control descriptors | Open lifecycle | Database owner | OPENING through CLOSING/NC | Failed-open or close teardown | Runtime | CLOSED if clean; NC if unsafe | Clear |
| Process-local root claim | Open lifecycle | Database owner registry | OPENING through final close | After OS-lock release | Runtime | Retained until safe release | Clear |
| POSIX control lock | Open/removal lifecycle | OS process owner coordinated by database owner | OPENING through CLOSING/NC | Final teardown/process death | Runtime OS state | Busy or retained NC ownership | Clear |
| Recovery DiskManager/file registry | Open step 6 | Recovery owner under database owner | RECOVERING | Upgrade/normalization or cleanup | Runtime | CLOSED or NC | Clear |
| WAL reader/manager | Open step 6 | WAL subsystem | RECOVERING through CLOSING | Shutdown step 7 | Runtime plus persisted WAL | Open/recovery failure or NC | Clear |
| BufferPool | Open step 6/11 | BufferPool subsystem | RECOVERING through CLOSING | Failed-open or step 7 | Runtime cache | Failure reported to lifecycle owner | Clear |
| Transaction-status service | Open step 6/11 | Transaction/status subsystem | RECOVERING through CLOSING | Teardown | Persisted plus runtime cache | Prevents READY or causes NC | Clear |
| Catalog bootstrap/cache | Open step 6/11 | Catalog subsystem | RECOVERING through CLOSING | Teardown | Persisted descriptors plus runtime cache | Prevents READY or causes NC | Clear |
| Transaction/lock services | Open step 11 | TransactionManager/lock owners | READY through drain | After terminal quiescence | Runtime | NC if ownership cannot be reconciled | Clear |
| Background workers | Constructed gated in step 11 | Owning subsystem, lifecycle admission gate | READY; stopped in DRAINING | Shutdown step 3/7 | Runtime | Failed stop may cause NC | Clear |

## 62. Lifecycle state table

| State | Entry | New admission? | Existing transaction work | Durable mutation | Exits | Error exits | Lock |
|---|---|---:|---|---|---|---|---|
| CLOSED | No process owner/resources | No | None | None by an owner | OPENING | — | No |
| OPENING | Final root opened; claim/lock acquisition begins | No | None | Validation/bootstrap opening only | RECOVERING, CLOSED | NC | Acquiring/held |
| RECOVERING | Lock held; recovery services established | No | Recovery only | Recovery-authorized | READY, CLOSED | NC | Held |
| READY | All gates atomically published | Yes | Ordinary work | Yes | DRAINING | NC | Held |
| DRAINING | Close wins admission gate | No | Cancellation and terminal completion | Required terminal/shutdown work | CLOSING | NC | Held |
| CLOSING | Ordinary work gone; durability/teardown | No | None | Final durability only | CLOSED | NC | Held until final step |
| NONCONTINUABLE | Fatal uncertainty/invariant failure | No | Safe failure propagation/quiescence only | No ordinary writes/checkpoint | CLOSING | May remain NC | Held |

The location of the `DRAINING -> CLOSING` transition within shutdown steps 3–4 is the subject of C-M1.

## 63. Ordered open protocol

| Step | Action/owner | Persistent side effects? | Failure/cleanup | Next |
|---:|---|---:|---|---|
| 1 | Lifecycle owner opens and identifies final root | No | Reject/close safely | OPENING |
| 2 | Claim registry identity; open/control-lock | No DB mutation | Busy releases claim/descriptor; other failures unwind | OPENING |
| 3 | Retain managed directory descriptors; inventory names | No | Clean unwind or NC if ownership uncertain | OPENING |
| 4 | Validate control/bootstrap/status/system roots privately | No ordinary publication | Specific validation error; cleanup | OPENING |
| 5 | Validate WAL inventory/checkpoint/recovery range | No | Recovery/corruption failure; cleanup | OPENING |
| 6 | Construct recovery-scoped services | Runtime only | Destroy cleanly or NC | RECOVERING |
| 7 | Reconcile WAL tail; analysis/initial redo | Yes, recovery-authorized | Known failure cleanup or NC | RECOVERING |
| 8 | Loser/status/catalog reconstruction; deferred redo | Yes | Known failure cleanup or NC | RECOVERING |
| 9 | Install durable recovery checkpoint/control slot | Yes | Prevent READY; cleanup or NC | RECOVERING |
| 10 | Classify managed orphans/retired objects | Optional durable unlink | Classified orphan cleanup may remain queued | RECOVERING |
| 11 | Enable status lookup; build normal services/caches behind gate | Runtime | Cleanup or NC | RECOVERING |
| 12 | Atomic READY/admission/background publication | Runtime publication | Publication uncertainty is NC | READY |

## 64. Controlled shutdown protocol

| Step | Action | Admission/transaction condition | Durability/service dependency | Failure |
|---:|---|---|---|---|
| 1 | Publish DRAINING | New work rejected | Lock held | NC on invariant failure |
| 2 | Cancel statements; ABORT ACTIVE/MUST_ABORT; finish COMMITTING/ABORTING | Terminal publication required | WAL/BufferPool alive | NC if terminal ownership cannot complete |
| 3 | Stop maintenance/query/checkpoint producers | No admitted producer work remains | WAL/writeback remain alive | NC on failed quiescence |
| 4 | Quiesce/drain BufferPool and flush required dirty pages | No new external page work | WAL-before-data | NC on drain/flush failure |
| 5 | Install final checkpoint | No active writer; empty DPT | WAL/control durability | NC on failure |
| 6 | Finish owned namespace cleanup/recycling | Unknown names untouched | Directory durability; WAL alive if recycling | NC on required sync uncertainty |
| 7 | Stop BufferPool helpers, then WAL; destroy managers | No remaining WAL consumer | Lock descriptor retained | NC if teardown unsafe |
| 8 | Release lock, remove claim, publish CLOSED | All prior steps complete | Final ownership publication | Success only here |

## 65. Lifecycle failure matrix

| Failure | Instance outcome | Durable DB semantics | Lock | Retry/reopen | Owner | Ambiguous? |
|---|---|---|---|---|---|---:|
| Competing lock | CLOSED/failed open | Untouched | Not acquired | Retry after owner release | §3.3.2 | No |
| Unsupported lock service | CLOSED/failed open | Untouched | Not acquired | Retry on supported service | §3.3.3 | No |
| Known open validation/recovery failure | CLOSED after cleanup | Valid recovery writes remain | Released last | Fresh open may retry | §§3.3.3–3.3.4 | No |
| Uncertain open mutation/cleanup | NONCONTINUABLE | Preserve valid durable state | Retained | After non-clean teardown/process exit | §3.3.4 | No |
| Classified orphan unlink failure | READY may still publish | Orphan remains inaccessible | Held normally | Cleanup retried | §§3.3.4, 4.7 | No |
| Fatal READY storage failure | NONCONTINUABLE | Durable facts preserved | Retained | Fresh recovery after release | §§12.12.4, 3.3.5 | No |
| Shutdown durability failure | NONCONTINUABLE | Last valid WAL/control/data remain | Released only after safe teardown | Full recovery | §3.3.6 | State edge only |
| Guard/worker cannot drain | NONCONTINUABLE | No clean claim | Retained | Teardown retry/process exit | §§3.3.5–3.3.6 | No |
| Process/machine crash | Runtime state gone | Persisted prefix governs | OS releases | Full recovery | §3.3.7 | No |
| Create crash/failure | No valid published DB or validated durable final root | Staging/final survivors classified by §4.7.8 | Creator lock/process ends | Recreate/validate | §4.7.8 | No |
| Offline removal failure | No open instance | Acknowledged namespace deletions remain durable | Held during operation | External retry/validation | §§3.3.7, 4.7 | No safety ambiguity |

## 66. NONCONTINUABLE matrix

| Trigger | New txn? | Existing txn? | COMMIT | ABORT/cleanup | Shutdown | Reopen | Source |
|---|---:|---|---|---|---|---|---|
| WAL append/restoration/publication uncertainty | No | No ordinary continuation | Recovery decides nondurable outcome; durable remains committed | Safe volatile cleanup only | Non-clean | Full recovery | §§12.12.4, 3.3.5 |
| Post-durable COMMIT publication/cleanup failure | No | Transaction already COMMITTED | Never reverted; no new success ack before C6 | Coherent cleanup where possible | Non-clean | Full recovery | §§39.1.5, 3.3.5 |
| ABORT publication/ownership failure | No | No ordinary continuation | Commit remains illegal | Preserve noncommit facts; recovery loser handling | Non-clean | Full recovery | §39.1.6 |
| Required catalog corruption after READY | No | Fail/close connections | Existing durable commits preserved | Non-clean | Non-clean | Repair/recovery required | §16.5.10 |
| Shutdown durability/drain failure | No | Terminal outcomes already established or recovery-resolved | Durable commits survive | Safe teardown only | Failed/non-clean | Full recovery | §3.3.6 |
| Failed-open uncertain cleanup | No | None admitted | N/A | Safe teardown only | Non-clean | Full recovery after release | §3.3.4 |

## 67. Durable-COMMIT matrix

| Situation | Durable point reached? | Transaction outcome | Instance consequence | Recovery | Consistent? |
|---|---:|---|---|---|---:|
| Before valid commit append | No | ABORT/MUST_ABORT possible | Healthy or local failure | Loser | Yes |
| Commit append uncertain/not durable | Unknown | Recovery decides | Often NC | Valid WAL prefix decides | Yes |
| C3 complete | Yes | COMMITTED forever | Continue or NC on later failure | COMMITTED | Yes |
| Shutdown sees already COMMITTING | Possibly | Terminal protocol finishes | DRAINING waits | WAL evidence authoritative | Yes |
| Post-C3 C4/C5 failure | Yes | COMMITTED | NONCONTINUABLE | COMMITTED | Yes |
| C6 delivered | Yes | COMMITTED and acknowledged | May remain healthy | COMMITTED | Yes |
| Shutdown failure after commit | Yes | COMMITTED | NONCONTINUABLE/non-clean | COMMITTED | Yes |
| Crash after durable commit | Yes | COMMITTED | Runtime lost | Recovery restores COMMITTED | Yes |

## 68. Ownership/alias matrix

| Case | Same DB identity? | Competing owner? | Result | Mutation before result? | Lock behavior |
|---|---:|---:|---|---:|---|
| Same textual path, same process | Yes | Yes | DATABASE_BUSY | No DB inspection/mutation | Registry rejects |
| Path alias to same root/control inode, same process | Yes | Yes | DATABASE_BUSY | No | Registry identity rejects |
| Same/alias root, different process | Yes | Yes | DATABASE_BUSY | No | POSIX lock conflicts |
| Fork child using copied handle | Copied runtime object | Unsupported | Must discard/close or exec | No DB APIs allowed | Child does not own parent lock |
| Fork child independently opens DB | Yes | Yes | DATABASE_BUSY while parent owns | No | Parent lock conflicts |
| Successful exec | N/A | N/A | No inherited owner handle | No | Close-on-exec |
| Fresh opener after final release | Yes | No | Full ordinary open/recovery | Only after acquiring lock | Acquires new lock |

## 69. Terminology

| Term | Chapter 3 meaning | Canonical owner | Consistent? | Notes |
|---|---|---|---:|---|
| Process-local database owner | Runtime orchestration authority for one root | §3.3 | Yes | Conceptual DatabaseInstance; no class name prescribed |
| Database root | Persistent namespace directory | §4.7.1 | Yes | Path spelling is not identity |
| Owner lock | Process-associated `fcntl` control-file lock | §3.3.2 | Yes | Not persisted bytes |
| Process-local claim | Same-process alias/duplicate exclusion | §3.3.2 | Yes | Released after OS lock |
| READY | Atomic ordinary-admission state | §§3.3.1, 3.3.4 | Yes | Not merely successful object construction |
| DRAINING | Admission closed; terminal/cancellation work remains | §§3.3.1, 3.3.6 | Yes | Transition into CLOSING needs precision |
| CLOSING | Final durability/resource teardown | §3.3.1 | Mostly | Late failure edge needs precision |
| NONCONTINUABLE | Running database owner cannot continue safely | §§3.3.5, 12.12.4 | Yes | Not synonymous with permanent corruption |
| Recovery | Exclusive pre-READY repair/reconstruction | Chapter 13 | Yes | Not ordinary query execution |
| DATABASE_BUSY | Live competing owner | §§3.3.2, 3.3.7 | Yes | Distinct from lock-service failure |
| Crash | Process/machine loss of runtime state | §3.3.7 | Yes | Distinct from controlled shutdown |
| Clean shutdown marker | Explicitly nonexistent | §§3.3.3, 3.3.6 | Yes | Every open recovers |

## 70. Cross-reference audit

Repeated references serving the same sentence/protocol step are grouped, but every Chapter 3 target is represented.

| Source | Target | Purpose | Exists/owner correct? | Precise? | Status |
|---|---|---|---:|---:|---|
| §3.3 | §§12.12.4, 39.1 | Source of noncontinuable gate | Yes | Yes | CLEAN |
| §3.3.1 | §§3.3.4, 3.3.6 | READY/DRAINING admitted work | Yes | Yes | CLEAN |
| §3.3.1 | §§13.11–13.19 | Recovery completion | Yes | Yes | CLEAN |
| §3.3.1 | §§12.12.4, 39.1 | READY fatal transition | Yes | Yes | CLEAN |
| §3.3.2 | §4.7 | No-follow namespace ownership | Yes | Yes | CLEAN |
| §3.3.3 | §§4.7, 4.7.6 | Root validation and orphan classification | Yes | Yes | CLEAN |
| §3.3.3 | §3.3.2 | Exclusive lock | Yes | Yes | CLEAN |
| §3.3.3 | §§13.2, 13.2.3 | Control slots | Yes | Yes | CLEAN |
| §3.3.3 | §16.9 | Immutable catalog bootstrap | Yes | Yes | CLEAN |
| §3.3.3 | §§13.11–13.19, 13.13.1 | Recovery and deferred redo | Yes | Yes | CLEAN |
| §3.3.3 | §§16.5, 16.5.9–16.5.10 | Catalog fixed-point validation | Yes | Yes | CLEAN |
| §3.3.3 | §§12.10.5, 13.13.2, 13.17 | Torn status repair | Yes | Yes | CLEAN |
| §3.3.3 | §§12.2, 13.11 | WAL naming and valid prefix | Yes | Yes | CLEAN |
| §3.3.4 | §§4.7.2, 4.7 | Namespace reconciliation/cleanup | Yes | Yes | CLEAN |
| §3.3.5 | §§12.12.4, 39.1 | Noncontinuable triggers | Yes | Yes | CLEAN |
| §3.3.5 | §15.5 | COMMIT acknowledgement boundary | Yes | Yes | CLEAN |
| §3.3.5 | §3.3.3 | Fresh recovery after teardown | Yes | Yes | CLEAN |
| §3.3.6 | §§15.6, 39.1 | Transaction drain | Yes | Yes | CLEAN |
| §3.3.6 | §14.17.1 | Maintenance shutdown | Yes | Yes | CLEAN |
| §3.3.6 | §7.12.5 | Retired-frame discard | Yes | Yes | CLEAN |
| §3.3.6 | §13.5 | Final checkpoint | Yes | Yes | CLEAN |
| §3.3.6 | §4.7 | Namespace durability | Yes | Yes | CLEAN |
| §3.3.6 | §§9.14, 15.5, 15.6 | Terminal publication/order | Yes | Yes | CLEAN |
| §3.3.7 | §4.7.8 | Database creation publication | Yes | Yes | CLEAN |
| §3.3.7 | §§3.3.2–3.3.4 | Opened-create gates | Yes | Yes | CLEAN |
| §3.3.7 | §4.7 | Removal/crash namespace outcomes | Yes | Yes | CLEAN |
| §3.3.7 | §§12.10.5, 13.13.2, 13.17 | Forbidden torn-status trust | Yes | Yes | CLEAN |

## 71. Normative-language audit

| Section | Requirement | Strength | Detailed consistency | Ambiguous? | Finding? |
|---|---|---|---:|---:|---:|
| §3.1 | Linux/POSIX/exclusive process baseline | Descriptive contract | Yes | Version label only | C-m1 |
| §3.2 | Implementation MUST use C++20 | MUST | Yes | No | No |
| §3.2 | Extensions excluded unless later decision | Baseline exception | Yes in intent | Revision authority vague | C-m2 |
| §3.3.1 | Exactly one lifecycle state | Normative descriptive | Yes | Late shutdown boundary | C-M1 |
| §3.3.1 | Only READY admits work | MUST-level effect | Yes | No | No |
| §3.3.2 | Lock before inspection/recovery | MUST NOT | Yes | No | No |
| §3.3.3 | Every open performs recovery | Normative | Yes | No | No |
| §3.3.4 | READY only after complete prerequisite set | Normative | Yes | No | No |
| §3.3.4 | “any failure” fully unwinds | Descriptive universal | Conflicts with next paragraph | Yes | C-m3 |
| §3.3.5 | No ordinary activity after NC | Normative | Yes | No | No |
| §3.3.6 | Exact shutdown order | Normative | Yes | State edge only | C-M1 |
| §3.3.6 | Close MUST NOT succeed after listed failures | MUST NOT | Yes | No | No |
| §3.3.7 | Lock/removal/crash rules | Normative | Yes | No | No |

72. **Temporal/ordering language:** `before`, `after`, `first`, `then`, `once`, `during`, `while`, and `until` overwhelmingly express required runtime ordering. “Later open,” “next owner,” and “future open” describe lifecycle retries. The §3.1 “initial/later experimentation/initial architecture” wording is the only project-scope ambiguity. The §3.2 “later explicit decision” is architecture-evolution language but lacks precise revision terminology.

73. **Implementation-layout coupling:** None. `database.control`, `catalog.dat`, `txn_status.dat`, `pending/`, and `wal/` are persistent namespace contracts, not source-layout coupling.

74. **Development-sequencing ownership:** No implementation phase/order appears in lifecycle protocols. §3.1’s “later experimentation” wording is the sole roadmap-like phrase.

75. **Verification ownership:** Chapter 3 defines protocol boundaries and outcomes, not fault-injection mechanics. Detailed procedures remain in `VERIFICATION.md`.

76. **Duplication:** Acceptable lifecycle orchestration. Chapter 3 does not independently redefine WAL, BufferPool, transaction, or recovery local protocols.

77. **Analytical depth:** Strong. The lock-before-inspection rule, READY gate, DRAINING, WAL lifetime, lock-release order, and NONCONTINUABLE rationale are sufficiently explained.

## 78. High-priority consistency matrix

| # | Relationship | Result |
|---:|---|---|
| 1 | Database owner | CONSISTENT |
| 2 | Exclusive owner lock | CONSISTENT |
| 3 | Alias/same-database competing open | CONSISTENT |
| 4 | Early DATABASE_BUSY | CONSISTENT |
| 5 | Process-crash lock release | CONSISTENT |
| 6 | Fork/exec | CONSISTENT |
| 7 | Ordered open protocol | CONSISTENT |
| 8 | Recovery entry | CONSISTENT |
| 9 | READY admission | CONSISTENT |
| 10 | Failed-open cleanup | FINDING — wording only |
| 11 | Missing/corrupt durable component handling | CONSISTENT |
| 12 | Control-slot fallback | CONSISTENT |
| 13 | Transaction admission | CONSISTENT |
| 14 | DRAINING | CONSISTENT |
| 15 | Active transaction shutdown | CONSISTENT |
| 16 | COMMIT during shutdown | CONSISTENT |
| 17 | ABORT during shutdown | CONSISTENT |
| 18 | BufferPool drain | CONSISTENT |
| 19 | Final checkpoint | CONSISTENT |
| 20 | WAL shutdown ordering | CONSISTENT |
| 21 | Owner-lock release | CONSISTENT |
| 22 | Shutdown failure | FINDING — normative state-table edge |
| 23 | NONCONTINUABLE | CONSISTENT |
| 24 | Durable COMMIT survival | CONSISTENT |
| 25 | Create failure/orphans | CONSISTENT |
| 26 | Removal lifecycle/outcomes | CONSISTENT BUT SIMPLIFIED |

## Findings

79. **Complete BLOCKING findings:** None.

80. **Complete MAJOR findings:**

### C-M1 — Late shutdown state transition is not fully represented

- **Section:** §§3.3.1 and 3.3.6.
- **Evidence:** [§3.3.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:333) defines `CLOSING` as executing “final durability and/or resource teardown.” The transition table provides `DRAINING -> NONCONTINUABLE` for “shutdown invariant/durability failure,” but `CLOSING -> NONCONTINUABLE` only for a live guard/worker/ownership invariant. [§3.3.6](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:609) requires WAL/page/checkpoint/control/directory failures to enter `NONCONTINUABLE`.
- **Severity:** MAJOR.
- **Type:** LIFECYCLE STATE.
- **Scope:** Cross-section.
- **Explanation:** The operational response is clear, but the protocol never identifies exactly where steps 3–4 transition from DRAINING to CLOSING, and the normative table lacks an explicit CLOSING durability-failure edge.
- **Canonical comparison:** §3.3.6 and §7.12.6.
- **Consequence:** Implementations and diagnostics can disagree about the state occupied during final flush/checkpoint/namespace failures, despite agreeing that admission is closed and the result is noncontinuable.
- **Future action:** **D. STATE/OWNERSHIP CLARIFICATION.**

81. **Complete MINOR findings:**

### C-m1 — Noncanonical platform-scope temporality

- **Section:** §3.1.
- **Evidence:** [lines 282 and 292](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:282): “initial supported environment,” “later experimentation,” and “requirements of the initial architecture.”
- **Severity:** MINOR.
- **Type:** TEMPORALITY.
- **Scope:** Local.
- **Explanation:** The durable contract is the v1 platform baseline. “Initial/later” can be read as project sequencing rather than v1 scope/evolution freedom.
- **Canonical comparison:** Front matter and Chapter 1’s canonical `v1` terminology.
- **Consequence:** No semantic ambiguity, but avoidable roadmap interpretation.
- **Future action:** **A. LOCAL WORDING FIX.**

### C-m2 — Compiler-extension exception does not name revision authority

- **Section:** §3.2.
- **Evidence:** [line 298](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:298): “unless a later explicit decision introduces one.”
- **Severity:** MINOR.
- **Type:** NORMATIVE CLARITY.
- **Scope:** Cross-section.
- **Explanation:** Front matter requires an explicit architecture revision for accepted requirement changes; “explicit decision” is less precise.
- **Canonical comparison:** Architecture contract-language revision rule.
- **Consequence:** A reader could mistake an implementation-local decision for authority to add compiler extensions.
- **Future action:** **A. LOCAL WORDING FIX.**

### C-m3 — “Any failure” overstates failed-open unwinding

- **Section:** §3.3.4.
- **Evidence:** [lines 532–544](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:532): “any failure” releases the lock and claim, immediately followed by the rule that uncertain outcomes or unquiesced workers enter `NONCONTINUABLE` and retain the lock.
- **Severity:** MINOR.
- **Type:** FAILURE SEMANTICS.
- **Scope:** Local.
- **Explanation:** The next paragraph and state table establish the intended cleanable-versus-uncertain distinction, but the universal phrase is literally too broad.
- **Canonical comparison:** §§3.3.1, 3.3.5, and 12.12.4.
- **Consequence:** Reading only the first paragraph could encourage unsafe lock release while process-local users remain.
- **Future action:** **F. FAILURE-SEMANTICS CLARIFICATION.**

82. **Complete EDITORIAL findings:** None.

83. **Frozen architecture semantic questions:** None. The intended safe outcomes are already established; the findings can be resolved as document clarification without choosing new lifecycle semantics.

84. **Out-of-scope observations:**
    **KNOWN OUT-OF-SCOPE OBSERVATION FOR CHAPTER 7 REVIEW:** [§7.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:4342) still says “once the buffer layer exists.” It was not modified or deeply reviewed.

## Direct conclusions

85. **Ambiguous lifecycle owner:** No. The process-local database owner is authoritative.

86. **Unspecified correctness-relevant transition:** No unsafe behavioral outcome is unspecified. C-M1 leaves the state-label boundary and one transition-table edge ambiguous.

87. **Undefined open-step failure outcome:** No. Known cleanable failures unwind; uncertain ownership/publication/cleanup enters NONCONTINUABLE.

88. **Undefined shutdown-step failure outcome:** No. Every listed durability, invariant, and drain failure enters NONCONTINUABLE, although C-M1 affects its state-table representation.

89. **Can a competing opener mutate before ownership failure?** No.

90. **Can READY occur before required recovery/open work?** No.

91. **Does DRAINING admit new user transactions?** No.

92. **Are NONCONTINUABLE semantics complete?** Yes.

93. **Can durable COMMIT be reverted by later local failure?** No.

94. **Is owner-lock release ordering safe?** Yes; it is last.

95. **Are process-crash/reopen semantics complete?** Yes.

96. **Chapter 2 regression:** Passed. The coordination-flow label, resolved-metadata boundary, provider-to-consumer storage stack, and timeless BufferPool rule remain intact.

97. **Known §7.5 observation:** Unchanged and retained for Chapter 7’s own review.

98. **Recommended next action:** Targeted document fixes before the next slice.

99. **Recommended Pass-D scope:** Chapter 4 §§4.1–4.7.8, covering persistent foundations, identifier/exhaustion contracts, page/file identity, and namespace lifecycle.

100. **Files modified by this audit:** NONE.

101. **Initial Git state:** untracked external/user directory
`docs/reviews/2026-08-26_docs_separation_of_concerns/postFINAL-ARCHITECTURE.md/Chapter 3/`; empty index; HEAD `5731dc060dca245ac281e361832023be478c8239`.

102. **Final Git state:** identical to initial state; index empty; HEAD unchanged.

103. **`git diff --check`:** Passed.

104. **Repository state changed during audit:** No.

105. **Changes caused by this audit:** None.