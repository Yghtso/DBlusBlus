# Chapter 9 architecture review

## 1–9. Verdict, scope, and finding counts

**Verdict: CHAPTER 9 — TARGETED DOCUMENT FIXES RECOMMENDED**

Chapter 9 is technically coherent. No frozen semantic decision is required, and no correctness-relevant transaction policy is left to implementer invention. The verdict is driven by one implementation-sequencing leak and one cross-reference precision issue.

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 0 |
| MINOR | 1 |
| EDITORIAL | 1 |

Primary scope read: [docs/ARCHITECTURE.md §9](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6712) through the line before Chapter 10.

Context consulted:

- Chapters 3–5, 7–8
- Chapters 10–15
- §39.1
- §41.3
- Appendix C v1-deferred scope

Other live documents consulted:

- `AGENTS.md`
- `docs/VERIFICATION.md`
- `docs/PROJECT_STATE.md`
- `docs/DEVELOPMENT.md`

No source or tests were audited.

## 3. Actual Chapter-9 heading inventory

There are 22 Chapter-9 subsection headings, excluding the Chapter-9 heading itself.

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 9.1 | Scope and subsystem coordination | Chapter boundary and collaborating owners | ARCHITECTURE-APPROPRIATE |
| 9.2 | Transaction identifiers | TxnId domain, sentinels, nonreuse | ARCHITECTURE-APPROPRIATE |
| 9.3 | Durable transaction-ID reservation | Fixed-block durable allocation | ARCHITECTURE-APPROPRIATE |
| 9.4 | Transaction object and lifecycle state | Runtime states and legal transitions | ARCHITECTURE-APPROPRIATE |
| 9.5 | Isolation levels | Supported v1 isolation identities | ARCHITECTURE-APPROPRIATE |
| 9.6 | Command IDs | Statement identity and exhaustion | ARCHITECTURE-APPROPRIATE |
| 9.7 | Snapshot representation | Snapshot logical fields | ARCHITECTURE-APPROPRIATE |
| 9.7.1 | xmax | Snapshot future-transaction horizon | ARCHITECTURE-APPROPRIATE |
| 9.7.2 | active | Nonterminal active set and self exclusion | ARCHITECTURE WITH DOCUMENT-ROLE ISSUE |
| 9.7.3 | xmin | Vacuum/visibility horizon derivation | ARCHITECTURE-APPROPRIATE |
| 9.7.4 | owner and command | Self-visibility identity | ARCHITECTURE-APPROPRIATE |
| 9.8 | Snapshot capture synchronization | Begin/capture linearization | ARCHITECTURE-APPROPRIATE |
| 9.9 | READ COMMITTED snapshot lifetime | Per-statement snapshot registration | ARCHITECTURE-APPROPRIATE |
| 9.10 | REPEATABLE READ snapshot lifetime | Transaction snapshot lifetime | ARCHITECTURE-APPROPRIATE |
| 9.11 | Transaction-status store | Persistent status file and encodings | ARCHITECTURE-APPROPRIATE |
| 9.11.1 | RESERVED semantics | Recognized nonterminal status meaning | ARCHITECTURE-APPROPRIATE |
| 9.12 | Transaction-status page capacity | Exact page bytes and mapping arithmetic | ARCHITECTURE-APPROPRIATE |
| 9.13 | Transaction-status lookup | Runtime/persistent lookup precedence | ARCHITECTURE-APPROPRIATE |
| 9.14 | Terminal status publication boundary | Terminal-state linearization | ARCHITECTURE-APPROPRIATE |
| 9.14.1 | Runtime terminal publication | Atomic cache/state/registry publication | ARCHITECTURE-APPROPRIATE |
| 9.14.2 | Commit | Commit publication requirements | ARCHITECTURE-APPROPRIATE |
| 9.14.3 | Abort | Abort publication requirements | ARCHITECTURE-APPROPRIATE |
| 9.15 | Read-only transactions | No-WAL/status terminal path | ARCHITECTURE-APPROPRIATE |
| 9.16 | Transaction/snapshot invariants | Consolidated normative invariants | ARCHITECTURE-APPROPRIATE |

## 10. Section-by-section review

Legend: `OK` = clear/consistent; `—` = not owned locally; `N` = note; `F` = finding.
Columns abbreviate the requested dimensions.

| Section | Role | Time | Own | Depth | Terms | Txn | State | Snap | Vis | Cmd | C/A | Status | Rec | Life | Fail | Xref | Sem | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 9.1 | Boundary | OK | OK | OK | OK | — | — | — | — | — | — | — | — | OK | — | F | OK | FINDING |
| 9.2 | Identity | OK | OK | OK | OK | OK | — | — | — | — | — | — | — | — | OK | OK | OK | CLEAN |
| 9.3 | Reservation | OK | OK | OK | OK | OK | — | — | — | — | — | — | — | — | OK | F | OK | FINDING |
| 9.4 | Lifecycle | OK | OK | OK | OK | OK | OK | — | — | — | OK | — | OK | OK | OK | F | OK | FINDING |
| 9.5 | Isolation | OK | OK | OK | OK | — | — | OK | — | — | — | — | — | — | — | OK | OK | CLEAN |
| 9.6 | CommandId | OK | OK | OK | OK | — | — | — | OK | OK | — | — | — | OK | OK | OK | OK | CLEAN |
| 9.7 | Snapshot | N | OK | OK | OK | — | — | OK | OK | OK | — | — | — | OK | — | OK | OK | CLEAN WITH NOTE |
| 9.7.1 | xmax | OK | OK | OK | OK | — | — | OK | OK | — | — | — | — | — | — | OK | OK | CLEAN |
| 9.7.2 | active | F | F | OK | OK | — | — | OK | OK | — | — | — | — | OK | — | OK | OK | FINDING |
| 9.7.3 | xmin | OK | OK | OK | OK | — | — | OK | OK | — | — | — | — | OK | — | OK | OK | CLEAN |
| 9.7.4 | owner/command | OK | OK | OK | OK | — | — | OK | OK | OK | — | — | — | — | — | OK | OK | CLEAN |
| 9.8 | Capture sync | OK | OK | OK | OK | OK | — | OK | OK | — | — | OK | — | OK | — | OK | OK | CLEAN |
| 9.9 | RC lifetime | OK | OK | OK | OK | — | — | OK | OK | OK | — | — | — | OK | OK | OK | OK | CLEAN |
| 9.10 | RR lifetime | OK | OK | OK | OK | — | — | OK | OK | OK | — | — | — | OK | — | OK | OK | CLEAN |
| 9.11 | Status store | OK | OK | OK | OK | — | — | — | — | — | — | OK | OK | — | — | OK | OK | CLEAN |
| 9.11.1 | RESERVED | OK | OK | OK | OK | — | — | — | — | — | — | OK | OK | — | OK | OK | OK | CLEAN |
| 9.12 | Status bytes | OK | OK | OK | OK | OK | — | — | — | — | OK | OK | OK | — | OK | OK | OK | CLEAN |
| 9.13 | Status lookup | OK | OK | OK | OK | — | — | — | OK | — | — | OK | OK | OK | OK | OK | OK | CLEAN |
| 9.14 | Publication | OK | OK | OK | OK | — | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 9.14.1 | Runtime terminal | OK | OK | OK | OK | — | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 9.14.2 | Commit | OK | OK | OK | OK | — | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 9.14.3 | Abort | OK | OK | OK | OK | — | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 9.15 | Read-only | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 9.16 | Invariants | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |

## 11–16. Ownership, TxnId, and CommandId

Chapter 9’s ownership boundary is precise:

- Chapter 9 owns transaction identity, lifecycle, snapshots, command identity, status lookup/publication, and read-only behavior.
- Chapter 10 owns tuple visibility.
- Chapter 11 owns write conflicts and transactional uniqueness.
- Chapters 12–13 own WAL durability and recovery.
- Chapter 14 owns reclamation horizons and physical reuse.
- Chapter 15 owns integrated DML/COMMIT/ABORT sequencing.
- §39 owns failure results and continuation rules.

### Transaction identity

| Property | Contract |
|---|---|
| Width | uint64 |
| Invalid | `INVALID_TXN_ID = 0` |
| Frozen | `FROZEN_TXN_ID = 1` |
| First normal | `2` |
| Allocation | Monotonic, fixed blocks of `2^20` |
| Persistence | Durable exclusive `reserved_txn_id_end` before issue |
| Reuse | Never after durable reservation |
| Crash | Unused reserved suffix may be skipped |
| Maximum normal | `18,446,744,073,708,503,041` |
| Next allocation | `TXN_ID_EXHAUSTED` |
| Wrap | Forbidden |

The maximum status mapping is also representable:

```text
txn_id                  = 18,446,744,073,708,503,041
ordinal                 = 18,446,744,073,708,503,039
status_page_no          = 565,157,600,297,442
entry_in_page           = 28,799
payload_byte_index      = 7,199
page_byte_offset        = 7,231
bit_shift               = 6
```

That PageNo is below the v1 physical maximum `1,125,899,906,842,622`.

### CommandId

| Event | Before | After | Tuple consequence | Self-visibility |
|---|---:|---:|---|---|
| First admitted statement | 0 available | 0 assigned | Writes use `cmin/cmax=0` | Current command excluded by strict comparison |
| Successful statement end | C assigned | C consumed | Next statement gets C+1 | Earlier writes become self-visible |
| Recoverable failed statement | C assigned | C consumed | No reuse even if no tuple exists | Fresh command/snapshot if transaction continues |
| Internal pre-write retry | C assigned | C unchanged | Retry reuses C | Same logical statement boundary |
| Statement enters MUST_ABORT | C assigned | C consumed | Transaction proceeds only to abort | No later ordinary statement |
| Statement using `UINT32_MAX` ends | max assigned | no successor | Existing work remains committable/abortable | Later statement rejected |
| Later statement after maximum | no next ID | unchanged | `COMMAND_ID_EXHAUSTED` before statement publication | N/A |

No CommandId ambiguity or wrap path exists.

## 17–20. Transaction state and begin semantics

### State table

| State | Entry | Statements? | COMMIT? | ABORT? | Snapshot-active? | Durable meaning | Terminal |
|---|---|---:|---:|---:|---:|---|---:|
| ACTIVE | Transaction admitted | Yes | Yes | Yes | Yes | No terminal fact | No |
| MUST_ABORT | Transaction-fatal statement/error before terminal outcome | No | No | Cleanup must drive abort | Yes | No terminal fact | No |
| COMMITTING | Accepted COMMIT | No | Already executing | Only before authorizing append via MUST_ABORT | Yes | Commit record may be absent/appended/durable | No |
| ABORTING | Abort initiated | No | No | Already executing | Yes | Abort record may be absent/appended | No |
| COMMITTED | Runtime terminal publication after durable COMMIT | No | No | No | No | Irreversible committed outcome | Yes |
| ABORTED | Runtime terminal publication of abort | No | No | No | No | Terminal aborted outcome | Yes |

### Legal transitions

| Source | Event | Durable prerequisite | Destination | Client result | Cleanup |
|---|---|---|---|---|---|
| ACTIVE | Accept COMMIT | C0/C1 prerequisites | COMMITTING | Pending | Retain locks/registry |
| COMMITTING | Durable commit plus runtime publication | Durable `TXN_COMMIT` | COMMITTED | Success only after C5 | Release after publication |
| COMMITTING | Known failure before authorizing append | No commit record | MUST_ABORT | Commit fails | Retain ownership; abort |
| ACTIVE | Transaction-fatal statement | None | MUST_ABORT | Original error | Automatic abort |
| ACTIVE | Explicit/required abort | None | ABORTING | Pending | Retain ownership |
| MUST_ABORT | Automatic cleanup | None | ABORTING | Original error retained | Retain ownership |
| ABORTING | Runtime terminal publication | Required abort prerequisites | ABORTED | ROLLBACK/error completion after A3 | Release after publication |

Forbidden transitions include every terminal reversal and every post-authorizing-commit transition toward abort.

Begin semantics are reconstructable without invention:

1. obtain a durably reserved TxnId;
2. create ACTIVE runtime state with CommandId 0;
3. register the transaction under the same short synchronization domain used for snapshot high-water capture;
4. do not emit a durable begin/status record;
5. acquire RC/RR snapshots at the Chapter-9-defined statement boundary, not automatically at `BEGIN`.

## 21–34. Snapshot and visibility assessment

### Snapshot representation

| Component | Meaning | Captured | Compared against | Lifetime |
|---|---|---|---|---|
| `xmax` | Next normal TxnId not assigned | Capture linearization | `xmin/xmax` TxnIds | Snapshot lifetime |
| `active` | Other nonterminal normal TxnIds `< xmax` | Same linearization | Creator/deleter membership | Snapshot lifetime |
| `xmin` | Minimum `active`, else `xmax` | Derived at capture | Vacuum horizon | Snapshot lifetime |
| `owner_txn_id` | Evaluating transaction | Capture | Self creator/deleter | Snapshot lifetime |
| `command_id` | Statement-order boundary | Statement boundary | `cmin/cmax` | Updated per RR statement |

READ COMMITTED uses one stable snapshot per attempt and a fresh snapshot for the next statement or permitted pre-write retry. REPEATABLE READ captures on its first ordinary statement and retains the same transaction snapshot while updating only `command_id`.

Snapshot capture and transaction registration share one synchronization protocol, preventing a transaction from falling outside both the high-water and active-set classifications.

### Visibility matrix

Exact visibility is Chapter-10-owned.

| Case | Visible? | Reason |
|---|---:|---|
| `xmin=FROZEN_TXN_ID` | Creator passes | Frozen means committed |
| `xmin=self`, `cmin < command_id` | Creator passes | Earlier own command |
| `xmin=self`, `cmin >= command_id` | No | Current command not rediscovered |
| Creator status not COMMITTED | No | In-progress/aborted creator |
| Committed creator `xmin >= xmax` | No | Too new |
| Committed creator in `active` | No | Active at capture |
| Other committed creator before horizon, not active | Creator passes | Visible past |
| `xmax=INVALID_TXN_ID` | Yes after creator passes | No deleter |
| `xmax=self`, `cmax < command_id` | No | Deleted by earlier own command |
| `xmax=self`, `cmax >= command_id` | Yes | Current command does not erase its own input |
| Other deleter ABORTED | Yes | Delete ineffective |
| Other deleter IN_PROGRESS | Yes | Read does not wait |
| Committed deleter before horizon, not active | No | Delete visible |
| Committed deleter too new/in active | Yes | Delete invisible to snapshot |

### Update-version cases

| Situation | Old version | New version |
|---|---|---|
| Same transaction, current command | Still visible to current command | Not visible through ordinary rescan |
| Same transaction, later command | Hidden by prior `cmax` | Visible through prior `cmin` |
| Concurrent reader before updater commit | Visible | Invisible |
| Reader snapshot includes updater active | Visible even after updater later commits | Invisible to that snapshot |
| Updater commits before new snapshot | Hidden | Visible |
| Updater aborts | Old aborted `xmax` ineffective | New aborted `xmin` invisible |

Aborted inserts and updates may remain physically present. Visibility and reclamation remain separate. Version-chain storage is Chapter 5-owned; visibility is Chapter 10-owned; reuse safety is Chapter 14-owned.

## 35–37. Transaction-status authority

### Status domain

| Status/result | Encoding | Runtime meaning | Visibility effect | Persistent? |
|---|---:|---|---|---:|
| INVALID | `00` | No terminal record | Must not be guessed | Yes |
| COMMITTED | `01` | Terminal commit | Creator/deleter committed | Yes |
| ABORTED | `10` | Terminal abort | Creator invisible; delete ineffective | Yes |
| RESERVED | `11` | Recognized nonterminal marker | Neither commit nor abort | Yes |
| SELF | — | Current transaction | Use command rules | No |
| IN_PROGRESS | — | Active nonterminal transaction | Nonterminal | No |
| RETIRED | — | Status history safely reclaimed | Caller must already be status-independent | No |

Lookup precedence is:

1. FROZEN;
2. SELF;
3. runtime terminal cache;
4. active registry;
5. retired cutoff;
6. cached/persistent status.

Terminal WAL is authoritative after crash. Runtime cache/registry is authoritative for the current process linearization. Persistent status pages are a no-force acceleration/reconstruction target, not an independent competing commit authority.

## 38–50. Commit, abort, and failure boundaries

### Commit protocol

| Step | Runtime state | WAL/status | Ownership | Success allowed? | Failure meaning |
|---|---|---|---|---:|---|
| C0 | ACTIVE | None | Statement finished | No | Reject invalid state |
| C1 | ACTIVE | Reserve all resources | Retain all locks/gates | No | Remain ACTIVE or exact §39 result |
| C2 | COMMITTING | Optional PAGE_IMAGE, append COMMIT, install status bits | Retain | No | Pre-append may abort; post-append uncancellable |
| C3 | COMMITTING | Wait `durable_lsn >= commit_lsn` | Retain | No | Retry/escalate; never abort appended COMMIT |
| C4 | COMMITTED | Runtime terminal cache/state/registry publication | Retain through linearization | No | Post-durable failure is noncontinuable/uncertain, still committed |
| C5 | COMMITTED | Caches coherent/invalidated | Release locks, gates, snapshots | No | Committed; cleanup failure affects health |
| C6 | COMMITTED | No data-page force required | Cleanup complete | Yes | Transport failure creates client uncertainty only |

Durable commit occurs at C3. Data pages need not be flushed. A durable COMMIT cannot transition to ABORTED.

### Abort protocol

| Step | Runtime state | WAL/status | Ownership | Acknowledgment? |
|---|---|---|---|---:|
| A0 | ABORTING | Prepare abort | Retain all ownership | No |
| A1 | ABORTING | Optional PAGE_IMAGE; append ABORT; install status | Retain | No |
| A2 | ABORTED | Runtime terminal publication | Retain through linearization | No |
| A3 | ABORTED | No physical user-DML undo | Release locks/gates/snapshots | No |
| A4 | ABORTED | Cleanup complete | Released | Yes |

Abort need not synchronously flush its WAL before runtime ABORTED publication; WAL-before-data prevents unsafe status-page persistence, and crash loser resolution establishes ABORTED again.

`MUST_ABORT` is the explicit doomed state. Statement errors are classified by §39 into:

- `FAILED_TRANSACTION_REMAINS_ACTIVE`;
- `FAILED_TRANSACTION_MUST_ABORT`;
- `DATABASE_NONCONTINUABLE`.

Not every statement failure aborts a transaction.

## 51–62. Locks, lifetime, read-only, and cross-owner scope

- Transaction locks and gates remain held through terminal publication.
- Page latches are short-lived and are never transaction-lifetime locks.
- Snapshot registration lasts through the executing RC attempt or entire RR transaction.
- Read epochs are separate from SQL snapshots and are acquired around RID-bearing execution access, not at transaction begin.
- Transaction objects retain conceptual identity, state, command, snapshots, WAL tail, logical ownership, and cancellation state through terminal cleanup.
- Cursors/query execution cannot outlive the transaction/snapshot/read-epoch context that makes their references legal.
- Read-only transactions receive TxnIds, enter the active registry, and register snapshots, but write no terminal status or COMMIT WAL.
- Autocommit is owned by §39.1 and the executor/DML integration: one implicit transaction commits after successful statement completion and aborts on failure.
- Savepoints, subtransactions, prepared transactions, 2PC/XA, and distributed transactions are outside the v1 baseline.
- Supported isolation levels are READ COMMITTED and snapshot-isolation REPEATABLE READ. SERIALIZABLE/SSI are deferred.
- Chapter 11 owns write-write and uniqueness conflicts; Chapter 8’s physical duplicate-key behavior does not implement logical uniqueness.

## 63–77. Status storage, recovery, lifecycle, and failure

### Status storage

- File: `txn_status.dat`
- Page 0: `FileKind::TXN_STATUS = 5`, `object_id=0`
- Pages 1..N: `PageType::TXN_STATUS = 7`
- Header: common 32 bytes, no specialized header
- Payload: 8,160 bytes
- Entries: four two-bit statuses per byte
- Capacity: 32,640 normal TxnIds per page
- Mapping: absolute and permanent; reclaimed pages are sparse holes, not renumbered
- New pages use PAGE_INIT before terminal records
- Terminal mutation follows §12.10.5

### Recovery matrix

| Crash state | WAL evidence | Status evidence | Recovered outcome | READY condition |
|---|---|---|---|---|
| Active/no terminal record | No terminal | INVALID/RESERVED/stale | ABORTED loser | Recovery ABORT recorded/repaired |
| Durable COMMIT | Complete commit in valid prefix | May be stale/missing | COMMITTED | Status repaired |
| Complete commit survived but client not acknowledged | Complete commit | Any older legal image | COMMITTED | Status reconciled |
| Durable ABORT | Abort record | May be stale | ABORTED | Status repaired |
| Abort record lost | No terminal commit | Stale/nonterminal | ABORTED loser | New recovery abort |
| Uncertain pre-crash API outcome | Inspect valid WAL prefix | Never trust remembered result | Record present → terminal; absent → loser | Reconciliation complete |
| Torn status page | Retained full image plus terminal WAL | Untrusted page | Reconstruct then replay terminals | L0/L1 valid |
| Status below reclaim cutoff | Retired history | Page may be punched | RETIRED/no lookup | Persistent objects status-independent |

READY is forbidden until status reconciliation, loser resolution, allocator high-water reconstruction, catalog/file ownership validation, and recovery checkpoint completion.

DRAINING rejects new transactions/statements, forces ACTIVE/MUST_ABORT toward abort, lets COMMITTING/ABORTING complete, waits for terminal publication and resource release, and tears down durability services last. NONCONTINUABLE admits no new ordinary work and never reverses an already durable COMMIT.

### Failure matrix

| Failure | Logical state/outcome | May continue? | Recovery owner |
|---|---|---:|---|
| TxnId exhaustion | No transaction begun | Existing work yes | Control high-water |
| CommandId exhaustion | Existing transaction remains ACTIVE | COMMIT/ROLLBACK only | N/A |
| Statement-local recoverable failure | ACTIVE | Yes | §39.1 |
| Transaction-fatal statement | MUST_ABORT → ABORTED | No | §39.1/§15.6 |
| Commit reservation/known pre-append failure | ACTIVE or MUST_ABORT | Exact §39 rule | §39.1.5 |
| Valid commit append, not durable | COMMITTING, uncancellable | No ordinary work | WAL retry/recovery |
| Commit append uncertainty | NONCONTINUABLE | No | WAL prefix on reopen |
| Post-durable publication failure | Semantically COMMITTED | No ordinary continuation | Recovery repairs runtime/status |
| Abort append known failure | ABORTING retained | Retry cleanup only | §39.1.6 |
| Abort uncertainty/publication failure | ABORTING/NONCONTINUABLE | No | Recovery loser handling |
| Status corruption | Access/open/recovery failure | No guessing | Full-image and terminal WAL if recoverable |
| Shutdown/noncontinuable | State-specific drain | No new work | Chapters 3/13 |

## 78–88. Key semantic distinctions and numeric domains

All required distinctions are explicit:

- COMMITTED does not mean data pages are flushed.
- ABORTED does not mean heap/index bytes were physically undone.
- Visible does not imply committed: earlier own writes can be visible.
- Committed does not imply visible to every snapshot.
- Aborted `xmax` does not hide the original row.
- SQL snapshot lifetime and RID read-epoch lifetime are separate.

### Exhaustion table

| Domain | Maximum | Last legal action | Next result | No-wrap/reuse basis |
|---|---:|---|---|---|
| TxnId | 18,446,744,073,708,503,041 | Issue from terminal exact block | `TXN_ID_EXHAUSTED` | Exact `2^20` reservation sequence |
| CommandId | 4,294,967,295 | Admit one statement with max | `COMMAND_ID_EXHAUSTED` | No successor computed |
| Status PageNo | 565,157,600,297,442 for max TxnId | Map maximum TxnId | N/A beyond legal TxnId | Checked absolute mapping |
| Snapshot active size | Runtime/resource bounded | Represent all relevant active TxnIds | Structured resource failure | No persisted narrowing |
| Read epoch | Chapter 14, uint64 | Current may equal UINT64_MAX | Further retirement/reuse disabled | No wrap; restart/quiescence required |

## 89–100. Documentation-model assessment

### Global answers

| Question | Result |
|---|---|
| Analytical rather than chronological? | YES overall; one localized exception |
| Current-state narration? | NO |
| DEVELOPMENT sequencing leakage? | YES, §9.7.2 |
| VERIFICATION procedure leakage? | NO |
| PROJECT_STATE leakage? | NO |
| Devlog/history leakage? | NO |
| Implementation absence confused with optionality? | NO |
| Correctness-relevant terminology ambiguous? | NO |
| Rationale sufficient? | YES |
| Readable without implementation-status knowledge? | YES |
| Fully timeless canonical contract as written? | NO, pending the localized wording fix |
| Technically canonical and implementable? | YES |

### Temporal-language classification

| Language | Classification | Result |
|---|---|---|
| “current transaction/command/currently executing” | Runtime or MVCC ordering | Valid |
| “later statement/commits later” | Transaction history | Valid |
| “initial reservation/initial command/newly initialized” | Runtime initial state | Valid |
| “Future architecture may assign…” for RESERVED | Persistent architecture evolution | Valid |
| “later-owned/later chapters” | Document navigation | Valid but imprecise |
| “Initial membership testing…” | Project/implementation staging | Finding |
| “A later high-concurrency implementation…” | Project roadmap/sequencing | Finding |
| Phase/current implementation language | Project state | Absent |

### Document ownership

| Material | Proper owner | Chapter-9 result |
|---|---|---|
| Transaction lifecycle/snapshot/status semantics | ARCHITECTURE | Correct |
| Implementation order/optimization staging | DEVELOPMENT | One leak in §9.7.2 |
| Deterministic race/fault procedures | VERIFICATION | No leakage |
| Implementation availability | PROJECT_STATE | No leakage |
| Historical events/results | devlog | No leakage |

### Analytical depth

| Boundary | Result |
|---|---|
| TxnId nonreuse/reservation | Analytically sufficient |
| State machine/terminal immutability | Analytically sufficient |
| Snapshot capture/high-water | Analytically sufficient |
| Command self-visibility | Analytically sufficient with Chapter 10 |
| Commit durability/publication | Analytically sufficient |
| Abort logical-vs-physical behavior | Analytically sufficient |
| Status authority/recovery | Analytically sufficient with Chapters 12–13 |
| Snapshot/reclamation horizon | Analytically sufficient with Chapter 14 |
| Lock release/publication | Analytically sufficient |

No source paths, concrete mutex types, TODOs, or source-file ownership appear. The conceptual transaction fields and v1 sorted snapshot vector are architectural runtime contracts, not source-layout coupling. Representation freedom is preserved outside architecture-fixed semantics.

## 101–120. Consolidated required matrices

### Concurrency matrix

| Race | Ordering point | Legal outcomes | Forbidden outcome |
|---|---|---|---|
| BEGIN vs snapshot capture | Registry/high-water synchronization | New TxnId is either active below xmax or at/above xmax | Neither active nor too-new |
| COMMIT vs capture | Runtime terminal publication | Before: active/invisible; after: terminal/excluded | Locks released while lookup says IN_PROGRESS |
| ABORT vs lookup | Runtime terminal publication | Before: nonterminal; after: ABORTED | Guess from tuple bytes |
| Read vs COMMIT | Snapshot membership/horizon | Old snapshot may not see new commit | Commit visible merely because data flushed |
| Read vs ABORT | Status/cache linearization | Aborted creator invisible; aborted delete ineffective | Aborted delete hides row |
| Update conflict | Chapter 11 lock/recheck | Wait/recheck or isolation error | MVCC read rule substitutes for write lock |
| Cleanup vs snapshot horizon | Snapshot unregister | Reclamation only after no legal snapshot needs version | Early reclaim |
| RID reuse vs reader | ReadEpochManager grace | Reuse after exact grace | Replacement observed through old RID |
| Shutdown vs BEGIN | READY→DRAINING gate | Existing terminal work drains | New transaction admitted |
| Shutdown vs active txn | §3.3.6 state handling | Abort or finish terminal work | Owner resources destroyed first |

### Cross-chapter consistency

| Chapter | Consumed contract | Result |
|---|---|---|
| 3 | READY/DRAINING/NONCONTINUABLE; durable commit irreversibility | CONSISTENT |
| 4 | TxnId/CommandId domains, status validation, exhaustion | CONSISTENT |
| 5 | xmin/xmax/cmin/cmax, physical versions and RIDs | CONSISTENT |
| 7 | Dirty ≠ durable; latch ≠ lock; WAL-before-data | CONSISTENT |
| 8 | Index candidate ≠ visibility; heap recheck | CONSISTENT |
| 10 | Exact creator/deleter visibility | CONSISTENT BUT SPECIALIZED |
| 11 | Write/unique conflicts and lock lifetime | CONSISTENT BUT SPECIALIZED |
| 12 | Terminal WAL/status mutation and durability | CONSISTENT BUT SPECIALIZED |
| 13 | Terminal reconstruction and loser resolution | CONSISTENT BUT SPECIALIZED |
| 14 | Snapshot horizon, read epochs, status reclaim | CONSISTENT BUT SPECIALIZED |
| 15 | Integrated DML/COMMIT/ABORT sequence | CONSISTENT BUT SPECIALIZED |
| 39 | Failure and client-result taxonomy | CONSISTENT BUT SPECIALIZED |
| 41 | Verification obligations | CONSISTENT; methodology gaps remain in VERIFICATION |

### Explicit cross-reference audit

| Source | Target | Purpose | Exists/owner | Precision |
|---|---|---|---|---|
| 9.1 | Chapter 10 | Tuple visibility | Correct | Precise |
| 9.1 | Chapter 11 | Write/unique conflicts | Correct | Precise |
| 9.1 | “later durability/recovery chapters” | WAL/checkpoint/recovery | Correct owner | Vague |
| 9.2 | §4.3.2.1 | TxnId maximum | Correct | Precise |
| 9.3 | §4.3.2.1 | Reservation exhaustion | Correct | Precise |
| 9.3 | “later durability/recovery chapter” | Control layout/torn update | §13.2 | Vague |
| 9.4 | §39.1 | Post-append completion/failure | Correct | Precise |
| 9.4 | §§3.3, 3.3.6 | Admission/shutdown | Correct | Precise |
| 9.4 | “later durability/recovery chapters” | Commit/recovery completion | Correct owner | Vague |
| 9.6 | §4.3.2.2 | CommandId maximum | Correct | Precise |
| 9.9 | §15.7 | RC retry boundary | Correct | Precise |
| 9.11.1 | §4.14.4 | Known status codes | Correct | Precise |
| 9.12 | §4.13.6 | Status validation | Correct | Precise |
| 9.12 | §§12.10.5, 13.13.2 | Status publication/redo | Correct | Precise |
| 9.13 | §34.3.1 | StatsVersion non-status use | Correct | Precise |
| 9.14.2 | §§15.5, 12.10.5, 39.1.5 | Commit sequence/failure | Correct | Precise |
| 9.14.3 | §§15.6, 12.10.5, 39.1.6 | Abort sequence/failure | Correct | Precise |

### Terminology

| Term | Canonical meaning | Ambiguity |
|---|---|---|
| ACTIVE | Runtime state admitting ordinary statements | None |
| Nonterminal | ACTIVE/MUST_ABORT/COMMITTING/ABORTING | None |
| Terminal | COMMITTED or ABORTED | None |
| Durable COMMIT | Commit record included in durable WAL prefix | None |
| Runtime terminal publication | Cache/state/registry linearization | None |
| Snapshot-active | Nonterminal at capture and represented in `active` | None |
| Visible | Tuple passes Chapter-10 snapshot/status rules | Not synonymous with committed |
| MUST_ABORT | Doomed, nonterminal, abort-only runtime state | None |
| RESERVED | Recognized nonterminal persisted status code | None |
| RETIRED | Runtime result for reclaimed status history | Not persisted |
| Transaction end | Terminal publication plus required cleanup | Context is clear |
| Read epoch | Physical RID-lifetime protection | Distinct from SQL snapshot |

### Normative language

| Contract | Strength | Result |
|---|---|---|
| TxnId nonreuse/no wrap | MUST/MUST NOT | Correct |
| Durable reservation before handout | Ordered normative protocol | Correct |
| Legal lifecycle transitions | Exact closed state graph | Correct |
| CommandId nonreuse | Exact normative behavior | Correct |
| Snapshot atomicity | MUST | Correct |
| Terminal lock release | MUST NOT before publication | Correct |
| Commit publication | Only after durable WAL | Correct |
| Durable commit reversal | Forbidden | Correct |
| Status guessing | MUST NOT | Correct |
| Read-only WAL/status elision | Defined permission | Correct |

## 121. Seventy-item technical consistency matrix

`CS` means consistent but specialized by another canonical section.

| # | Item | Result | # | Item | Result |
|---:|---|---|---:|---|---|
| 1 | Chapter-9 ownership boundary | CONSISTENT | 36 | TxnStatus authority | CS |
| 2 | TxnId width/domain | CONSISTENT | 37 | Status state domain | CONSISTENT |
| 3 | TxnId invalid sentinel | CONSISTENT | 38 | Commit protocol | CS |
| 4 | TxnId nonreuse | CONSISTENT | 39 | Durable commit point | CONSISTENT |
| 5 | TxnId allocation | CONSISTENT | 40 | Client acknowledgment | CS |
| 6 | TxnId exhaustion | CONSISTENT | 41 | COMMITTED irreversible | CONSISTENT |
| 7 | CommandId width/domain | CONSISTENT | 42 | Pre-durable commit failure | CS |
| 8 | Initial CommandId | CONSISTENT | 43 | Post-durable failure | CS |
| 9 | Increment point | CONSISTENT | 44 | Uncertain commit | CS |
| 10 | CommandId exhaustion | CONSISTENT | 45 | Abort protocol | CS |
| 11 | State set | CONSISTENT | 46 | Physical undo | CONSISTENT |
| 12 | Legal transitions | CONSISTENT | 47 | Statement vs transaction failure | CS |
| 13 | Terminal immutability | CONSISTENT | 48 | Doomed state | CONSISTENT |
| 14 | Begin registration | CONSISTENT | 49 | Lock release | CS |
| 15 | Snapshot owner | CONSISTENT | 50 | Page-latch distinction | CS |
| 16 | Capture point | CONSISTENT | 51 | Active registry role | CONSISTENT |
| 17 | Snapshot lifetime | CONSISTENT | 52 | Transaction object lifetime | CONSISTENT |
| 18 | Txn/statement snapshots | CONSISTENT | 53 | Read-epoch lifetime | CS |
| 19 | Own insert visibility | CS | 54 | Read-only transactions | CONSISTENT |
| 20 | Own delete visibility | CS | 55 | Autocommit/implicit owner | CS |
| 21 | cmin | CS | 56 | Nested/savepoint scope | CS |
| 22 | cmax | CS | 57 | Isolation owner | CONSISTENT |
| 23 | xmin committed before | CS | 58 | Write conflict owner | CS |
| 24 | xmin committed after | CS | 59 | Unique conflict owner | CS |
| 25 | xmin in progress | CS | 60 | Status mapping | CONSISTENT |
| 26 | xmin aborted | CS | 61 | Status extension | CS |
| 27 | xmax absent | CS | 62 | Status corruption | CS |
| 28 | xmax committed before | CS | 63 | Active at crash | CS |
| 29 | xmax committed after | CS | 64 | Durable commit recovery | CS |
| 30 | xmax in progress | CS | 65 | Abort recovery | CS |
| 31 | xmax aborted | CS | 66 | Status/WAL reconciliation | CS |
| 32 | Aborted insert | CS | 67 | READY gate | CS |
| 33 | Aborted delete | CS | 68 | Shutdown gate | CS |
| 34 | Update visibility | CS | 69 | Failure taxonomy | CS |
| 35 | Version-chain owner | CS | 70 | Implementer invention | CONSISTENT—none |

## 122. Documentation-model matrix

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | No current implementation status | CONSISTENT |
| 3 | No Phase-2 narration | CONSISTENT |
| 4 | No implementation sequencing | FINDING |
| 5 | No VERIFICATION leakage | CONSISTENT |
| 6 | No PROJECT_STATE leakage | CONSISTENT |
| 7 | No devlog/history leakage | CONSISTENT |
| 8 | No source-layout coupling | CONSISTENT |
| 9 | Transaction-state terminology | CONSISTENT |
| 10 | Committed/durable distinction | CONSISTENT |
| 11 | Committed/visible distinction | CONSISTENT |
| 12 | Visible/own-write distinction | CONSISTENT |
| 13 | Abort/physical-undo distinction | CONSISTENT |
| 14 | Snapshot terminology | CONSISTENT |
| 15 | Command terminology | CONSISTENT |
| 16 | Status-authority references | CONSISTENT |
| 17 | Durable-commit rationale | CONSISTENT |
| 18 | Snapshot-visibility rationale | CONSISTENT |
| 19 | Abort/reclamation rationale | CONSISTENT |
| 20 | Independent of implementation status | CONSISTENT |

## 123–126. Complete findings

### BLOCKING findings

None.

### MAJOR findings

None.

### MINOR finding M1

- **Section:** §9.7.2, [lines 7065–7069](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:7065)
- **Evidence:** “Initial membership testing MAY therefore use binary search. A later high-concurrency implementation MAY replace this with a hybrid or different runtime representation…”
- **Severity:** MINOR
- **Type:** DOCUMENT OWNERSHIP
- **Scope:** Local, with Appendix-C scope context
- **Arithmetic:** N/A
- **Explanation:** The first sentence describes an implementation stage, and the second describes a later implementation roadmap. The semantic freedom—equivalent active-set representations preserving snapshot semantics—is architecture-appropriate; “initial” and “later implementation” sequencing is not.
- **Canonical comparison:** Appendix C separately treats high-concurrency alternative snapshot representations as outside the v1 baseline.
- **Consequence:** Chapter 9 is not fully time-independent and can be read as prescribing contributor sequencing rather than the canonical v1 contract.
- **Correct owner:** Timeless semantic freedom remains in ARCHITECTURE; implementation order belongs in DEVELOPMENT.
- **Future action:** **D. DEVELOPMENT-OWNERSHIP FIX**—rewrite locally as a timeless v1 representation/semantic-equivalence rule; do not add progress history.

### EDITORIAL finding E1

- **Sections:** §§9.1, 9.3, 9.4
- **Evidence:**
  - [§9.1 line 6753](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6753): “The later durability/recovery chapters…”
  - [§9.3 line 6871](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6871): “the later durability/recovery chapter”
  - [§9.4 line 6928](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:6928): “the later durability/recovery chapters”
- **Severity:** EDITORIAL
- **Type:** CROSS-REFERENCE
- **Scope:** Cross-section
- **Arithmetic:** N/A
- **Explanation:** The owner is understandable but navigation is vague despite precise canonical sections existing.
- **Canonical comparison:** Control-file updates are in §13.2; terminal status/WAL in §12.10.5 and §§12.13–12.15; recovery/status reconstruction in §§13.12–13.19.
- **Consequence:** No semantic ambiguity, but readers must search broadly and future chapter reorganization could weaken ownership navigation.
- **Correct owner:** ARCHITECTURE cross-reference/navigation.
- **Future action:** **G. CROSS-REFERENCE FIX**.

## 127. Frozen architecture semantic questions

**None.**

No transaction identity, lifecycle, snapshot, visibility, status, durability, recovery, or reclamation decision requires frozen semantic review.

## 128. Follow-up verification gaps

A separate verification synchronization is recommended. Existing verification is strong for:

- TxnId/CommandId exhaustion;
- statement failure classification;
- C0–C6 commit faults;
- A0–A4 abort faults;
- basic MVCC matrices;
- RC/RR end-to-end isolation;
- shutdown/noncontinuable behavior;
- vacuum/read-epoch reclamation.

The following Chapter-9-specific deterministic procedures remain missing or only partial:

1. exact `txn_status.dat` superblock/page/two-bit byte and mapping matrix, including `RESERVED`, high-water-invalid entries, and checked 32,640-entry boundaries;
2. deterministic BEGIN/TxnId-registration versus snapshot-capture high-water race;
3. deterministic COMMIT/ABORT terminal-publication versus snapshot-capture race;
4. direct snapshot representation checks for owner exclusion, sorted active membership, `xmin` derivation, and stable RC/RR registration lifetime;
5. status-lookup precedence covering FROZEN, SELF, terminal-cache-over-stale-active, IN_PROGRESS, RETIRED, INVALID, and RESERVED;
6. status-page extension plus PAGE_INIT/terminal-record reconstruction at first entry and page boundaries;
7. explicit stale/torn status-page versus terminal-WAL reconciliation fixtures;
8. read-only transaction verification proving active/snapshot/vacuum participation while emitting no terminal WAL/status entry.

**FOLLOW-UP VERIFICATION GAP: OPEN.**

## 129. Out-of-scope observations

- Appendix C calls the sorted-vector model “initial” at [line 25214](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:25214). That is outside Chapter 9 and should be handled by a future Appendix-C/document-scope review.
- Chapter 10 contains similarly broad “later WAL/recovery chapters” navigation. Its precision belongs to the direct Chapter-10 review unless it contradicts Chapter 9; no contradiction was found.

## 130–152. Direct-answer matrix

| Question | Answer |
|---|---|
| TxnId identity/reuse ambiguity? | NO |
| CommandId ambiguity? | NO |
| Transaction-state contradiction? | NO |
| Begin/snapshot ambiguity? | NO |
| Self-visibility ambiguity? | NO |
| xmin/xmax ambiguity? | NO |
| TxnStatus authority ambiguity? | NO |
| Durable-commit point ambiguity? | NO |
| Post-durable reversal path? | NO |
| Abort ambiguity? | NO |
| Statement/transaction-failure ambiguity? | NO |
| Status/WAL recovery ambiguity? | NO |
| Read-epoch/reclamation ambiguity? | NO |
| Shutdown transaction ambiguity? | NO |
| Correctness policy invention required? | NO |
| Project-time/current-state wording? | YES—localized §9.7.2 |
| DEVELOPMENT-owned material? | YES—localized sequencing |
| VERIFICATION procedure leakage? | NO |
| PROJECT_STATE material? | NO |
| Devlog/history material? | NO |
| Ambiguous correctness terminology? | NO |
| Underexplained critical boundary? | NO |
| Can Chapter 9 stand as timeless canonical v1 as written? | Technically yes, but document-clean timelessness requires the targeted wording fix |

## 153–156. Regression and next action

Previous-chapter regression result: **PASS**.

- Chapter 3 durable-COMMIT irreversibility is preserved.
- Chapter 4 numeric/status foundations are consumed exactly.
- Chapter 5 physical version and command metadata are consistent.
- Chapter 7 dirty/durable and latch/lock distinctions are preserved.
- Chapter 8’s mandatory heap visibility recheck remains intact.

Chapter-8 compatibility result: **CONSISTENT**. Chapter 9 does not treat physical index presence or duplicate suppression as MVCC visibility or transactional uniqueness.

Recommended next action: **targeted documentation edit**, followed by separate Chapter-9 verification synchronization.

Recommended Chapter-10 review scope, based on the actual boundary:

- exact creator/deleter visibility;
- strict `cmin/cmax` self-command rules;
- FROZEN/SELF/IN_PROGRESS/COMMITTED/ABORTED interaction;
- committed-before/after-snapshot rules;
- aborted INSERT/DELETE/UPDATE;
- visibility evaluation order;
- status lookup error/corruption handling;
- hint cleanup and its WAL/reclamation boundary;
- separation of read visibility from Chapter-11 write conflicts;
- document temporality and vague recovery references.

## 157–164. Read-only and repository-state confirmation

Initial repository state:

```text
git status --short:          clean
git diff --cached --name-only: empty
HEAD: 43d81680b388e78c52b534ab39fc1b87cc61ee53
```

Final repository state:

```text
git status --short:          clean
git diff --cached --name-only: empty
HEAD: 43d81680b388e78c52b534ab39fc1b87cc61ee53
git diff --check:            passed, no output
```

- Files modified by audit: **NONE**
- Repository state changed by audit: **NO**
- Audit-created artifacts: **NONE**
- Pre-existing material modified, removed, or staged: **NO**
- Builds/tests/benchmarks run: **NONE**
- Implementation work performed: **NONE**
- Chapter 10 review started: **NO**
- Phase 2 remains: **NOT STARTED / NOT AUTHORIZED**