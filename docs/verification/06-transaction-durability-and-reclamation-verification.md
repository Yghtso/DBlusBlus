# Transaction, Durability, and Reclamation Verification

## Transaction identity, snapshot, and status verification

This section is the detailed procedural owner for the transaction identity, snapshot,
transaction-status, and terminal-publication contracts in
[`ARCHITECTURE.md`](../ARCHITECTURE.md) §§9.1–9.16. Numeric Exhaustion and
Terminal-Boundary Verification remains the detailed owner for TxnId reservation/exhaustion
and CommandId boundaries; the COMMIT, ABORT, lifecycle, recovery, isolation, and vacuum
sections below remain the detailed owners for their existing cross-layer procedures. The
procedures here add the Chapter-9-specific byte, lookup, lifetime, and race oracles without
redefining tuple visibility (§10), write/uniqueness conflicts (§11), WAL semantics
(Chapter 12), recovery outcomes (Chapter 13), or read-epoch reclamation (Chapter 14).

### Deterministic harness, observability, and independent oracles

Transaction concurrency verification uses controlled scheduling and barriers rather than
elapsed-time sleeps or repeated stress until a race occurs. The harness can pause and record
these semantic events without requiring production function names:

```text
TxnId reservation begins / control high-water becomes durable / TxnId is issued
transaction registration begins / active-registry publication completes
snapshot high-water is read / active set is captured / snapshot registration completes
terminal status-page mutation begins / terminal WAL append completes
terminal WAL becomes durable
runtime terminal-cache publication / runtime state transition / active removal
snapshot unregister / read-only terminal cleanup
status-page PAGE_INIT reservation / append / page-bound publication
crash or injected failure
recovery image reconstruction / terminal redo / loser resolution / READY publication
```

The event log records TxnIds, isolation and CommandId, runtime states, snapshot fields,
active-registry membership, terminal-cache result, transaction `last_wal_lsn` and
persistent/current-statement write flags, durable WAL end, status PageId and bytes, file
published bound, held logical locks/gates, cancellation/deadlock state, snapshot-horizon
registration, database health, and client result. Every race below forces both legal orders
at the owning linearization point. Random stress remains complementary; it is not a
substitute for these schedules.

Test-side oracles are independent:

- TxnId-to-status mapping is computed from mathematical integers and checked before
  conversion to persisted widths;
- snapshot membership is an expected ordered set built from the harness-controlled states,
  not copied from the production snapshot;
- terminal-race outcomes follow the barrier order and §9.14 linearization, not the state
  returned by the implementation under test;
- recovery outcomes derive from the independently constructed valid durable WAL prefix, not
  from resident status bytes or the precrash API result.

Every valid fixture is canonical except for the one defect under test. Valid status fixtures
have the correct file/page identity, published bound, checksum, version, flags/reserved
bytes, PAGE_INIT history, and registered owner. A status-lookup test cannot accidentally use
a corrupt page, and a corruption test changes only its selected dimension.

### TXN_STATUS persisted bytes, capacity, and mapping

The generic FileSuperblock codec and registered-owner procedures in Storage Verification and
§§4.10, 4.13.6 verify the `txn_status.dat` specialization with this matrix:

| Region/property | Canonical fixture | Independent mutation/oracle |
|---|---|---|
| Page 0 framing | Exact 8192-byte FileSuperblock; `FileKind::TXN_STATUS=5`; valid FileId; `object_id=0`; page size/version/header fields from §4.10 | Mutate kind, FileId, object identity, version, header size, flags, checksum, or each required-zero field/range independently |
| Trailing superblock bytes | Generic 72-byte header followed by canonical zero bytes | Walk the complete required-zero suffix with one nonzero byte per fixture |
| Physical/published bound | Aligned file length and §12.12-reconciled process-local `published_page_count`; this is not invented as a superblock field | Truncated, overlong, misaligned, and publication/WAL-disagreeing fixtures never expose an unowned page |
| Data-page framing | 32-byte common header; `PageType::TXN_STATUS=7`; no specialized header; payload bytes `32..8191` | Mutate PageId, type, version, `header_size`, flags/reserved, checksum, page length, and published-bound relation one at a time |

Ordinary lookup validates registered file/owner and published PageNo, performs an exact read,
dispatches file/page version, verifies checksum and common PageId/header fields, verifies
TXN_STATUS type, and only then reads the two-bit payload. A recognized future file/page
version produces the owning unsupported-format result. A supported-v1 framing/checksum,
identity, range, or reserved-zero defect produces the owning corruption/open/recovery
failure. The valid semantic code `INVALID` is neither corruption nor an unsupported format.

The test oracle independently recomputes:

```text
payload bytes       = 8192 - 32 = 8160
entries per byte    = 4
entries per page    = 8160 * 4 = 32640

ordinal             = txn_id - FIRST_NORMAL_TXN_ID
status_page_no       = 1 + ordinal / 32640
entry_in_page        = ordinal % 32640
payload_byte_index   = entry_in_page / 4
two_bit_index        = entry_in_page % 4
page_byte_offset     = 32 + payload_byte_index
bit_shift            = 2 * two_bit_index
```

All subtraction, division, addition, and PageNo/file-offset conversion uses checked
mathematical arithmetic. Reserved TxnIds `0` and `1`, values above the maximum normal TxnId,
and any result outside the physical PageNo domain are rejected before page access.

The two-bit test is the Cartesian product of every status code and position:

| Position | Shift | Mask | Codes encoded/decoded independently |
|---:|---:|---:|---|
| 0 | 0 | `0x03` | `00 INVALID`, `01 COMMITTED`, `10 ABORTED`, `11 RESERVED` |
| 1 | 2 | `0x0c` | all four codes |
| 2 | 4 | `0x30` | all four codes |
| 3 | 6 | `0xc0` | all four codes |

For each of the 16 cases, prefill the other six bits with both zero and nonzero patterns,
replace only the selected two bits, and assert that neighboring statuses remain unchanged.
Decode through an independent mask/shift oracle so production encode/decode are not each
other's oracle.

The required boundary/status mapping matrix is:

| TxnId/case | Ordinal | PageNo | Entry | Byte offset | Shift | Status fixture | Expected observation/result |
|---|---:|---:|---:|---:|---:|---|---|
| `2` | 0 | 1 | 0 | 32 | 0 | INVALID plus active registry | First normal TxnId; lookup is IN_PROGRESS |
| `3` | 1 | 1 | 1 | 32 | 2 | COMMITTED | Second bit position; lookup is COMMITTED |
| `5` | 3 | 1 | 3 | 32 | 6 | ABORTED | Fourth bit position; lookup is ABORTED |
| `6` | 4 | 1 | 4 | 33 | 0 | RESERVED | Next payload byte; recognized nonterminal result |
| `32640` | 32638 | 1 | 32638 | 8191 | 4 | INVALID plus active registry | Penultimate first-page entry; lookup is IN_PROGRESS |
| `32641` | 32639 | 1 | 32639 | 8191 | 6 | COMMITTED | Last first-page entry; lookup is COMMITTED |
| `32642` | 32640 | 2 | 0 | 32 | 0 | ABORTED | First next-page entry; extension then ABORTED lookup |
| `32643` | 32641 | 2 | 1 | 32 | 2 | RESERVED | Adjacent next-page entry; recognized nonterminal result |
| `18,446,744,073,708,503,041` | `18,446,744,073,708,503,039` | `565,157,600,297,442` | 28,799 | 7,231 | 6 | COMMITTED | Maximum legal mapping; lookup is COMMITTED |
| Normal TxnId at/above durable allocation end | formula result, if physically represented | formula result | formula result | formula result | formula result | INVALID | Unallocated/high-water-invalid; never guessed ABORTED or allocated |
| Allocated nonretired TxnId on malformed page | formula result | published mapped page | formula result | formula result | formula result | untrusted | Owning corruption/open/recovery failure before status decode |

The maximum calculation is recomputed from the §4.3.2.1 block sequence, not copied from
production constants. The oracle additionally proves status PageNo
`565,157,600,297,442` is below maximum physical PageNo
`1,125,899,906,842,622`. The universal B-1/B0/B+1 procedure remains the no-wrap and
`TXN_ID_EXHAUSTED` owner.

A freshly initialized status payload decodes as `INVALID`, but interpretation also consumes
the durable allocation high-water and runtime context:

- an entry at or above `reserved_txn_id_end` must remain `INVALID` and is not evidence of an
  allocated or aborted transaction;
- an allocated active TxnId with `INVALID` persistent bits is `IN_PROGRESS` through the
  active registry, not ABORTED;
- an allocated, nonactive status-dependent reference with only `INVALID` or `RESERVED`
  after completed recovery is the §9.11.1/§9.13 invariant failure, not a guessed outcome;
- `RESERVED` is recognized v1 nonterminal state, never COMMITTED or ABORTED and never an
  unknown-format value.

### Snapshot registration, representation, and lifetime

The BEGIN/snapshot harness pauses a new transaction immediately before and after TxnId issue
and active-registry publication, while another transaction pauses at high-water capture and
active-set capture under §9.8's shared synchronization. It forces these orders:

1. **Registration wins.** The new nonterminal TxnId is below captured `xmax` and appears in
   `active` unless it is the owner.
2. **Capture wins.** The new TxnId is assigned at or above the captured `xmax`; it need not
   appear in `active` and is too new for that snapshot.

For an ordinary writable BEGIN, instrumentation also asserts that registration, rather than
a persistent RESERVED/IN_PROGRESS status mutation, represents the new nonterminal
transaction. The valid status page, WAL end, and file bound remain unchanged by BEGIN.

The explicit forbidden oracle is:

```text
txn_id < snapshot.xmax
and transaction was nonterminal at capture
and txn_id != snapshot.owner_txn_id
and txn_id not in snapshot.active
```

This state fails the test because it would let a live transaction fall through both the
active and too-new classifications. The synchronization may be held only for the capture or
registration bookkeeping; instrumentation also asserts it is released before statement
execution.

Snapshot representation uses these canonical deterministic fixtures:

| Case | Controlled state at capture | Expected `active` | Expected `xmin` | Stability oracle |
|---|---|---|---:|---|
| No other active | owner 50, `xmax=60` | `[]` | 60 | Later BEGIN/terminal events do not alter fields |
| One lower active | active 20, owner 50, `xmax=60` | `[20]` | 20 | Membership retained after 20 later commits |
| Multiple active | 30, 10, 40; owner 50; `xmax=60` | `[10,30,40]` | 10 | Sorted vector remains immutable |
| Owner would be minimum | owner 10, other active 30, `xmax=60` | `[30]` | 30 | SELF is represented only by owner/command fields |
| Sparse TxnIds | active 2 and 1,048,578; owner 500; larger `xmax` | `[2,1048578]` | 2 | Reservation gaps create no synthetic members |
| BEGIN before capture | new 40 registered, owner 50, `xmax>40` | includes 40 | ordered minimum | Registration-wins oracle |
| Capture before BEGIN | captured `xmax=60`, new TxnId `>=60` | excludes new | unchanged | High-water-wins oracle |
| Terminal before capture | terminal cache/state published first | excludes terminal TxnId | from remaining set | Lookup observes terminal result |
| Terminal after capture | TxnId nonterminal at capture | retains TxnId | captured value | No retroactive removal |

The architecture requires the sorted-vector representation but not one lookup routine. Tests assert exact
sorted membership, owner exclusion, snapshot stability, and §9.7.3 `xmin`; they do not
require `std::binary_search` or any other production helper. Alternative high-concurrency
active-set representations are outside the test obligation.

After capture, controlled BEGIN, COMMIT, and ABORT operations mutate the live registry while
the captured `xmax`, `active`, and `xmin` remain byte/value stable. For REPEATABLE READ, only
the architecture-authorized `command_id` boundary changes between statements.

Isolation-identity cases begin transactions with no isolation clause, explicit READ
COMMITTED, explicit REPEATABLE READ, and each deferred §9.5 mode. They assert that omission
selects READ COMMITTED, the two supported identities reach the matching procedures below,
and deferred modes are rejected rather than silently mapped to a supported level. Public
metadata and diagnostics identify REPEATABLE READ as snapshot isolation and never as
SERIALIZABLE. The existing Isolation Tests remain the owner of behavioral visibility and
write-skew outcomes.

READ COMMITTED procedures hold one registered statement snapshot for the complete attempt,
including scans and result production. SUCCESS and
`FAILED_TRANSACTION_REMAINS_ACTIVE` unregister it and consume the CommandId; the next
statement obtains a fresh snapshot. A permitted §15.7 pre-write retry unregisters the old
attempt snapshot, captures a fresh snapshot, and retains the same logical CommandId. A
post-write conflict follows MUST_ABORT/ABORT and cannot reuse the snapshot.

REPEATABLE READ procedures assert no transaction snapshot exists merely because BEGIN
returned. The first ordinary statement captures and registers it; later statements retain
the same `xmax`, `active`, `xmin`, and owner while advancing the command boundary. It remains
registered through terminal transaction cleanup. Snapshot registration is observed by the
§14.2 global horizon: early unregister is a failure, and terminal/statement cleanup must not
leak a registration. SQL snapshot lifetime remains distinct from §14.6's RID read-epoch
guard.

### Terminal publication versus snapshot capture

For COMMIT and ABORT separately, pause immediately before and after §9.14.1's atomic runtime
terminal publication and race a snapshot capture through the same synchronization domain.
Durable WAL/status prerequisites are arranged by the existing COMMIT/ABORT procedures; this
test observes only Chapter 9's cache/state/registry linearization.

| Race order | Expected snapshot/lookup result | Forbidden result |
|---|---|---|
| COMMITTED publication completes before capture | TxnId absent from `active`; terminal cache returns COMMITTED | Absent from active while lookup remains IN_PROGRESS |
| Capture completes before COMMITTED publication | Nonterminal TxnId `<xmax` remains captured in `active`, even after later commit | Retroactive removal from captured snapshot |
| ABORTED publication completes before capture | TxnId absent from `active`; terminal cache returns ABORTED | New snapshot records terminal TxnId as active |
| Capture completes before ABORTED publication | TxnId remains in captured `active`; later status lookup may return ABORTED | Captured membership mutates after abort |

At publication, the harness can retain a test observation of the old active-registry entry
while the runtime terminal cache is already installed. Direct lookup must return the cache's
COMMITTED/ABORTED value. This catches stale registry state overriding a terminal result.
Logical locks/gates remain unavailable to competitors until terminal publication; their
detailed waits remain Chapter-11 verification.

### Transaction-status lookup precedence

Use canonical valid pages and independently control each runtime source. The mandatory
precedence matrix is:

| Case | FROZEN | SELF | Terminal cache | Active registry | Below retired cutoff | Persisted bits | Expected | Forbidden |
|---|---:|---:|---|---|---:|---|---|---|
| Frozen sentinel | yes | no | any | any | any | inaccessible | COMMITTED/frozen result | Page access |
| Current transaction | no | yes | none | active | no | INVALID/RESERVED | SELF | Persistent status overriding self |
| Cached commit with stale active | no | no | COMMITTED | stale active | no | any older value | COMMITTED | IN_PROGRESS |
| Cached abort with stale active | no | no | ABORTED | stale active | no | any older value | ABORTED | IN_PROGRESS/COMMITTED |
| Active ordinary TxnId | no | no | none | active | no | INVALID | IN_PROGRESS | Guessed ABORTED |
| Retired history | no | no | none | absent | yes | page may be punched | RETIRED | Missing-page corruption or guessed COMMITTED |
| Persisted commit | no | no | none | absent | no | COMMITTED | COMMITTED | IN_PROGRESS |
| Persisted abort | no | no | none | absent | no | ABORTED | ABORTED | COMMITTED |
| Persisted INVALID | no | no | none | absent | no | INVALID | nonterminal/invariant result defined by context | Guessed terminal result |
| Persisted RESERVED | no | no | none | absent | no | RESERVED | recognized nonterminal/invariant result | Corruption or terminal result |

RETIRED is tested only after the Chapter-14 status-history protocol durably publishes the
cutoff and proves every surviving persistent correctness object status-independent. A
missing or punched page without that proof is never synthesized as RETIRED. FROZEN and SELF
tests instrument storage access and require no status-page fetch. Persisted terminal tests
have valid owner/checksum/PageNo bytes and no earlier runtime source.

### Status-page extension, failure, and recovery

The first terminal status mapped to an unpublished TXN_STATUS PageNo uses the generic
PAGE_INIT and §12.10.5 protocol. The harness records private page construction, PAGE_INIT
append, file-bound publication, terminal-record append, status-bit installation, and
writeback eligibility. Before bound publication ordinary lookup cannot fetch the page; after
publication the page is canonical and its first terminal update follows PAGE_INIT in WAL.

Run the same procedure at the last entry of status page `N` and first entry of `N+1`.
The former performs no extension; the latter selects exactly the next absolute PageNo,
zero-initializes all 8,160 payload bytes to INVALID, and changes only the mapped two bits.

| Case | Bound/WAL/page fixture | Injected boundary | Required outcome | READY? |
|---|---|---|---|---:|
| First status page | Bound contains only page 0 | PAGE_INIT then first terminal record | Page 1 publishes once; terminal bit and page_lsn follow WAL order | after valid completion |
| Existing-page last entry | Page N published | Last mapped two bits | No extension; neighboring bits unchanged | yes |
| Next-page first entry | N published, N+1 absent | PAGE_INIT for N+1 | Exact next PageNo; zero payload then terminal bit | after valid completion |
| Known pre-PAGE_INIT failure | Old bound and no authorizing append | Reservation/construction/known append failure | Exact old file/bound/frame/WAL state; private page invisible | yes if generic rollback succeeds |
| Post-authorizing failure | Valid PAGE_INIT or terminal append may exist | Runtime publication/status install failure | Retained completion/recovery or DATABASE_NONCONTINUABLE; no rollback-and-continue | no ordinary continuation until resolved |
| Durable COMMIT, stale page | Commit in durable valid WAL prefix | Crash before status/data flush | Reconstruct/redo COMMITTED | yes after repair |
| Durable ABORT, stale page | Abort terminal record survives | Crash before status flush | Reconstruct/redo ABORTED | yes after repair |
| Torn status page | Bad checksum plus retained image/terminal WAL | Recovery page read | Ignore stored bits/LSN; restore image then terminal redo | yes after valid repair |
| Unrecoverable malformed page | Bad page and no required retained reconstruction base | Recovery | Exact corruption/RECOVERY_FAILED result; no guessed statuses | no |
| Active writer at crash | No surviving terminal COMMIT/ABORT | Analysis/loser resolution | ABORTED through canonical recovery terminal protocol | yes after loser completion |

Crash prefixes cover PAGE_INIT append, terminal append, status-bit installation, page flush,
and client acknowledgement. A durable COMMIT with stale/unflushed status, heap, and index
pages recovers COMMITTED. A crash after durable COMMIT but before client acknowledgement
also recovers COMMITTED and records client knowledge as uncertain. A stale status image never
overrides terminal WAL; a torn page contributes neither trusted bits nor `page_lsn`.

### Read-only transaction specialization

Read-only verification reuses ordinary transaction admission and snapshot procedures while
asserting the no-terminal-WAL/status specialization:

| Phase | TxnId | Active registry | Snapshot/horizon | Terminal WAL | Terminal status entry | Required result |
|---|---:|---:|---|---:|---:|---|
| BEGIN | yes | yes | none until owning statement boundary | no | no | ACTIVE transaction participates in registration race |
| READ COMMITTED statement | yes | yes | registered statement snapshot | no | no | Same capture/lifetime rules as writable RC |
| REPEATABLE READ first statement | yes | yes | transaction snapshot registered | no | no | Snapshot begins here, not merely at BEGIN |
| REPEATABLE READ later statement | yes | yes | same membership/horizon, new command boundary | no | no | Long-lived snapshot may hold global horizon |
| Successful completion | yes | removed at runtime terminal cleanup | snapshot unregistered | no | no | Successful transaction result after cleanup |
| Abort/failure | yes | removed only through runtime abort cleanup | snapshot unregistered | no persistent terminal record required | no | No leaked active/snapshot ownership |
| Long-running reader | yes | yes | registered RR/statement snapshot | no | no | §14.2 reclamation remains blocked where required |
| Crash while active | durably reserved identity | process-local registry disappears | no surviving snapshot | no | no | No invented durable read-only commit/status fact |

The long-running case cross-checks the Vacuum and Reclamation Tests: version/status history
needed by the registered snapshot is retained. It does not equate the SQL snapshot with a
§14.6 ReadEpochGuard. Durable reservation still prevents TxnId reuse even though no terminal
record exists.

### Chapter-9 procedural matrices

The error/result matrix keeps semantic status codes, runtime results, and failures distinct:

| Condition | Expected category | Detailed owner |
|---|---|---|
| Exact next TxnId block unavailable | `TXN_ID_EXHAUSTED` | Numeric Exhaustion — TxnId terminal block |
| Statement after final CommandId | `COMMAND_ID_EXHAUSTED` | Numeric Exhaustion — CommandId specialization |
| Persisted `INVALID` | Valid nonterminal status code interpreted with runtime/high-water/recovery context | §§9.11–9.13 procedures above |
| Persisted `RESERVED` | Recognized v1 nonterminal status | RESERVED and lookup procedures above |
| Active registry winner | `IN_PROGRESS` runtime result | Lookup precedence |
| Proven retired history | `RETIRED` runtime result | Lookup precedence plus Chapter 14 |
| Malformed supported-v1 page | Owning corruption/open/recovery failure | Status-byte validation |
| Recognizable future format | Owning unsupported-format result | §4.14 dispatch tests |
| Known terminal-WAL failure | Exact retained-state retry/failure outcome | COMMIT/ABORT Fault-Injection Tests |
| Uncertain append/publication | `DATABASE_NONCONTINUABLE` | Non-Crash WAL/MTR and §39.1 |
| Recoverable effect-free statement failure | `FAILED_TRANSACTION_REMAINS_ACTIVE` | Statement Failure tests |
| Transaction-fatal statement | `FAILED_TRANSACTION_MUST_ABORT` and automatic abort | Statement Failure/ABORT tests |

The deterministic concurrency matrix is:

| Scenario | Barrier/linearization | Legal survivors | Forbidden survivor | Oracle/owner |
|---|---|---|---|---|
| BEGIN vs capture | §9.8 registry/high-water synchronization | registered below `xmax` and active, or assigned at/above `xmax` | below `xmax`, nonterminal, absent | Independent expected set |
| COMMIT vs capture | §9.14 runtime terminal publication | terminal-before capture, or captured active-before terminal | retroactive active removal | Terminal race matrix |
| ABORT vs capture | §9.14 runtime terminal publication | terminal-before capture, or captured active-before terminal | retroactive active removal | Terminal race matrix |
| Cache vs stale registry | terminal cache publication observed first | COMMITTED/ABORTED cache result | IN_PROGRESS | Lookup precedence |
| RC attempt vs next statement | snapshot unregister/next capture | stable old attempt, fresh next snapshot | membership mutation or snapshot reuse | RC lifetime procedure |
| RR snapshot vs later commit | first-statement capture | original active/horizon retained | later commit added to visible past | RR lifetime plus Chapter 10 |
| Unregister vs reclamation | snapshot registry removal | no reclaim before legal unregister | early horizon advance | Vacuum/reclamation cross-check |
| Shutdown vs BEGIN | READY→DRAINING admission gate | admitted-before drain or rejected-after gate | new transaction after gate | Database Lifecycle Tests |

The high-level domain/case matrix is:

| Domain | Deterministic fixture | Barrier/fault | Independent oracle | Architecture |
|---|---|---|---|---|
| TxnId reservation | Exact block/high-water fixture | control write/sync/issue | durable exclusive end | §§4.3.2.1, 9.2–9.3 |
| TxnId exhaustion | Terminal exact block | next reservation | mathematical block sequence | §§4.3.2.1, 9.3 |
| CommandId | 0/max/failed/retry fixture | statement admission/end | checked command sequence | §§4.3.2.2, 9.6 |
| Status bytes | Canonical page vectors | one-field/bit mutation | independent byte/mask codec | §§4.13.6, 9.11–9.12 |
| Status mapping | Boundary TxnIds | page/bit access | mathematical formula | §9.12 |
| BEGIN/capture | Two controlled transactions | shared registry/high-water sync | expected ordered set | §9.8 |
| Snapshot representation | Empty/owner/sparse/multiple sets | capture | set/sort/min oracle | §§9.7–9.8 |
| Isolation identity | Default/RC/RR/deferred modes | transaction admission/diagnostic | exact admitted identity | §9.5 |
| RC lifetime | Two statements and retry | register/unregister | snapshot identity/event log | §9.9 |
| RR lifetime | First/later statements | first capture/terminal cleanup | fixed fields plus command boundary | §9.10 |
| COMMIT/capture | Durable writer and capturer | §9.14 publication | barrier order | §§9.14.1–9.14.2 |
| ABORT/capture | Aborting writer and capturer | §9.14 publication | barrier order | §§9.14.1, 9.14.3 |
| Lookup precedence | Controlled runtime/persistent sources | each precedence stage | explicit matrix | §9.13 |
| Status PAGE_INIT | First/next status page | append/publication faults | old/new bound and WAL order | §§9.12, 12.10.5 |
| Stale status vs WAL | Durable terminal records, stale page | crash/reopen | durable WAL model | §§12.10.5, 13.13.2–13.17 |
| Torn status | Bad checksum plus retained image | recovery | image plus terminal redo | §§13.14, 13.17 |
| Active loser | No terminal record | crash/analysis | WAL terminal evidence | §13.15 |
| Read-only | RC/RR/no-write transactions | completion/crash | event and WAL/status absence | §9.15 |
| Reclamation horizon | Long-lived read-only snapshot | vacuum/horizon query | registered snapshot set | §§14.2, 14.14 |

### Chapter 9 architecture-obligation coverage map

Each obligation requires a
deterministic procedure in this section or a precise existing owner below; an architecture
statement by itself is not counted as verification.

| # | Domain | Atomic obligation | Architecture owner | Verification procedure/reference |
|---:|---|---|---|---|
| 1 | TXNID RESERVATION | TxnId width and reserved sentinel values | §§4.3.2.1, 9.2 | Numeric Exhaustion — TxnId domain/boundaries |
| 2 | TXNID RESERVATION | First normal value and monotonic allocation | §§4.3.2.1, 9.2 | Numeric Exhaustion — TxnId domain/boundaries |
| 3 | TXNID RESERVATION | Fixed `2^20` reservation block | §§4.3.2.1, 9.3 | Numeric Exhaustion — exact-block procedure |
| 4 | TXNID RESERVATION | Reservation stores an exclusive durable end | §§4.3.2.1, 9.3 | Numeric Exhaustion — exact-block procedure |
| 5 | TXNID RESERVATION | No issue before durable high-water; known failure issues none | §9.3 | Numeric Exhaustion — reservation failure procedure |
| 6 | TXNID RESERVATION | Crash gaps are legal and reserved TxnIds are never reused | §9.3 | Numeric Exhaustion — restart/nonreuse procedure |
| 7 | TXNID EXHAUSTION | Maximum normal TxnId, no wrap, deterministic next-allocation failure | §§4.3.2.1, 9.3 | Numeric Exhaustion — terminal exact block |
| 8 | RECOVERY / STATUS RECONCILIATION | Torn control slots recover a valid durable TxnId high-water | §§9.3, 13.2 | Numeric Exhaustion — control-slot crash matrix |
| 9 | COMMANDID | CommandId is uint32 and zero is the first legal value | §§4.3.2.2, 9.6 | Numeric Exhaustion — CommandId specialization |
| 10 | COMMANDID | One CommandId is assigned per admitted logical statement | §9.6 | Numeric Exhaustion — statement-boundary sequence |
| 11 | COMMANDID | Permitted pre-write retry reuses the same logical CommandId | §§9.6, 15.7 | Numeric Exhaustion — retry specialization |
| 12 | COMMANDID | Success and recoverable failure consume the CommandId | §§9.6, 39.1 | Numeric Exhaustion plus Statement Failure tests |
| 13 | COMMANDID | `UINT32_MAX` is legal and the next statement is rejected without successor arithmetic | §§4.3.2.2, 9.6 | Numeric Exhaustion — terminal boundary |
| 14 | COMMANDID | Transaction-control operations do not consume ordinary CommandIds | §9.6 | Numeric Exhaustion — control-operation specialization |
| 15 | TRANSACTION STATE | Runtime state domain is exact | §9.4 | Statement Failure and Transaction-State Tests |
| 16 | TRANSACTION STATE | ACTIVE admits statements, commit, and abort as specified | §9.4 | Transaction-State Tests |
| 17 | TRANSACTION STATE | MUST_ABORT rejects statements/commit and admits abort | §9.4 | Statement Failure and Transaction-State Tests |
| 18 | TRANSACTION STATE | COMMITTING/ABORTING reject new statements and retain terminal ownership | §§9.4, 9.14 | COMMIT/ABORT Fault-Injection Tests |
| 19 | TRANSACTION STATE | Every legal lifecycle edge and illegal edge is enforced | §9.4 | Transaction-State Tests transition matrix |
| 20 | COMMIT | Authorizing commit append forbids transition to ABORTING/ABORTED | §§9.4, 9.14.2 | COMMIT Fault-Injection Tests |
| 21 | TRANSACTION STATE | COMMITTED and ABORTED are terminal and irreversible | §§3.3.6, 9.4 | Lifecycle plus COMMIT/ABORT tests |
| 22 | FAILURE / NONCONTINUABLE | Ordinary transaction admission requires READY | §§3.3, 9.4 | Database Lifecycle Tests |
| 23 | FAILURE / NONCONTINUABLE | DRAINING/NONCONTINUABLE transaction handling preserves terminal facts | §§3.3.5–3.3.6, 9.16, 39.1 | Database Lifecycle and Non-Crash WAL/MTR tests |
| 24 | BEGIN / REGISTRATION | TxnId issue, active registration, and snapshot high-water share the required synchronization | §§9.3, 9.8 | Snapshot registration race harness |
| 25 | SNAPSHOT REPRESENTATION | Snapshot contains exactly `xmax`, `active`, `xmin`, owner, and command fields | §9.7 | Snapshot representation matrix |
| 26 | SNAPSHOT REPRESENTATION | `xmax` is the captured next-unassigned normal TxnId | §§9.7.1, 9.8 | BEGIN/capture two-order procedure |
| 27 | SNAPSHOT REPRESENTATION | `active` contains every relevant normal nonterminal TxnId below `xmax` | §§9.7.2, 9.8 | Snapshot representation and forbidden-gap oracle |
| 28 | SNAPSHOT REPRESENTATION | Snapshot owner is excluded from `active` | §§9.7.2, 9.8 | Owner-exclusion fixture |
| 29 | SNAPSHOT REPRESENTATION | ACTIVE, MUST_ABORT, COMMITTING, and ABORTING are snapshot-active | §§9.4, 9.7.2 | Controlled-state membership fixture |
| 30 | SNAPSHOT REPRESENTATION | A captured nonterminal remains in `active` after later terminal publication | §§9.7.2, 9.14 | Terminal/capture race matrix |
| 31 | SNAPSHOT REPRESENTATION | `active` is a sorted vector | §9.7.2 | Sorted empty/owner/sparse/multiple fixtures |
| 32 | SNAPSHOT REPRESENTATION | Membership implementation may vary without changing sorted-vector semantics | §9.7.2 | Representation-freedom assertion |
| 33 | SNAPSHOT REPRESENTATION | Nonempty `xmin` is the minimum captured active TxnId | §9.7.3 | Xmin multi-member/owner fixtures |
| 34 | SNAPSHOT REPRESENTATION | Empty `active` gives `xmin=xmax` | §9.7.3 | Empty-set fixture |
| 35 | SNAPSHOT REPRESENTATION | Owner and command fields drive self visibility without self membership | §9.7.4 | Owner fixture plus CommandId cross-reference |
| 36 | SNAPSHOT CAPTURE | High-water and active-set capture are one atomic observation | §9.8 | BEGIN/capture deterministic barriers |
| 37 | SNAPSHOT CAPTURE | No below-`xmax` nonterminal nonowner can be absent from `active` | §9.8 | Explicit forbidden-classification oracle |
| 38 | SNAPSHOT CAPTURE | Capture synchronization is not held during query execution | §9.8 | Harness lock-ownership observation |
| 39 | SNAPSHOT CAPTURE | Captured membership/horizons are immutable | §§9.7–9.10 | BEGIN/COMMIT/ABORT post-capture mutation fixture |
| 40 | RECLAMATION / HORIZON | Registered SQL snapshots contribute to the global snapshot horizon | §§9.9–9.10, 14.2 | Snapshot registration plus Vacuum/Reclamation tests |
| 41 | SNAPSHOT LIFETIME | READ COMMITTED captures a fresh snapshot for each statement attempt | §9.9 | Two-statement RC fixture |
| 42 | SNAPSHOT LIFETIME | RC snapshot remains stable through the complete attempt | §9.9 | In-attempt registry mutation fixture |
| 43 | SNAPSHOT LIFETIME | RC success/recoverable failure unregisters snapshot and consumes command | §§9.6, 9.9 | RC event-log cleanup fixture |
| 44 | SNAPSHOT LIFETIME | Allowed pre-write retry refreshes snapshot but reuses logical CommandId | §§9.9, 15.7 | RC retry fixture |
| 45 | SNAPSHOT LIFETIME | Post-write conflict does not reuse the attempt snapshot | §§9.9, 15.7 | RC conflict/abort cross-reference |
| 46 | SNAPSHOT LIFETIME | REPEATABLE READ captures on first ordinary statement, not BEGIN | §9.10 | RR BEGIN/first-statement fixture |
| 47 | SNAPSHOT LIFETIME | RR retains membership/horizon/owner fields across statements | §9.10 | RR multi-statement fixture |
| 48 | SNAPSHOT LIFETIME | RR updates only the command boundary per statement | §§9.6, 9.10 | RR fixture plus CommandId procedure |
| 49 | SNAPSHOT LIFETIME | RR registration remains until terminal cleanup | §§9.10, 14.2 | RR terminal/unregister horizon fixture |
| 50 | STATUS FILE LAYOUT | `txn_status.dat` superblock identity is canonical singleton metadata | §§4.10, 4.13.6, 9.12 | TXN_STATUS superblock specialization matrix |
| 51 | STATUS FILE LAYOUT | Status data page uses common header, PageType 7, and no specialized header | §§4.13.6, 9.12 | Data-page framing mutation matrix |
| 52 | STATUS FILE LAYOUT | Payload is 8,160 bytes and capacity is 32,640 entries | §§4.13.6, 9.12 | Independent capacity arithmetic |
| 53 | STATUS FILE LAYOUT | All four two-bit encodings and four bit positions round-trip independently | §§4.13.6, 9.11–9.12 | 16-case mask/shift matrix |
| 54 | STATUS FILE LAYOUT | RESERVED is recognized nonterminal v1 state | §§9.11–9.13 | RESERVED fixture and lookup matrix |
| 55 | STATUS MAPPING | Reserved TxnIds have no ordinary status ordinal | §9.12 | Checked mapping-domain rejection |
| 56 | STATUS MAPPING | Normal ordinal is `txn_id-2` | §9.12 | Independent mapping oracle |
| 57 | STATUS MAPPING | Absolute page and entry mapping use 32,640-entry pages | §9.12 | Boundary mapping matrix |
| 58 | STATUS MAPPING | Payload byte and least-significant-first shift are exact | §9.12 | Four-position encoding matrix |
| 59 | STATUS MAPPING | Status mutation preserves all neighboring two-bit entries | §§9.12, 12.10.5 | Nonzero-neighbor mutation fixtures |
| 60 | STATUS MAPPING | Checked maximum mapping fits the physical PageNo domain | §§4.3.2.1, 9.12 | Maximum-TxnId independent arithmetic |
| 61 | STATUS FILE LAYOUT | Newly initialized payload is canonical INVALID | §§9.11–9.12, 12.10.5 | PAGE_INIT zero-payload fixture |
| 62 | STATUS FILE LAYOUT | Owner, bound, version, checksum, identity, and reserved bytes validate before payload | §§4.10, 4.13.6 | Validation-order/corruption matrix |
| 63 | STATUS LOOKUP | INVALID above allocation high-water is not a terminal transaction result | §§9.11–9.13 | High-water INVALID fixtures |
| 64 | RECOVERY / STATUS RECONCILIATION | Status `page_lsn`/images accelerate reconstruction but terminal WAL is authoritative | §§9.11.1, 12.10.5, 13.17 | Status extension/recovery matrix |
| 65 | STATUS LOOKUP | FROZEN resolves without status-page access | §9.13 | FROZEN precedence fixture |
| 66 | STATUS LOOKUP | Current transaction resolves SELF before persistent state | §9.13 | SELF precedence fixture |
| 67 | STATUS LOOKUP | Runtime terminal cache precedes active and persistent sources | §§9.13–9.14 | Terminal-cache fixture |
| 68 | STATUS LOOKUP | Terminal cache wins over an intentionally stale active observation | §§9.13–9.14 | Cache-versus-stale-active race |
| 69 | STATUS LOOKUP | Active ordinary transaction resolves IN_PROGRESS | §9.13 | Active/INVALID fixture |
| 70 | STATUS LOOKUP | RETIRED requires a durably published Chapter-14 retirement proof | §§9.13, 14.14 | RETIRED/punched-page fixture |
| 71 | STATUS LOOKUP | Persisted COMMITTED and ABORTED decode after earlier sources miss | §9.13 | Valid persisted-terminal fixtures |
| 72 | STATUS LOOKUP | INVALID and RESERVED never produce guessed terminal results | §§9.11–9.13 | Lookup precedence matrix |
| 73 | STATUS LOOKUP | Missing referenced, nonretired status produces the owning invariant/corruption result | §§9.11.1, 9.13 | Missing-page/no-retirement fixture |
| 74 | TERMINAL PUBLICATION | Snapshot capture and terminal publication use one synchronization domain | §9.14.1 | COMMIT/ABORT capture race barriers |
| 75 | TERMINAL PUBLICATION | Cache install, terminal state, and active removal are one atomic publication | §9.14.1 | Event-log atomicity assertion |
| 76 | TERMINAL PUBLICATION | A new snapshot excludes a published terminal TxnId while old snapshots stay immutable | §9.14.1 | Terminal-before/after-capture fixtures |
| 77 | TERMINAL PUBLICATION | Logical locks/gates release only after terminal publication | §§9.14.1, 11.2 | Terminal-race assertion plus Locking Tests |
| 78 | COMMIT | Durable COMMIT C3 precedes runtime publication C4 | §9.14.2 | COMMIT C0–C6 Fault-Injection Tests |
| 79 | COMMIT | Resident status bits alone do not publish runtime commit | §§9.11.1, 9.14.2 | Status/capture event-order fixture |
| 80 | COMMIT | Durable COMMIT is irreversible despite later failure or lost acknowledgement | §§3.3.6, 9.14.2 | COMMIT faults plus crash-before-ack fixture |
| 81 | ABORT | Runtime ABORTED publication precedes logical lock release | §9.14.3 | ABORT A0–A4 plus terminal-race fixture |
| 82 | ABORT | Ordinary abort does not require immediate WAL sync | §9.14.3 | ABORT Fault-Injection Tests |
| 83 | ABORT | Crash before durable abort is resolved as a loser, not guessed from status bytes | §§9.14.3, 13.15 | Recovery loser fixture |
| 84 | ABORT | Abort requires no synchronous physical heap/index undo | §9.14.3 | ABORT tests plus MVCC/Reclamation cross-check |
| 85 | READ-ONLY TRANSACTION | Read-only transactions use normal identity/registry/snapshots but no ordinary terminal WAL/status | §9.15 | Read-only phase matrix |
| 86 | READ-ONLY TRANSACTION | Read-only success, abort/failure, and crash release runtime ownership without invented persistence | §9.15 | Read-only completion/crash fixtures |
| 87 | STATUS MAPPING | First/new status pages use PAGE_INIT and publish bounds before lookup | §§9.12, 12.10.5 | Status-page extension matrix |
| 88 | RECOVERY / STATUS RECONCILIATION | Durable COMMIT overrides stale/unflushed status and data pages | §§9.14.2, 13.13.2–13.17 | Durable-COMMIT stale-page crash fixture |
| 89 | RECOVERY / STATUS RECONCILIATION | Durable ABORT overrides stale status bytes | §§9.14.3, 13.15–13.17 | Durable-ABORT stale-page fixture |
| 90 | RECOVERY / STATUS RECONCILIATION | Torn status pages are reconstructed from valid images/WAL or recovery fails | §§13.14, 13.17 | Torn/unrecoverable page fixtures |
| 91 | RECOVERY / STATUS RECONCILIATION | Active transaction without terminal record becomes recovery loser | §13.15 | Active-at-crash fixture |
| 92 | FAILURE / NONCONTINUABLE | READY follows transaction-status/WAL reconciliation and loser completion | §§3.3, 13.19 | Page extension/recovery matrix plus Lifecycle Tests |
| 93 | CROSS-OWNER VISIBILITY | Chapter 10 receives stable snapshot, owner, command, and status inputs | §§9.7–9.14, 10.2–10.3 | This section plus MVCC Visibility Tests |
| 94 | CROSS-OWNER VISIBILITY | Chapter 11 remains owner of write/write and transactional uniqueness conflicts | §§9.14.1, 11.2–11.4 | Terminal input assertion plus Locking/UNIQUE Tests |
| 95 | RECLAMATION / HORIZON | SQL snapshot horizon, RID read epoch, and status retirement remain distinct and coordinated | §§9.9–9.10, 14.2, 14.6, 14.14 | Snapshot lifetime plus Vacuum/Reclamation Tests |
| 96 | OTHER | READ COMMITTED and REPEATABLE READ are the supported identities and READ COMMITTED is the default | §9.5 | Isolation-identity cases plus Isolation Tests |
| 97 | OTHER | REPEATABLE READ is identified as snapshot isolation, never SERIALIZABLE | §9.5 | Isolation-identity diagnostics plus write-skew case |
| 98 | OTHER | Deferred isolation/concurrency modes are not silently admitted as modes | §9.5 | Isolation-identity rejection cases |
| 99 | CROSS-OWNER VISIBILITY | WAL/durability and recovery mechanics remain owned by Chapters 12–13 while Chapter 9 supplies lifecycle inputs | §9.1 | COMMIT/ABORT Fault-Injection and Recovery Property Tests |
| 100 | BEGIN / REGISTRATION | Ordinary BEGIN uses runtime registration and does not require persisted RESERVED/IN_PROGRESS status | §§9.8, 9.11, 9.11.1 | BEGIN event-log/status-absence fixture |
| 101 | TRANSACTION STATE | Required transaction-local semantic resources remain observable through their owning lifetime and cleanup | §9.4 | Harness resource log plus Statement, COMMIT, ABORT, and Lifecycle Tests |


---

## WAL Persistent Codec, Append, and Recovery Verification

This section is the detailed procedural owner for the byte, append, durability,
tail, and transaction-status WAL obligations in [`ARCHITECTURE.md`](../ARCHITECTURE.md)
Chapter 12. Numeric Exhaustion and Terminal-Boundary Verification remains the
owner of mathematical end-of-domain and terminal-credit cases; Buffer management
verification owns copied page writeback; B+ Tree Verification owns structural
old-or-complete-new outcomes; the COMMIT, ABORT, lifecycle, checkpoint, and
recovery sections own their existing cross-layer consequences. The procedures
below verify Chapter 12 without redefining those contracts.

### Deterministic WAL harness and independent oracles

The harness records architecture-level events without requiring a production
class or function name:

```text
segment create requested / final name created / exact size established
segment fdatasync completed / wal-directory fsync completed
candidate reservation proposed / private record encoded / CRC and padding finalized
valid append end published / transaction last_wal_lsn updated
page/frame metadata published / physical WAL write begins or partially completes
durability target requested / segment fdatasync completes / durable_lsn advances
waiter released with success or exact failure
recovery inventory begins / header and payload decode / CRC validation
record accepted or rejected / valid tail established / READY published
```

Every concurrency or crash-prefix fixture uses a barrier, explicit scheduler
gate, injected exact transfer result, or directly constructed persisted prefix.
Elapsed sleeps, timing luck, and repeated random crashes are not correctness
oracles. Random crash/property testing remains complementary.

Four independent test-side models are mandatory:

1. **Byte oracle.** Construct expected little-endian bytes from mathematical
   field values and the architecture tables. Independently calculate header,
   payload, `total_length`, `physical_span`, zero padding, and nested lengths.
   Production encode followed by production decode is not sufficient.
2. **CRC oracle.** Compute CRC32C with a test-side implementation or fixed
   reference vectors over the 48-byte header with bytes `44..47` treated as zero
   plus the exact payload. Alignment padding is excluded and checked separately.
3. **LSN oracle.** Use widened arithmetic for Align8, segment index/offset,
   exclusive end, record placement, and terminal `2^64` reasoning. Production
   reservation helpers are not the oracle.
4. **Recovery-prefix oracle.** Build the expected valid prefix from independently
   accepted complete records and filesystem namespace facts, not from the
   runtime append result or precrash API status.

Each negative fixture is canonical except for one selected defect. A payload
grammar test retains valid outer framing and CRC; a CRC test retains valid
framing and payload; a namespace test changes only the synchronization fact.
This isolation is essential because malformed recognized-v1 WAL, unsupported
format, torn final tail, resource failure, and uncertain runtime authority have
different outcomes.

An ownership-seam fixture instruments WAL segment positional I/O, ordinary
BufferPool page access, and the lower file-service calls. WAL append, flush, and
recovery scanning must use the bounded raw-WAL path without treating a segment
as a BufferPool page; the lower service may report exact I/O completion/failure
but cannot choose append order or advance `durable_lsn`. This verifies the
Chapter-7/12 ownership boundary without prescribing source layout.

### WAL segment namespace and creation

Construct segment-name fixtures for indexes `0`, `1`, and `2^38-1`. The
independent formatter expects exactly 16 lowercase hexadecimal digits, leading
zeros, and `.wal`. Reject shorter, uppercase, signed, decimal, alternate-suffix,
overwide, and index-`2^38` names through the owning namespace/inventory result.

For segment zero, verify bytes `0..7` are zero, append end begins at LSN `8`, and
no parser attempts to decode that prefix. For every later segment, place a record
at offset zero to prove there is no invented segment header. Every segment fixture
has exact length `67,108,864`; test one byte short, one byte long, and a legal
exact final offset without allocating the complete maximum-index namespace.

Instrument segment creation in this exact observable sequence:

```text
exact next final name created with exclusive/no-follow ownership
ftruncate establishes 67,108,864 bytes
segment fdatasync succeeds
fsync(database_root/wal) succeeds
records in the segment become eligible for a durability proof
```

Fail create, truncate, segment `fdatasync`, and directory `fsync` independently.
No failure may advance `durable_lsn` into that segment or release a durability
waiter successfully. A next-contiguous exact-size all-zero survivor is adoptable
only after zero validation and renewed segment/directory synchronization. This
procedure cross-references §§4.7, 12.2–12.2.1, and 13.10–13.11.

### WAL record framing, header, CRC, padding, and segment tails

Build one canonical record with distinctive non-palindromic values in every
multibyte field. Compare all 48 header bytes independently at offsets for
`total_length`, `header_length=48`, `record_type`, zero flags/reserved, `lsn`,
`txn_id`, `prev_txn_lsn`, `payload_length`, and `crc32c`. The same fixture
verifies every integer is little-endian and `header.lsn` equals the physical
logical start.

Mutate one dimension at a time: header length, flags, reserved, total/payload
relationship, physical-start LSN, alignment, segment containment, one covered
header bit, one payload bit, and CRC bytes. Use a non-eight-aligned total to
verify `physical_span=Align8(total_length)`, external bytes are all zero, and
those bytes affect neither `total_length` nor CRC. A nonzero external padding
byte in a complete required record is malformed recognized-v1 WAL even though
the covered CRC remains unchanged.

At a segment boundary construct:

- a record whose physical span exactly consumes the remaining bytes;
- a record that would exceed the remainder by one byte and therefore is never
  split across segments;
- at least 48 remaining bytes encoded as one valid CRC-protected `WAL_PAD` whose
  zero payload consumes the tail;
- fewer than 48 remaining bytes as a raw all-zero tail;
- a complete header-sized all-zero suffix, which is unused zero tail rather
  than a valid record because it lacks valid framing;
- nonzero garbage in the equivalent tail position.

The scanner never assembles an ordinary record across two segments. Tail
classification is completed by the recovery matrix below; this local procedure
proves the physical bytes and parser bounds in §§12.3–12.4.

Feed an independently valid WAL_PAD to analysis and redo and assert it only
advances framing to the segment boundary: it creates no page, transaction,
checkpoint, or `page_lsn` action.

### WAL PageId and per-transaction ownership codecs

Build the exact 16-byte WAL PageId independently: little-endian `file_id` at
offset 0, zero `reserved32` at 4, and little-endian `page_no` at 8. Exercise
minimum and high legal identities plus `INVALID_FILE_ID`, `INVALID_PAGE_NO`,
nonzero reserved bytes, wrong target owner, and unpublished target PageNo.
Malformed target identity must fail before redo or page publication.

For one user transaction encode records `R1`, `R2`, and a terminal record and
assert `R1.prev_txn_lsn=0`, `R2.prev_txn_lsn=R1.lsn`, and
`terminal.prev_txn_lsn=R2.lsn`. Pure system records use zero header TxnId and
previous LSN; BTREE_MTR's payload owner remains diagnostic. Also construct a
recovery-generated TXN_ABORT using the analysis-derived loser TxnId and prior
transaction LSN.

### PAGE_DELTA codec and payload rejection

The minimum positive fixture uses `patch_count=1` and one one-byte patch at a
legal page offset. Independently calculate its 24-byte prefix, 8-byte patch
header, after-image byte, payload length `33`, record total `81`, physical span
`88`, CRC, and padding. A second fixture uses multiple ordered nonoverlapping
patches, including legal page-boundary ranges, and proves exact count and length
accounting.

Construct these complete outer-framing- and CRC-valid negative fixtures
independently:

| Defect | Required observation |
|---|---|
| `patch_count=0` | malformed recognized-v1 WAL/corruption; no redo or `page_lsn` advance |
| `patch_count=1`, patch length zero | malformed recognized-v1 WAL |
| descending or repeated offsets | malformed canonical ordering |
| overlapping ranges | malformed payload |
| checked `offset+length > 8192` | malformed payload without overflow |
| overlap with common-header bytes `8..19` | malformed payload |
| count/entry mismatch | malformed payload |
| valid entries plus trailing payload byte | malformed payload |
| nonzero patch reserved field | malformed recognized-v1 WAL |

The zero-count fixture remains a complete known-type record: it is neither an
empty legal operation, unsupported type, nor torn tail. Positive `count=1`
guards against accidentally implementing `count>1`, while synthetic maximum
record arithmetic preserves the existing upper bound.

Exercise the writer boundary independently by requesting construction with
`patch_count=0`. It must reject the request before LSN reservation and emit no
record bytes; inspection of every successfully emitted PAGE_DELTA fixture must
find at least one nonempty patch. Decoder rejection is not the writer oracle.

### PAGE_INIT and PAGE_IMAGE codecs

Construct independent PAGE_INIT and PAGE_IMAGE vectors with the exact 24-byte
payload prefix followed by 8192 image bytes: payload `8216`, record total
`8264`. Verify WAL PageId, expected PageType, zero reserved field,
`image_length=8192`, image PageNo/type, image `page_lsn=record.lsn`, image
checksum, outer CRC, and complete physical span.

Mutate one image dimension at a time: FileId/PageNo owner, PageType, common
header, reserved bytes, image length, embedded `page_lsn`, and page checksum.
The test-side page builder/checksum is independent from the production WAL/page
encoder. PAGE_INIT targets a private new page and participates in bound/runtime
publication; PAGE_IMAGE replaces an already-published page. Tests never collapse
those record identities merely because their payload grammar is shared.

The existing BufferPool new-page publication and PAGE_INIT/MTR Rollback Tests
remain the detailed procedure for private extension, known pre-authorization
failure, valid append, publication failure, crash with WAL but no data-page
image, and WAL-before-data. Reuse those procedures for these handoffs.

### BTREE_MTR codec and nested-entry rejection

The minimum positive PATCH_SET fixture uses `page_count=1`, one affected-page
entry, and nested `patch_count=1`. Independently calculate the 16-byte MTR
prefix, 24-byte entry prefix, 8-byte PATCH_SET prefix, one patch header/data,
payload `57`, record total `105`, physical span `112`, outer CRC, and padding.
Separately encode one FULL_IMAGE entry with exact `data_length=8192` and a valid
image whose `page_lsn=mtr.lsn`.

Use valid outer framing and CRC for each nested negative fixture:

| Defect | Required observation |
|---|---|
| `page_count=0` | malformed recognized-v1 WAL; never an empty MTR/barrier/no-op |
| one PATCH_SET with `patch_count=0` | malformed recognized-v1 WAL |
| encoding `0` or unregistered encoding | WAL corruption, not unsupported top-level type |
| short/long `data_length` | malformed nested payload |
| nested count/length mismatch or trailing bytes | malformed nested payload |
| nonzero entry/PATCH_SET reserved field | malformed recognized-v1 WAL |
| invalid or duplicate PageId | malformed MTR membership |
| unsorted affected-page entries | malformed canonical membership |
| invalid nested patch order/overlap/bounds | malformed nested payload |
| FULL_IMAGE length other than 8192 | malformed nested payload |

Positive one-count fixtures prove the legal minimum. Synthetic count/length
builders exercise the largest complete MTR admitted by the one-segment record
bound and one unit beyond, without requiring massive resident allocations.

Exercise both writer boundaries independently: request a BTREE_MTR with zero
affected pages and request a one-page PATCH_SET with zero patches. Each request
must fail before LSN reservation and emit no record. Inspect every successfully
emitted MTR to prove it has at least one page and every PATCH_SET has at least one
nonempty patch; decoder rejection is tested separately.

For a legal multi-page fixture, entries are unique and sorted by
`(file_id,page_no)`. Every final image and redo result uses the one common MTR
LSN. The existing BTREE_MTR failure/crash/recovery and full-tree procedures own
the no-flush, exact pre-authorization rollback, post-authorization completion,
and old-or-complete-new recovery oracle; malformed/zero-membership records fail
before that semantic layer.

Associate one legal structural MTR diagnostically with a user transaction, then
abort that user. Recovery and reopen preserve the authorized structural page
shape while MVCC/status rules suppress loser-owned logical entries; no user
abort physically reverses the system MTR.

### Registry, terminal, and checkpoint record codecs

Build one independent valid vector for every record type code `0..9` and one
complete CRC-valid vector using an unknown code. The unknown record is
`UNSUPPORTED_WAL_FORMAT`, never an ignorable extension, generic corruption, or
torn tail. A recognizable future owning format uses the §4.14 unsupported-format
path; a known type with invalid v1 payload grammar uses corruption.

TXN_COMMIT and TXN_ABORT fixtures have zero payload, total length 48, normal
TxnId, and the exact transaction-chain tail. The recovery-generated abort form
uses analysis-derived ownership. Byte validity is tested separately from C3 or
ordinary ABORT durability. Read-only transaction procedures continue to require
no ordinary terminal WAL/status record.

Checkpoint vectors use the Chapter-13 owner rather than inventing Chapter-12
payload fields: exact 32-byte BEGIN, DATA's 24-byte prefix plus exact DPT/writer
arrays, and exact 32-byte END. Exercise zero DATA records, contiguous chunk
indexes, count totals, cross-linked BEGIN identities, reserved-zero fields,
redo bound, complete-sequence validation, WAL durability before control-slot
installation, and retention of required image/checkpoint records.

### Append reservation, authorization, and physical failure

Place deterministic barriers immediately before candidate reservation, after
private encoding, immediately before and after valid-end publication, and before
page/frame metadata publication. A canceled reservation or any injected
reservation, allocation, validation, or known no-append failure must leave the
valid append end, transaction `last_wal_lsn`, published page metadata, and DPT
state unchanged. The next successful append uses the canonical current end, so
the independently decoded stream has neither a hole nor an abandoned LSN.

Schedule at least two appenders at each barrier. Decode the resulting physical
prefix independently and require one total order, disjoint complete spans,
correct segment padding, and each transaction's own `prev_txn_lsn` order. The
procedure observes semantics only; it does not require a mutex, append-buffer
capacity, ring, queue, thread count, or other realization.

Use the two sides of valid-end publication as the authorization oracle:

| Controlled stop/fault | Valid end | Published page/transaction metadata | Required outcome |
|---|---:|---|---|
| before authorization | old | old | exact restoration and ordinary error are permitted by the owning protocol |
| complete bytes installed but valid end not published | old | old | private bytes are not WAL and cannot authorize publication |
| immediately after valid-end publication | advanced across the complete record | publication must complete under retained ownership | rollback-and-continue is forbidden |
| physical short write after authorization | advanced | legal published dirty state | retain exact append bytes, retry the same physical range, and keep `durable_lsn` unchanged |
| append-end/assigned-byte ownership uncertain | unknown | never released as ordinary state | canonical database-noncontinuable path |

For short-write fixtures, inject exact returned byte counts at the first byte,
inside the header, inside the payload, and inside external padding. Compare every
retry byte with the retained independent vector. A crash image contains only the
chosen persisted prefix; recovery accepts only complete records. For uncertainty,
make the harness unable to prove either valid-end state or the exact bytes assigned
to it and assert no ordinary retry, page rollback, later append, writeback, or
durability acknowledgement. These procedures specialize §12.12 and retain the
broader Non-Crash WAL/MTR Failure Injection and §39.1 consequence tests.

### Durable prefix, group commit, COMMIT, and WAL-before-data

Recovery first establishes the append end and initial durable-prefix fact from
the validated persisted stream. An empty stream begins appending at LSN `8`; the
test never interprets invalid LSN zero as a durable record.

For a single-segment request through record LSN `X`, pause after bytes are written
and again after segment `fdatasync`. The waiter cannot return and `durable_lsn`
cannot cover `X` until the complete record and every required preceding byte are
proven durable. For a cross-segment target, leave unsynchronized bytes in the
older segment and prove synchronizing only the newer file is insufficient. Then
synchronize every affected segment and permit monotonic advancement.

For a record in a newly created segment, hold `fsync(database_root/wal)` after
successful segment data synchronization. No durability waiter, COMMIT C3, or
page writeback may treat that record as durable. Release the directory barrier
and verify that namespace proof plus required segment synchronization permits the
advance. Injecting either synchronization failure preserves every independently
proven lower prefix and releases each affected waiter with the exact failure;
fatal WAL-service or shutdown failure releases all unsatisfiable waiters rather
than leaving them asleep.

Use three legal aligned commit starts, `1000`, `1104`, and `1256`, with separate
waiters. One durability operation through `1256` may release all three, but each
assertion is evaluated independently against its own target. A lower-target waiter
may return earlier only if its complete prefix is independently proven durable;
no batching cadence is required. A flush failure cannot report success for an
unproven target or falsely advance `durable_lsn`. This proves group-commit
semantics while leaving scheduling, thread count, buffering, and coalescing
mechanisms free.

The COMMIT fixture appends TXN_COMMIT at `L`, holds `durable_lsn < L`, and proves
C3 cannot complete. Once `durable_lsn >= L`, C3 may complete without a heap,
index, or status-page force. C4 runtime terminal publication, C5 resource release,
and C6 acknowledgement remain separately gated by the COMMIT Fault-Injection
Tests. Put the same record in a new segment to exercise the namespace prerequisite,
and coalesce several commits to prove each C3 remains tied to its own LSN. The
ABORT fixture preserves ordinary A2 publication without inventing synchronous
force, while WAL-before-data still prevents its status page from passing an
undurable terminal `page_lsn`. A successful read-only transaction remains exempt
from an ordinary terminal WAL/status record.

For direct WAL-before-data coverage, prepare a copied stable dirty image with
`page_lsn=L` and hold `durable_lsn<L`; assert data-page `pwrite` cannot begin.
Advance the canonical durable prefix through the complete record at `L`, then
permit writeback. Merely writing an OS byte at offset `L` never satisfies the
oracle. Inject page `pwrite` and page `fdatasync` failures after WAL durability:
the frame remains dirty under Chapter 7 and a durably COMMITTED transaction does
not reverse. The Buffer management copied-image fixture remains the owner for a
legal mutation/writeback ordering and the injected stale-token defense that
prevents an old `G` completion from clearing a subsequent generation `G+1`.
Repeat the gate for heap, TXN_STATUS, WAL-protected FSM, catalog, B+ tree, and
every applicable BufferPool-managed superblock/control-like page family; no
record family receives an implicit unlogged writeback exception.

### WAL tail, segment inventory, and recovery handoff

Construct exact persisted byte prefixes instead of depending on kill timing. The
tail scanner matrix below covers a final suffix shorter than 48 bytes, a complete
header with truncated payload/span, and a complete-sized CRC-invalid candidate at
the first unrequired final suffix. Under §13.11 each is excluded from the valid
tail; a complete malformed record inside the required retained prefix, or an
invalid record followed by required history, is corruption and cannot be silently
truncated. No decoder reads past the constructed bytes.

Create a valid segment followed by one exact-size all-zero next-contiguous segment
and verify it is an empty/adoptable tail only through §§12.2.1 and 13.11. Separately
omit a required interior segment and require recovery failure. Retention-floor
fixtures may omit or reconcile only segments Chapter 13/14 prove wholly older than
the installed floor; an unexplained noncontiguous or future segment is never used
to bridge a gap.

Place a complete CRC-valid unknown type in the authoritative prefix and require
`UNSUPPORTED_WAL_FORMAT`. Place a complete known PAGE_DELTA/BTREE_MTR with one
forbidden zero count in the same position and require the canonical corruption
path. Neither is a torn tail, and neither permits READY. This precedence is tested
with otherwise canonical framing so nested grammar is the sole defect.

Recovery-admission barriers prove ordinary append cannot begin until exclusive
ownership, segment inventory, valid-tail reconstruction, append position,
durable-prefix state, analysis/redo/loser resolution, and the Chapter-3 READY gate
are established. Negative target fixtures use the actual PageId/catalog/file
ownership chain: wrong FileId, wrong PageNo/type/owner, missing required target,
and unpublished/orphan targets are classified before ordinary publication; no
invented WAL database UUID is assumed.

For redo, first use a checksum-invalid data page carrying a plausible high
`page_lsn`; the stored LSN must not suppress reconstruction. After a page is
valid/trusted, cover `page_lsn < record_lsn`, equality, and greater-than cases
with the exact Chapter-13 apply/skip oracle. PAGE_INIT/PAGE_IMAGE/full-image MTR
fixtures supply reconstruction bases; repeated recovery must be idempotent and a
complete MTR may not leave a subset of page effects.

### TXN_STATUS image/terminal crash prefixes

Use a status mutation that requires preparatory system image `F` and terminal
record `T`, with `F<T`. Independently decode that `F` has system ownership and no
terminal bit for the subject transaction, while `T` is on the user transaction's
WAL chain. After successful runtime installation assert `rec_lsn=F` and
`page_lsn=T`; the same rule uses PAGE_INIT LSN `I` for a newly allocated status
page.

Construct every crash case by selecting persisted record bytes, segment/directory
durability facts, and status-page disk bytes independently. Runtime append success
is not evidence after restart. In particular, an F-only prefix is merely a
physical base and cannot create COMMITTED or ABORTED; a validly appended but
unproven T produces the terminal outcome only if a complete T survives in the
validated prefix; durable T reconstructs its terminal status even when the data
page was never flushed. A durable COMMIT remains COMMITTED across crashes before
runtime publication or acknowledgement.

Before Chapter-9 runtime terminal publication, pause after resident status-bit
installation and prove the active registry still reports the transaction as
nonterminal to ordinary lookup. Persistent bits become ordinary terminal
authority only through the owning publication/recovery rules; early resident
installation cannot expose COMMITTED before C3/C4.

For repeated same-page terminal mutations within one dirty interval, preserve the
first required `rec_lsn`, advance `page_lsn` to each latest authorizing terminal
record, and preserve dirty/DPT coherence. If a checkpoint FPI epoch adds a newer
image while the page is already dirty, the original dirty-interval `rec_lsn`
remains. Retention tests hold the image at `rec_lsn` and all later required
terminal WAL until a durably clean page/checkpoint state makes them unnecessary;
only then may Chapter 13/14 advance the floor.

### Length, position, checkpoint, and lifetime cross-owners

For every record family, the independent builder exercises the smallest legal
payload, the largest aligned record that fits one segment, and the next byte or
count that would exceed the family, uint32, or segment bound. All additions and
alignment use widened integers before narrowing; rejection occurs before record
allocation, LSN reservation, provisional mutation, or publication. Count maxima
remain consequences of the unchanged record-size grammar, so the positive
nonzero-count boundary does not alter any upper limit.

Numeric Exhaustion and Terminal-Boundary Verification directly supplies
synthetic fixtures for the last legal minimum-size record start
`2^64-48`, a legal exclusive mathematical end of `2^64`, rejection of the next
reservation, maximum segment index `2^38-1`, and rejection of the next index
before name formatting or creation. A separate injected ENOSPC at an ordinary
legal LSN proves filesystem exhaustion is not `WAL_POSITION_EXHAUSTED`.
Its terminal-headroom fixture admits an active persistent writer, consumes all
general WAL capacity up to the credited boundary, rejects another ordinary WAL
operation, and still permits the one required TXN_COMMIT or TXN_ABORT plus any
bounded preparatory status image. Candidate reservation must preserve that
credit without turning it into a hole.

Checkpoint codecs and sequence validation are byte-covered above; Crash
Injection Framework, Database Lifecycle Tests, and Recovery Property Tests
remain the procedural owners for fuzzy capture, installation only after
checkpoint WAL/control prerequisites, DPT redo bounds, full-image retention,
and recycling. The status-image retention fixture specifically holds
`F=rec_lsn` until those owners prove it unnecessary.

Database Lifecycle Tests own the shutdown order: stop new admission, drain or
fail authorized append/durability requests, wake unsatisfiable waiters, preserve
WAL service until BufferPool no longer needs it, and perform no ordinary append
before recovery publishes READY. Chapter 15/§39 fault tables remain the owners
for the first-persistent-statement-write boundary; this section supplies the
underlying WAL known-failure versus uncertainty result without changing the SQL
consequence. Recovery continues to use redo plus loser status rather than
physical user-DML undo or CLRs.

### WAL format-classification matrix

| Fixture | Outer framing complete? | CRC valid? | Known type? | Payload grammar valid? | First unrequired final tail? | Expected classification |
|---|---:|---:|---:|---:|---:|---|
| canonical record | yes | yes | yes | yes | either | accepted |
| incomplete final header | no | N/A | undecodable | N/A | yes | incomplete/torn final tail; exclude suffix |
| complete header, incomplete final payload/span | no | N/A | yes | undecidable | yes | incomplete/torn final tail; exclude suffix |
| complete-sized CRC-invalid final candidate | yes | no | yes | otherwise yes | yes | invalid/torn final tail under §13.11; exclude suffix |
| CRC-invalid required interior record | yes | no | yes | otherwise yes | no | `CORRUPT_WAL`/recovery failure |
| complete unknown type | yes | yes | no | N/A | no | `UNSUPPORTED_WAL_FORMAT` |
| recognizable future owning format | yes | as owned | not v1 | N/A | no | canonical unsupported-format result |
| PAGE_DELTA `patch_count=0` | yes | yes | yes | no | either | malformed recognized-v1 WAL / corruption |
| BTREE_MTR `page_count=0` | yes | yes | yes | no | either | malformed recognized-v1 WAL / corruption |
| PATCH_SET `patch_count=0` | yes | yes | yes | no | either | malformed recognized-v1 WAL / corruption |
| nonzero external alignment byte | yes | yes over covered bytes | yes | framing invalid | no | malformed recognized-v1 WAL / corruption |
| malformed nested length | yes | yes | yes | no | no | malformed recognized-v1 WAL / corruption |

The incomplete/CRC-invalid final-tail rows apply only at the first unrequired
suffix. Relocating those same bytes into the required retained prefix changes
the outcome to corruption. A complete CRC-valid known-v1 record with a forbidden
zero count remains malformed/corrupt even when it is physically final; its
recognized payload grammar prevents torn-tail treatment.

### Durable-prefix and WAL-before-data matrix

| Scenario | Valid append end | Namespace durable? | Required segment data durable? | `durable_lsn` may cover target? | Waiter may return success? | Page at target LSN may write back? |
|---|---|---:|---:|---:|---:|---:|
| append only | includes target | yes | no | no | no | no |
| data write complete, `fdatasync` pending | includes target | yes | no | no | no | no |
| single-segment `fdatasync` complete | includes target | yes | yes | yes | yes | yes |
| new-segment directory sync pending | includes target | no | yes | no | no | no |
| cross-segment target, older file unsynced | includes target | yes | no | no | no | no |
| cross-segment target, all required files synced | includes target | yes | yes | yes | yes | yes |
| group waiter at lower proven target | beyond target | yes | through own target | yes for lower target | yes | yes |
| group waiter at maximum unproven target | includes target | yes | no | no | no | no |
| flush failure | includes target | as constructed | no for target | preserve prior proven value | no for affected target | no for affected page |

### WAL segment and framing matrix

| Domain | Positive fixture | Negative/boundary fixture | Independent observation |
|---|---|---|---|
| segment basename | indexes 0, 1, `2^38-1` | short, uppercase, decimal, wrong suffix, `2^38` | exact 16 lowercase hex digits plus `.wal` |
| segment-zero prefix | eight zero bytes; first LSN 8 | record-like bytes before 8 | scanner begins at 8 |
| segment header | later segment record at offset 0 | invented header displacement | no header bytes consumed |
| size | exactly 67,108,864 | one short/long | file length and terminal offset |
| creation durability | truncate, file sync, directory sync | fail each step | no durability credit before both sync classes |
| record start | eight-byte aligned | misaligned encoded/physical start | widened LSN arithmetic |
| header | exact 48-byte vector | each field/length mutation | byte oracle |
| CRC | independent canonical vector | header and payload bit flips | independent CRC32C |
| padding | zero and outside `total_length`/CRC | nonzero byte | byte and CRC oracles |
| no crossing | exact fit | one-byte excess | no cross-segment assembly |
| segment tail | valid WAL_PAD; short zero tail | header-sized zeros; garbage | framing and §13.11 location oracle |

### WAL record-family codec matrix

| Type/code | Minimum payload / positive boundary | Principal persistent fields | Page / txn association | Malformed boundary | Oracle |
|---|---|---|---|---|---|
| WAL_PAD / 0 | zero-filled payload sized to segment remainder | normal header, zero payload bytes | neither | wrong size/nonzero payload | byte/CRC/segment arithmetic |
| PAGE_INIT / 1 | 8216 | PageId, type, reserved, length, full image | page; user or system per owner | image length/identity/LSN/checksum | independent page and WAL bytes |
| PAGE_DELTA / 2 | 33, count 1 and one-byte patch | PageId, type, count, patches | page; user or system per owner | count 0, empty/invalid patch | byte/nested-length oracle |
| PAGE_IMAGE / 3 | 8216 | same codec as PAGE_INIT, existing-page meaning | page; user or system per owner | image length/identity/LSN/checksum | independent page and WAL bytes |
| BTREE_MTR / 4 | 57 for one one-byte PATCH_SET | diagnostic owner, page count, entries | system record; multiple pages | page count 0, patch count 0, bad nested entry | nested byte/LSN oracle |
| TXN_COMMIT / 5 | 0 | header TxnId and previous transaction LSN | transaction | payload nonzero/invalid owner | byte/chain oracle |
| TXN_ABORT / 6 | 0 | header TxnId and previous transaction LSN | transaction | payload nonzero/invalid owner | byte/chain oracle |
| CHECKPOINT_BEGIN / 7 | 32 | exact Chapter-13 BEGIN fields | system | wrong identity/reserved/length | independent checkpoint vector |
| CHECKPOINT_DATA / 8 | 24 plus exact arrays | chunk/count prefix, DPT/writers | system | count/length/index mismatch | independent array arithmetic |
| CHECKPOINT_END / 9 | 32 | exact Chapter-13 END fields | system | cross-link/count/reserved mismatch | independent sequence oracle |

### TXN_STATUS crash-prefix matrix

| Controlled prefix/boundary | Surviving WAL | Status-page disk state | Runtime state after crash | Recovered terminal status | Recovery base | Legal client interpretation |
|---|---|---|---|---|---|---|
| before F | no F/T | prior trusted page | lost | loser becomes ABORTED where required | prior page/older retained image | no durable COMMIT |
| F appended only | complete F, no T | prior page | lost | F supplies no terminal outcome; loser resolution applies | F | no durable COMMIT |
| F durable only | durable F, no T | prior or F image | lost | same: F is semantically inert for terminal outcome | F | no durable COMMIT |
| T appended, absent from surviving valid prefix | F only | unflushed | lost | loser outcome | F | no durable COMMIT |
| complete T survives without prior acknowledgement | F then T in valid prefix | unflushed | lost | terminal code from T | F then terminal redo | client may be uncertain; recovery truth controls |
| T durable, status page unflushed | durable F/T | prior page | lost | exact COMMITTED/ABORTED from T | F then T | COMMIT durable and irreversible |
| page write attempted before T durability | T not durable | no new legal page | lost if crashed | derived from valid WAL only | prior/F | write must have been blocked |
| stable status page after WBD | WAL through page_lsn durable | trusted page at T or later | lost | reflected outcome, idempotently checked | trusted page | outcome agrees with durable WAL |
| crash after durable T before C4 | durable F/T | either | lost | COMMITTED for commit T | F/page plus T | success may be unknown; not ABORTED |
| crash after C3 before C6 | durable F/T | either | lost | COMMITTED | F/page plus T | transport uncertainty cannot reverse outcome |

### WAL tail and inventory matrix

| Fixture | Location | Complete framing / CRC | Segment continuity | Expected valid end | Classification | READY allowed? |
|---|---|---|---|---|---|---:|
| clean EOF/zero suffix | final tail | no next record | contiguous | after last complete record | clean tail | yes after recovery |
| short all-zero tail | segment end, `<48` bytes | raw padding | contiguous | next segment boundary only if next valid record exists | legal tail padding | yes |
| incomplete final header | final suffix | incomplete | contiguous | prior record end | torn/incomplete tail | yes after discard |
| complete header, incomplete payload/span | final suffix | incomplete | contiguous | prior record end | torn/incomplete tail | yes after discard |
| complete-sized CRC-invalid candidate | first unrequired final suffix | complete / bad | contiguous | prior record end | invalid/torn final tail | yes after discard |
| CRC-invalid required interior record | retained interior | complete / bad | contiguous | cannot establish required prefix | corruption | no |
| all-zero next segment | next contiguous segment | no record | contiguous | prior valid end | permitted empty tail; resync before adoption | yes |
| missing required middle segment | retained interior | N/A | gap | cannot establish | corruption/recovery failure | no |
| complete unknown record | authoritative prefix | complete / valid | contiguous | scan stops with semantic failure | `UNSUPPORTED_WAL_FORMAT` | no |
| complete known malformed record, including final zero-count record | authoritative prefix or physical final record | complete / valid | contiguous | scan stops with grammar failure | `CORRUPT_WAL`/recovery failure | no |

### WAL error/result matrix

| Condition | Expected architecture-owned result/consequence | Forbidden collapse |
|---|---|---|
| malformed recognized-v1 required WAL | `CORRUPT_WAL`/canonical recovery corruption | unsupported type, no-op, free truncation |
| unknown complete record type / future owning format | `UNSUPPORTED_WAL_FORMAT` or owning unsupported-format result | generic v1 corruption or skip |
| incomplete/invalid first final suffix | torn/invalid final-tail exclusion under §13.11 | accepted record or interior-corruption suppression |
| logical record physical span exceeds segment | `WAL_RECORD_TOO_LARGE`/`ENCODED_LENGTH_EXCEEDED` before reservation | partial append |
| mathematical WAL end exhausted | `WAL_POSITION_EXHAUSTED` before reservation/publication | wrap or ENOSPC |
| filesystem ENOSPC/write/sync failure at legal position | exact WAL/filesystem I/O failure and §12.12 consequence | numeric exhaustion |
| segment namespace synchronization failure | WAL I/O failure; no durability credit | durable segment inference from file data alone |
| uncertain valid-end/assigned bytes | `DATABASE_NONCONTINUABLE` | ordinary append failure/retry guess |
| fatal WAL service/shutdown failure | exact failure to all unsatisfiable waiters | indefinite wait or false success |
| data-page write/sync failure after WAL durable | page remains dirty; exact page I/O failure | reverse WAL or COMMITTED outcome |

### High-level WAL domain/case matrix

| Domain/case | Deterministic fixture or order | Independent oracle | Architecture / verification owner |
|---|---|---|---|
| segment namespace | indexes 0, 1, maximum and malformed names | text grammar | §§12.2–12.2.1 / segment subsection |
| segment creation durability | fail create/truncate/file sync/directory sync | event order and namespace fact | §12.2.1 / segment subsection |
| record header | distinctive 48-byte vector | byte oracle | §12.4 / framing subsection |
| CRC and padding | bit flips and nonaligned total | CRC/byte oracle | §§12.3–12.4 / framing subsection |
| no crossing | exact fit and one-byte excess | widened placement arithmetic | §12.3 / framing subsection |
| WAL PageId | legal and invalid identities | 16-byte oracle | §12.5 / PageId subsection |
| PAGE_DELTA valid | count 1 and multiple patches | nested byte oracle | §12.8 / PAGE_DELTA subsection |
| PAGE_DELTA zero count | otherwise canonical complete record | grammar oracle | §12.8 / PAGE_DELTA subsection |
| PAGE_INIT / PAGE_IMAGE | exact images and owner mutations | page/WAL oracle | §§12.9–12.10 / image subsection |
| BTREE_MTR valid | one-page and multi-page | nested/common-LSN oracle | §12.10.2 / MTR subsection |
| BTREE_MTR/PATCH_SET zero count | otherwise canonical complete records | grammar oracle | §12.10.2 / MTR subsection |
| terminal records | commit/abort zero-payload vectors | byte/chain oracle | §§12.7.2, 9.14 / registry subsection |
| append reservation | cancel before authorization | valid-end oracle | §12.12.1 / append subsection |
| append authorization | barriers around valid-end publication | state-transition oracle | §§12.12.2–12.12.4 / append subsection |
| physical short write | exact prefix return counts | retained-byte/persisted-prefix oracle | §12.12.2 / append subsection |
| append uncertainty | indeterminate end or byte ownership | continuation-state oracle | §12.12.4 / append subsection |
| single/cross-segment flush | hold each segment sync | durable-prefix oracle | §12.13 / durability subsection |
| namespace prerequisite | hold directory sync | namespace/durable-prefix oracle | §§12.2.1, 12.13 / durability subsection |
| group commit | aligned distinct targets | per-waiter predicate | §§12.14–12.15 / durability subsection |
| WAL-before-data | hold `durable_lsn<L` | complete-record prefix oracle | §§7.10, 12.17 / durability subsection |
| tail reconstruction | exact final byte prefixes | recovery-prefix oracle | §13.11 / tail subsection |
| unknown vs malformed | complete unknown and known-invalid records | registry/payload grammar | §§12.7, 13.11 / format matrix |
| status two-record protocol | F/T persisted-prefix matrix | terminal/status model | §§12.10.5, 13.13.2 / status subsection |
| LSN/record exhaustion | synthetic terminal arithmetic | widened LSN oracle | §4.3.2.4 / numeric exhaustion |
| recovery handoff | hold READY and append admission | lifecycle event oracle | §§3.3, 13.19 / tail subsection |

### Chapter-12 architecture-obligation coverage map

The atomic inventory below assigns each obligation one primary domain. Splitting
combined architecture sentences by independently faultable or observable result
produces the independently testable cases. A direct
procedure above or the named complete procedure supplies a
deterministic oracle, not merely that Architecture states the requirement.

| ID/domain | Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference |
|---|---|---|---|---|
| 1 A | WAL resides in the database `wal/` namespace | §12.2 | WAL segment namespace | exact directory/name inventory |
| 2 A | segment basename is 16 lowercase hex digits plus `.wal` | §12.2 | WAL segment namespace | indexes 0, 1, max plus malformed names |
| 3 A | segment size is exactly 67,108,864 bytes | §12.2 | WAL segment namespace | exact/short/long length fixtures |
| 4 A | segment has no header | §12.2.1 | WAL segment namespace | later-segment record at offset zero |
| 5 A | segment-zero bytes 0..7 are zero and first append end is 8 | §§12.2, 13.11 | WAL segment namespace / tail | reserved-prefix decode fixture |
| 6 A | fresh unused segment bytes are zero | §12.2.1 | WAL segment namespace | exact-size zero-content validation |
| 7 A | required retained segment inventory is contiguous | §13.11 | WAL tail and inventory | missing-middle fixture |
| 8 B | segment creation orders create, truncate, file sync, directory sync | §12.2.1 | WAL segment namespace | event barriers and per-step faults |
| 9 B | creation/sync failure cannot advance durability | §§12.2.1, 12.13 | WAL segment namespace / durable prefix | hold/fail each prerequisite |
| 10 B | all-zero next segment requires validation and resynchronization before adoption | §§12.2.1, 13.11 | WAL tail and inventory | all-zero survivor fixture |
| 11 D | ordinary record never crosses a segment | §12.3 | WAL record framing | exact-fit and one-byte-excess fixtures |
| 12 D | ordinary record starts are eight-byte aligned | §§12.3–12.4 | WAL record framing / LSN oracle | aligned and misaligned physical starts |
| 13 E | `total_length`, payload, and physical-span arithmetic is exact | §§12.3–12.4 | WAL record framing | byte vector plus widened arithmetic |
| 14 D | external padding is zero and outside length/CRC | §§12.3–12.4 | WAL record framing | nonaligned total and padding mutation |
| 15 D | WAL_PAD and raw short zero tail are distinct | §§12.3, 12.7.1 | WAL record framing / tail matrix | legal PAD, short zero, header-sized zero fixtures |
| 16 C | record header is exactly 48 bytes at fixed offsets | §12.4 | WAL record framing | distinctive all-field byte vector |
| 17 C | all multibyte header fields are little-endian | §12.4 | Byte oracle | non-palindromic field values |
| 18 C | header/payload length relationships are validated | §12.4 | WAL record framing | inconsistent-length mutations |
| 19 C | encoded LSN equals physical logical record start | §12.4 | WAL record framing / LSN oracle | relocated and mismatched fixtures |
| 20 C | flags and reserved fields are zero | §12.4 | WAL record framing | independent nonzero mutations |
| 21 E | CRC32C coverage is exact | §12.4 | CRC oracle | independent header-zero-field plus payload vector |
| 22 E | covered header/payload corruption is rejected | §12.4 | WAL record framing | one-bit mutations |
| 23 I | WAL PageId is exact 16-byte ABI-independent codec | §12.5 | WAL PageId codec | byte-exact boundary vectors |
| 24 I | invalid/reserved/wrong target identity fails before redo | §§12.5, 13.13 | WAL PageId / recovery handoff | one-defect owner fixtures |
| 25 M | user records maintain per-transaction previous-LSN chain | §§12.6–12.7.3 | ownership codecs | R1/R2/terminal chain |
| 26 M | system record ownership and recovery abort ownership are canonical | §§12.6–12.7.3 | ownership codecs | system zero-owner and loser-abort fixtures |
| 27 H | record codes 0..9 are stable and each codec is exercised | §12.7 | registry codec matrix | one independent vector per code |
| 28 H | complete unknown record type is unsupported | §§12.7, 13.11 | registry / format matrix | CRC-valid unknown-code fixture |
| 29 H | future format, malformed v1, and torn tail remain distinct | §§4.14, 12.7, 13.11 | format-classification matrix | canonical three-way fixtures |
| 30 J | PAGE_DELTA count-one minimum is accepted | §12.8 | PAGE_DELTA codec | exact one-byte patch vector |
| 31 J | PAGE_DELTA multiple ordered patches are accepted | §12.8 | PAGE_DELTA codec | boundary multi-patch vector |
| 32 J | PAGE_DELTA zero count is corruption and cannot advance page_lsn | §§12.8, 12.18 | PAGE_DELTA codec | complete CRC-valid count-zero fixture |
| 33 J | each PAGE_DELTA patch is nonempty | §12.8 | PAGE_DELTA codec | count-one/length-zero fixture |
| 34 J | PAGE_DELTA order and nonoverlap are validated | §12.8 | PAGE_DELTA codec | descending/repeated/overlap fixtures |
| 35 J | PAGE_DELTA bounds and protected header range are validated | §12.8 | PAGE_DELTA codec | checked end/page_lsn-checksum range fixtures |
| 36 J | PAGE_DELTA count/length consumes payload exactly | §12.8 | PAGE_DELTA codec | mismatch and trailing-byte fixtures |
| 37 K | PAGE_INIT exact full-image codec is accepted | §12.9 | PAGE_INIT/PAGE_IMAGE codecs | independent 8216-byte payload vector |
| 38 K | PAGE_INIT image identity/type/LSN/checksum is validated | §12.9 | PAGE_INIT/PAGE_IMAGE codecs | one-field image mutations |
| 39 K | PAGE_INIT private/publication/crash protocol is old-or-published | §§5.4.2, 12.12 | PAGE_INIT and MTR Rollback Tests | pre/post-authorization and crash fixtures |
| 40 K | PAGE_IMAGE codec is distinct existing-page semantics | §12.9 | PAGE_INIT/PAGE_IMAGE codecs | paired same-layout semantic fixtures |
| 41 K | clean-to-dirty/checkpoint epoch requires a complete image | §12.10 | Buffer management verification / Crash Injection Framework | clean/dirty and FPI-epoch schedules |
| 42 K | dirty-interval `rec_lsn` names retained complete image | §§12.10.1, 12.16 | Buffer management verification / Crash Injection Framework | clean-to-dirty and retention assertions |
| 43 L | one-page BTREE_MTR is a legal minimum | §12.10.2 | BTREE_MTR codec | page-count-one PATCH_SET vector |
| 44 L | zero-page BTREE_MTR is corruption, not no-op | §§12.10.2, 12.18 | BTREE_MTR codec | complete CRC-valid zero-page fixture |
| 45 L | FULL_IMAGE nested entry has exact 8192-byte canonical image | §12.10.2 | BTREE_MTR codec | full-image positive/length-negative vectors |
| 46 L | PATCH_SET count one is accepted | §12.10.2 | BTREE_MTR codec | nested one-byte patch vector |
| 47 L | PATCH_SET zero count is corruption | §§12.10.2, 12.18 | BTREE_MTR codec | complete CRC-valid nested-zero fixture |
| 48 L | nested encoding/length/reserved/patch grammar is validated | §12.10.2 | BTREE_MTR codec | isolated nested-entry matrix |
| 49 L | affected PageIds are sorted and unique | §12.10.2 | BTREE_MTR codec | duplicate/unsorted fixtures |
| 50 L | every MTR participant uses one common MTR LSN | §12.10.2 | BTREE_MTR codec and nested-entry rejection / B+ Tree Verification | legal multi-page recovery vector |
| 51 L | complete MTR recovers old or complete new state | §§12.10.2–12.10.3, 13.13.3 | B+ Tree Verification | BTREE_MTR failure/crash/recovery procedure |
| 52 L | no-flush excludes partial MTR writeback/publication | §§12.10.3, 12.17 | B+ Tree Verification / Publication-atomicity observers | deterministic checkpoint/writeback barriers |
| 53 N | status mutation orders system image F before terminal T | §12.10.5 | TXN_STATUS crash prefixes | independently decoded F/T fixture |
| 54 N | F alone is terminally inert | §12.10.5.4 | TXN_STATUS crash prefixes | image-only prefix |
| 55 N | appended-undurable T is decided only by surviving valid WAL | §12.10.5.4 | TXN_STATUS crash prefixes | two persisted-prefix variants |
| 56 N | durable T repairs unflushed status page | §§12.10.5, 13.13.2 | TXN_STATUS crash prefixes | durable-T/unflushed-page restart |
| 57 N | status page uses `rec_lsn=F`, `page_lsn=T` | §§12.10.5.2, 12.16 | TXN_STATUS crash prefixes | runtime metadata assertion |
| 58 N | repeated status updates preserve rec_lsn and advance page_lsn | §12.10.5.2 | TXN_STATUS crash prefixes | same dirty-interval schedule |
| 59 N | status image/terminal WAL remains retained while required | §§12.10.5.3, 13.10, 14.14 | TXN_STATUS image/terminal crash prefixes / Crash Injection Framework | hold F until clean/checkpoint proof |
| 60 Z | heap redo precedes index MTR that references its RID | §12.11 | B+ Tree Verification / Statement Failure and Transaction-State Tests | ordered WAL decode and crash prefixes |
| 61 O | candidate reservation is private and non-consuming | §12.12.1 | Append reservation, authorization, and physical failure | cancel/fail before authorization |
| 62 O | concurrent appends establish one hole-free total order | §§12.12.1–12.12.2 | Append reservation, authorization, and physical failure | two-appender barrier schedules |
| 63 P | valid append is atomic complete-byte plus valid-end publication | §12.12.2 | Append reservation, authorization, and physical failure | before/after valid-end barriers |
| 64 P | reserved LSN cannot publish page/transaction metadata | §§12.12.1–12.12.3 | Append reservation, authorization, and physical failure | metadata observers before valid end |
| 65 Q | known preauthorization failure restores exact old state | §§12.12.3–12.12.4 | Non-Crash WAL/MTR Failure Injection | clean/dirty rollback captures |
| 66 Q | postauthorization rollback-and-continue is forbidden | §§12.12.3–12.12.4 | Append reservation, authorization, and physical failure / PAGE_INIT and MTR Rollback Tests | publication completion or noncontinuable |
| 67 Q | physical short write retains exact bytes and no durability credit | §12.12.2 | Append reservation, authorization, and physical failure | exact partial-count/retry fixtures |
| 68 Q | uncertain append end/bytes is noncontinuable | §12.12.4 | Append reservation, authorization, and physical failure | uncertainty fault and admission assertions |
| 69 O | append buffer retains required bytes while mechanism remains free | §12.12.5 | Append reservation, authorization, and physical failure / Database Lifecycle Tests | retry, no-hole, shutdown observations |
| 70 R | recovery establishes initial append/durable-prefix facts | §§12.13, 13.11 | Durable prefix, group commit, COMMIT, and WAL-before-data / WAL tail, segment inventory, and recovery handoff | empty and nonempty reopen fixtures |
| 71 R | single-segment durability waits for complete prefix sync | §12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | write/fdatasync barriers |
| 72 R | cross-segment durability syncs every affected segment | §12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | older-segment-unsynced fixture |
| 73 R | new-segment namespace sync precedes durability credit | §§12.2.1, 12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | directory-fsync barrier |
| 74 R | durable_lsn is monotonic and means complete record/prefix | §12.13 | Durable prefix, group commit, COMMIT, and WAL-before-data | independent accepted-prefix model |
| 75 S | group commit handles distinct aligned targets | §12.14 | Durable prefix, group commit, COMMIT, and WAL-before-data | 1000/1104/1256 waiter schedule |
| 76 S | no waiter returns before its own target | §§12.13–12.14 | Durable prefix, group commit, COMMIT, and WAL-before-data | per-waiter predicate assertions |
| 77 S | flush failure preserves proven prefix and wakes affected waiters | §§12.13–12.14 | Durable prefix, group commit, COMMIT, and WAL-before-data | multi-target sync failure |
| 78 X | fatal WAL service/shutdown wakes unsatisfiable waiters | §§3.3.6, 12.13 | Database Lifecycle Tests / Durable prefix, group commit, COMMIT, and WAL-before-data | fatal-service and drain barriers |
| 79 M | TXN_COMMIT codec and C3 durability boundary are exact | §§12.7.2, 12.15; §9.14.1 | Registry, terminal, and checkpoint record codecs / COMMIT Fault-Injection Tests | byte vector and `durable_lsn` barrier |
| 80 M | TXN_ABORT codec preserves ordinary no-force semantics | §§12.7.2, 12.10.5.2; §9.14.2 | Registry, terminal, and checkpoint record codecs / ABORT Fault-Injection Tests | byte vector and status WBD gate |
| 81 M | read-only terminal-WAL exception remains | §9.14 | Transaction identity, snapshot, and status verification / COMMIT Fault-Injection Tests | successful read-only transaction fixture |
| 82 M | durable COMMIT is irreversible; C4–C6 remain separate | §§9.14.1, 12.15 | COMMIT Fault-Injection Tests | C3 through acknowledgement boundaries |
| 83 T | page writeback is blocked while `durable_lsn < page_lsn` | §12.17 | Durable prefix, group commit, COMMIT, and WAL-before-data / Buffer management verification | direct pwrite gate |
| 84 T | WBD uses complete-record/prefix durability meaning | §§12.13, 12.17 | Durable prefix, group commit, COMMIT, and WAL-before-data | independent record acceptance oracle |
| 85 T | data flush failure leaves page dirty and cannot reverse commit | §§7.10–7.11, 12.17 | Buffer management verification | post-WAL page I/O faults |
| 86 T | older copied writeback cannot clear newer dirty generation | §§7.10–7.11, 12.16 | Buffer management verification | Legal waiting-writer schedule and injected stale-token fixture |
| 87 U | incomplete final header is excluded as torn tail | §13.11 | WAL tail and inventory | exact short persisted prefix |
| 88 U | incomplete final payload/span is excluded as torn tail | §13.11 | WAL tail and inventory | exact truncated record variants |
| 89 U | CRC-invalid complete-sized first final suffix is excluded as invalid tail | §13.11 | WAL tail and inventory | final-location CRC fixture |
| 90 U | CRC-invalid/malformed required interior WAL is corruption | §13.11 | WAL tail and inventory | bad interior followed by required history |
| 91 U | all-zero next segment is distinguished from required history | §§12.2.1, 13.11 | WAL tail and inventory | next-contiguous zero segment |
| 92 U | missing required interior segment prevents recovery | §13.11 | WAL tail and inventory | N/N+2 inventory |
| 93 W | retention-floor omissions/extras follow checkpoint ownership | §§13.10–13.11, 14.14 | WAL tail, segment inventory, and recovery handoff / Crash Injection Framework | installed-floor inventory variants |
| 94 V | recovery establishes valid end before READY/ordinary append | §§3.3.3, 13.19 | WAL tail, segment inventory, and recovery handoff / Database Lifecycle Tests | append blocked at each open phase |
| 95 V | checksum is validated before trusting disk page_lsn | §§4.13, 13.14 | WAL tail, segment inventory, and recovery handoff | high-LSN checksum-bad page |
| 96 V | trusted redo page_lsn less/equal/greater cases are exact | §§12.8–12.10, 13.13 | WAL tail, segment inventory, and recovery handoff | apply/skip/idempotence matrix |
| 97 G | record-family min/max and one-unit-excess lengths are checked | §§4.3.2.4, 12.3 | Length, position, checkpoint, and lifetime cross-owners / Numeric Exhaustion and Terminal-Boundary Verification | synthetic widened builders |
| 98 F | final LSN/exclusive-end arithmetic never wraps | §4.3.2.4 | Numeric Exhaustion and Terminal-Boundary Verification | `2^64-48`, end `2^64`, next rejection |
| 99 G | segment index stops at `2^38-1` without name truncation | §§4.3.2.4, 12.2 | Numeric Exhaustion and Terminal-Boundary Verification / WAL segment namespace and creation | max and next-index fixtures |
| 100 Y | WAL_POSITION_EXHAUSTED differs from legal-position ENOSPC | §§4.3.2.4, 12.12.4 | Numeric Exhaustion and Terminal-Boundary Verification / WAL error/result matrix | synthetic exhaustion vs injected filesystem fault |
| 101 W | checkpoint record codecs and sequence links are exact | §§13.5–13.8 | registry / Crash Injection Framework | BEGIN/DATA/END byte vectors |
| 102 W | checkpoint installation follows WAL/control durability prerequisites | §§13.5–13.10 | Crash Injection Framework / Database Lifecycle Tests | crash boundaries before/after install |
| 103 W | checkpoint/DPT retains required full images and status bases | §§12.10, 13.9–13.10 | Crash Injection Framework / TXN_STATUS image/terminal crash prefixes | rec_lsn retention fixtures |
| 104 V | WAL target ownership prevents cross-database/file/page replay | §§4.7, 12.5, 13.13 | WAL PageId and per-transaction ownership codecs / WAL tail, segment inventory, and recovery handoff | actual registry/catalog owner negatives |
| 105 Y | WAL failure consequences respect first persistent statement write | §§15.7, 39.1 | Non-Crash WAL/MTR Failure Injection / Statement Failure and Transaction-State Tests | prewrite and postwrite fault rows |
| 106 X | shutdown drains authorized WAL before service teardown | §§3.3.6, 12.12.5 | Database Lifecycle Tests | DRAINING append/flush/waiter order |
| 107 V | recovery uses redo plus loser status, not user-DML physical undo | §§12.1, 13.15–13.16 | Recovery Property Tests | committed model plus loser visibility |
| 108 V | only complete valid persisted WAL determines recovery and terminal authority | §§12.12, 13.11–13.20 | Recovery-prefix oracle / COMMIT Fault-Injection Tests / ABORT Fault-Injection Tests | exact persisted-prefix and terminal model |
| 109 J | PAGE_DELTA writers never emit zero-patch records | §§12.8, 12.18 | PAGE_DELTA codec and payload rejection | zero-count construction fails before reservation; emitted-record inspection |
| 110 L | BTREE_MTR writers never emit zero-page records | §§12.10.2, 12.18 | BTREE_MTR codec and nested-entry rejection | zero-page construction fails before reservation; emitted-record inspection |
| 111 L | PATCH_SET writers never emit zero-patch entries | §§12.10.2, 12.18 | BTREE_MTR codec and nested-entry rejection | nested zero-count construction fails before reservation; emitted-record inspection |
| 112 Z | WAL raw positional I/O is a bounded WAL-owned exception, not BufferPool ownership | §§7.3, 12.1, 12.13 | Deterministic WAL harness and independent oracles | ownership-seam instrumentation and injected lower I/O result |
| 113 G | reservation preserves terminal WAL/status-image headroom without creating holes | §§4.3.2.4, 12.12.1 | Numeric Exhaustion and Terminal-Boundary Verification / Length, position, checkpoint, and lifetime cross-owners | credited-terminal record after general-capacity exhaustion |
| 114 H | WAL_PAD affects framing only and never enters page/transaction redo | §§12.7.1, 13.12–13.13 | WAL record framing, header, CRC, padding, and segment tails | independently decoded PAD through analysis/redo |
| 115 T | every BufferPool-managed WAL-protected page family obeys WAL-before-data | §12.17 | Durable prefix, group commit, COMMIT, and WAL-before-data / Buffer management verification | parameterized page-family writeback gate |
| 116 L | user abort does not physically reverse an authorized system BTREE_MTR | §§12.10.2–12.10.4, 13.13.3 | BTREE_MTR codec and nested-entry rejection / B+ Tree Verification | diagnostic owner abort plus recovery/reopen |
| 117 N | active-registry state dominates resident status bits before runtime terminal publication | §§9.13–9.14, 12.10.5.2 | TXN_STATUS image/terminal crash prefixes / Transaction identity, snapshot, and status verification | pause after status-bit install before C3/C4 |


## Crash Injection Framework

Add deterministic process-termination points around:

```text
after WAL append
before WAL append completion
before/after fdatasync
before/after data-page pwrite
after heap insert before index MTR
during B+ MTR construction
after B+ MTR append before page release
before/after TXN_COMMIT flush
during checkpoint construction
after checkpoint durability but before checkpoint installation
after checkpoint installation
during vacuum index cleanup
before DEAD transition
before DEAD -> UNUSED transition
```

For each point:

1. construct a database with known logical contents and enough physical state to make the
   selected boundary observable;
2. terminate the process at exactly that boundary;
3. reopen through the ordinary database lifecycle and run recovery;
4. compare logical contents with the expected durable-transaction model;
5. inspect any physical invariant that the boundary is specifically intended to protect,
   such as checkpoint selection, page LSNs, index reachability, slot state, or WAL-prefix
   validity.

Crash injection uses a separate process and must not run destructors or clean-shutdown
logic after the selected boundary. A crash test passes only when reopen/recovery succeeds
or returns the exact architecture-defined persistent error; process survival alone is not
success.

---

## Non-Crash WAL/MTR Failure Injection

Exercise every non-crash outcome in [`ARCHITECTURE.md`](../ARCHITECTURE.md) §12.12 separately
from process-crash testing. The harness must be able to fail reservation, record
construction, logical append, physical WAL writing, WAL synchronization, runtime
publication, and append-tail cleanup independently.

For each locally recoverable case:

1. capture the complete pre-operation state while the tested operation owns the required
   pins, latches, barriers, and frame-transition reservations;
2. inject one precise failure;
3. let the operation return its structured error without crashing the process;
4. compare the resulting state with the capture;
5. prove that ordinary operations may continue only when §12.12 classifies the outcome as
   locally recoverable.

The failure matrix must include:

```text
reservation/resource failure before page mutation
record construction/encoding/overflow/validation failure after no-flush acquisition
known append failure after one provisional page mutation
known append failure after all provisional page mutations
append-outcome or valid-end ownership uncertainty
valid append followed by runtime page/frame publication failure
valid append followed by WAL pwrite failure
valid append followed by WAL fdatasync failure
failure while restoring an unpublished appended tail
failure while truncating/verifying an unpublished appended tail
copied-writeback wins before no-flush reservation
no-flush reservation wins before copied-writeback
```

Known no-append failures must leave the valid WAL end and transaction WAL-chain tail
unchanged and restore every provisional mutation exactly. Append uncertainty must enter
the architecture-defined `NONCONTINUABLE` database state: no test may treat uncertainty as
an ordinary retryable append error.

After a valid append, WAL `pwrite`/`fdatasync` failure must retain the exact append bytes,
leave `durable_lsn` at the prior proven prefix, keep the mutation as legal dirty published
state, prevent dependent data-page writeback, and allow only the retry/escalation behavior
defined by §12.12. Publication failure after the publication-authorizing append must be
retried under retained ownership or become `NONCONTINUABLE`; it must not restore the old
state and continue.

The copied-writeback race is tested in both directions with deterministic barriers:

- when writeback reserves first, mutation waits for reconciliation and captures rollback
  state afterward; a reported writeback failure leaves the mutation unstarted;
- when no-flush reserves first, copied writeback and eviction cannot begin until complete
  publication or exact rollback releases the reservation.

### Exact rollback-state assertions

Run rollback cases with every affected page initially clean and initially dirty. Compare
the complete captured state, including:

```text
exact page bytes
PageId/frame identity token
page_lsn
dirty/clean status
modification generation
rec_lsn
dirty-page-table membership
FPI/checkpoint epoch state
owning root/height/allocation/free-list runtime metadata where applicable
```

An initially dirty page must return to the same dirty bytes and metadata; rollback must not
clean it, assign another generation, change its original `rec_lsn`, or alter DPT/FPI state.
An initially clean page must remain clean and absent from the DPT. Also assert that no
ordinary guard, checkpoint capture, writeback, eviction, or unrelated reader can observe
the provisional state.

---

## PAGE_INIT and MTR Rollback Tests

### PAGE_INIT failure

For ordinary page initialization, capture the file length/page count, runtime allocation
metadata, publication counts, and every structure that could refer to the new PageNo. Then
inject each known pre-append and cleanup failure required by §12.12 and §4.11.1.1.

For a known locally recoverable failure, assert:

- the unpublished appended tail is serialized against competing extension and immediately
  restored/truncated to the captured length;
- no published page count, parent/root/free-list reference, catalog reference, or other
  durable structure names the failed page;
- the valid WAL end and ordinary frame/DPT state do not advertise the failed initialization;
- the PageNo is reused deterministically by the next serialized successful extension where
  the architecture requires reuse.

Inject failure of tail restoration, truncation, and post-truncation verification
separately. If exact length/ownership cannot be established, assert transition to
`NONCONTINUABLE`, retained protection of uncertain state, rejection of ordinary work, and
recovery on the next open. Such failures must never be hidden as successful allocation or
ordinary rollback.

### B+ MTR rollback

Construct operations that provisionally mutate several existing pages and combinations of
newly allocated and reused pages, including split/root-change paths. Capture the complete
pre-MTR state, inject a known failure before the publication-authorizing append, and assert
exact restoration of:

```text
every affected page and frame metadata item
new-page and reused-page disposition
root PageNo and tree height
left/right sibling links
parent links and separator state
allocation/free-list state
```

Run the same cases with clean and dirty input pages. No page may retain a consumed
modification generation or provisional LSN. If the publication-authorizing record validly
appended, test completion/retry or `NONCONTINUABLE` escalation instead of rollback.

### Publication-atomicity observers

Pause a multi-page MTR after provisional mutation and again during publication. Coordinate
checkpoint DPT capture, flush/writeback, eviction, and permitted readers at those barriers.
Every observer must see either the complete pre-operation bytes/metadata or the complete
published WAL-backed state. No observer may see a mixed page set, a reserved-only LSN,
provisional root/free-list metadata, or a DPT snapshot that includes only part of the MTR.

---

## Database Lifecycle Tests

Exercise the exclusivity rules in `ARCHITECTURE.md` §3.3.2, ordered open protocol in
§3.3.3, READY and cleanup rules in §3.3.4, noncontinuable behavior in §3.3.5, controlled
shutdown protocol in §3.3.6, and lifecycle outcomes in §3.3.7. Use separate processes
wherever the contract depends on POSIX process ownership.

### Exclusive ownership, fork, and exec

Use a parent/child coordination harness to verify:

1. two independent processes race to open the same final database root and exactly one
   reaches ownership/READY;
2. the loser receives `DATABASE_BUSY` before control-slot selection, WAL inspection,
   recovery construction, or database mutation;
3. path-equivalent and alias opens in one process are rejected by the process-local stable
   root/control identity registry rather than path-string comparison;
4. normal close releases ownership only after database users and descriptors drain;
5. abrupt process exit releases the POSIX lock and permits a new process to acquire it and
   recover.

In a fork test, the child must not use copied live handles, must not be treated as the
database owner, and must close/discard them or `exec`. A child independently opening the
same database must conflict with the parent's ownership. Verify the control descriptor's
close-on-exec behavior by executing a helper and proving it cannot accidentally retain the
lock after the parent releases its own descriptors.

### Deterministic database-root lifecycle harness

The root-lifecycle procedures below verify `ARCHITECTURE.md` §§3.3.2–3.3.4 and 3.3.7,
§§4.7.1, 4.7.6, and 4.7.8–4.7.10, the root rules in §4.15 and §41.3, and Appendix B
invariants 13, 18, and 21–24. They use deterministic barriers rather than elapsed-time
sleeps. Separate processes are required for POSIX lock contention, lock release on process
death, and adoption after prior-owner death; in-process competitors separately verify the
process-local stable-identity claim.

The harness records an ordered event stream sufficient to assert, without depending on
implementation class names:

```text
external-parent/root/control descriptor acquisition and (device,inode) identity
lifecycle namespace-gate target identity, attempt, acquisition, and release
process-local root/control claim acquisition and release
ordinary database.control lock attempt, acquisition, and release
root rename request/result and exact source/destination identities
external-parent fsync begin/result
parent-entry/root identity verification and post-fsync revalidation
control-slot inspection, WAL inventory, recovery, catalog inspection, and mutation start
READY publication and transaction admission
private cleanup unlink/rmdir operations
operation semantic result
```

Required conceptual barriers are:

```text
CREATE: namespace gate acquired; staging created; control creation/initialization;
        staged bootstrap validated; control ownership acquired; final rename completed;
        external-parent fsync before/after; READY before/after; ownership release
OPEN:   ownership acquired; adoption before/after; control inspection begins;
        WAL inventory begins; recovery begins
REMOVE: ownership acquired; adoption before/after; destructive identity validated;
        retirement rename before/after; retirement parent fsync before/after;
        private cleanup begins; other entries durably absent; control-only validation;
        control unlink before/after; retired-root fsync before/after;
        retired-root rmdir completed; final parent fsync
```

Fault injection may intercept the corresponding filesystem operation or pause at the
conceptual boundary. Child-process IPC releases barriers and reports observations. A test
must not infer ordering from scheduler timing. Filesystem crash tests use the Crash
Injection Framework's durable-prefix model or an isolated equivalent that distinguishes
runtime rename visibility from parent-directory durability; real power removal is not
required.

Run the shared-ownership fixture once with creation publication, ordinary open, and offline
removal as the attempted owner. For the same retained root/control identity, assert that all
three use the same process claim and ordinary control-lock observations and that failure of
either common ownership check prevents ownership. The preceding lifecycle namespace gate
must not substitute for the common control ownership. Repeat through a pathname alias to
prove that path spelling is not identity.

### Pre-bootstrap lifecycle namespace exclusion

Verify Architecture §§4.7.8 and 4.7.10 with an isolated parent, target `D`, and a
test-owned manifest of staging contents. Use IPC barriers to pause a creator after
gate acquisition but before staging creation, immediately after staging creation,
during partial control initialization, after complete bootstrap validation, and
before/after control-lock acquisition. Independently capture the parent's identity,
target basename, attempted gate acquisition, control descriptor/lock events, and
every filesystem mutation; a cleanup return code alone is not the oracle.

At each barrier launch a stale cleaner for that exact target. Prove its acquisition
attempt actually occurred while the creator was held, and that it cannot acquire
the gate, classify staging as abandoned, or mutate the manifest. Repeat with a
same-process competitor and an alias of the retained parent. Then release the
creator to complete publication: the successful control-lock event must precede
rename, gate ownership must cover the publication barrier, and returning an open
handle must continue the same control ownership after gate release. For a different
target in the same parent, assert a distinct per-target gate identity. Where the
implementation supports independent acquisition, pause the first creator and
demonstrate second-target acquisition; additional implementation serialization
alone is not a violation of the Architecture's “need not serialize” permission.

For each pre-control barrier, terminate the creator without cleanup and wait for
confirmed process death through IPC/process control. The cleaner must now acquire
the same target gate before interpreting abandonment or removing staging. Exercise
missing control, partial control, and valid control survivors independently. For
any present safely openable regular control, the trace must contain ordinary local
root reservation before control open and control ownership before the first byte
interpretation. Repeat with zero-length, partial, damaged, and valid control files;
valid control also requires destructive-identity checks before deletion. A held
control contender must block byte interpretation even for a partial/damaged file,
and a rejected local contender must preserve the owner's sole descriptor/lock.
Pair with a
held-control contender and a recognizable unsupported-format control: gate
ownership alone cannot bypass either refusal. Compare independent before/after
tree manifests and parent-sync events to prove confined, durable cleanup and allow
a subsequent exclusive staged create. When control is unusable, verify that the
gate proof still occurs; absence of a usable lock is not accepted as proof.

Trace every lifecycle entry point, including removal and deferred retired cleanup,
to assert gate-before-control acquisition. Reject an injected reverse acquisition
before it can wait; a still-open ordinary database owner cannot be converted into
an offline remover by taking the gate. Compose with the POSIX duplicate-descriptor
fixture to prove a rejected local contender leaves the original control lock held.

Negative controls deliberately bypass gate acquisition or split the key by parent
path spelling: the harness must detect simultaneous entry or a changed live
creator manifest. Alter timestamps and PID-like fixture values without releasing
the gate and require unchanged exclusion. If the chosen primitive has a backing
object, leave it after owner death and require ownership acquisition to work;
attempt replacement through the adapter's supported backing-object lifecycle
while the gate is held and prove it cannot create a second gate owner. An external
actor bypassing that lifecycle is subject to Architecture §4.7.10's cooperative
environment boundary; this fixture does not claim enforced exclusion of arbitrary
nonparticipants. These controls verify the selected primitive's guarantees without
prescribing its representation. Cross-owner oracles compose retained-directory
containment, control-lock identity, create publication, and the durable-prefix
filesystem model; no elapsed-time threshold establishes liveness.

### Database-root creation publication

For a staged create, pause after the no-replace final-root rename is runtime-visible and
before the external-parent `fsync` succeeds while the creator retains its claim, control
descriptor, and lock. In a second process call ordinary open on final `D`; in another run
attempt offline removal. Each competitor must receive `DATABASE_BUSY`: ordinary open
through control ownership, and removal at the preceding namespace gate while it is held.
Its trace must contain no control-slot interpretation, WAL inventory,
recovery/catalog construction, namespace mutation, root retirement, READY publication, or
transaction admission. This demonstrates that ownership blocks use while the parent sync,
not the lock, remains the durability barrier.

At the same barrier, attempt another open in the creator process using both the original
spelling and an alias resolving to the same retained root/control inodes. The process claim
must reject both attempts, create no second owner/DatabaseInstance, and produce the same
pre-inspection `DATABASE_BUSY` semantics even though process-associated POSIX locks do not
conflict within one process.

Run create-result variants as follows:

| Variant | Required observations |
|---|---|
| returns an open handle | Claim and control lock are acquired before final rename and remain continuously held across rename, parent sync, ordinary recovery/READY prerequisites, and handle publication. No release/reacquire event occurs; a competing process stays busy throughout; READY, transaction admission, and handle publication occur only after the successful parent sync. |
| returns only durable-create success | Claim and lock remain held through successful parent sync; lock release occurs while the process claim still exists; claim removal follows lock release; success follows both. A competitor remains busy before release and may acquire/open after release. |

Inject failure of the external-parent `fsync` after a successful final rename. While the
creator is paused in reconciliation, assert no success, READY handle, transaction admission,
or ownership release and prove that competing open/removal remains busy. The oracle records
the rename as durability-uncertain—it must not label it definitely persisted or definitely
lost. If DatabaseInstance construction began, verify the ordinary failed-open versus
noncontinuable cleanup outcome without admitting work.

Terminate the creator process after final rename and before parent sync. The OS lock must be
released by process death, but that death is not a publication event. Start an independent
opener against a surviving final root and assert this order:

```text
claim and ordinary control lock
verify retained external-parent entry identifies retained root inode
fsync external parent
revalidate/reconcile parent entry and retained identity
control-slot selection / WAL inventory / recovery
```

No control/WAL/recovery observation may precede adoption. If the final root did not survive
an independently modeled machine crash, ordinary open instead follows the architecture's
missing/staging classification; process death by itself is never treated as publication
proof.

Exercise machine-crash durable prefixes around root publication:

| Crash boundary | Allowed survivor/classification | Assertions after restart |
|---|---|---|
| before final rename | staging root or no staged entry; no final live root | staging is never an ordinary open target; no acknowledged ordinary database work exists |
| after final rename, before external-parent sync | final or staging survivor according to the modeled durable prefix | no pre-crash ordinary work was acknowledged; a surviving final root completes independent adoption before use |
| after successful external-parent sync | durably published final root | ordinary ownership, adoption/open validation, and recovery may proceed |

For every create-publication case, capture root/control `(device,inode)` before rename and
after access through final `D`. Assert the retained descriptors and control lock still name
the same objects. This is platform-contract validation for the supported Linux/POSIX model,
not a replacement for the architecture's durability assertions.

### Independent final-root adoption

Use Architecture §4.7.10's cooperative managed-namespace boundary for supported
concurrency schedules. In an isolated parent, run a lifecycle actor and a second
administrative actor that attempts the applicable coordination before rebinding
a managed entry. Hold the first actor at its final identity check and release it
through the name-based mutation and required durability barrier. A test-owned
schedule and gate-acquisition log must prove that the administrative actor really
attempted entry, remained excluded through the protected interval, and can enter
after release. Compare retained root/control identities and sibling manifests
with the fixture's predeclared identities; the implementation's success result is
not the identity oracle. Repeat staging, retirement, and terminal finalization
using their existing owner procedures. Ordinary open retains its control-only
protocol; do not add a namespace-gate requirement to it.

As a negative control, bypass the administrative actor's coordination in the
test model and require the schedule oracle to reject that execution as outside
the supported environment. This must not be reported as a successful supported
concurrency test. Separately inject observable identity changes and newly nonempty
residue using the refusal fixtures below. Those diagnostic fixtures verify the
specified response when a change is observed; they do not prove exclusion of
arbitrary gate-ignoring rebinding between the last check and a name-based mutation.

Exercise ordinary open and offline removal with a barrier immediately after successful
process claim/control-lock acquisition. Permit each operation to continue one event at a
time and require parent-entry/root verification, external-parent `fsync`, and identity
revalidation to complete before any control interpretation beyond minimum adoption identity,
WAL inventory, recovery, catalog inspection, database/namespace mutation, removal identity
validation, or READY publication.

Inject open/read, identity-check, and parent-sync failures independently during adoption.
Assert the existing `IO_OR_DURABILITY_FAILURE` or architecture-defined identity error, no
READY/recovery/mutation/transaction admission, and dependency-ordered ownership cleanup or
retention when safe unwind is uncertain. Inspect the control bytes and filesystem to prove
that no persisted adoption marker, control bit, or alternate root state was written.

Test pathname rebinding in an isolated parent directory. After retaining parent/root/control
descriptors but before post-sync revalidation, have a coordinated helper move the final entry
and install a different root at the same pathname. The owner must detect that the parent
entry no longer identifies the retained root and stop before inspection or mutation of
either object. Repeat for open and removal. Where direct rebinding is unavailable, inject a
different `(device,inode)` identity result at the revalidation boundary and require the same
failure. Path-string equality alone is never an acceptable oracle.

### Offline whole-database removal and destructive identity

For each active-owner condition below, attempt whole-database removal and assert
`DATABASE_BUSY` or the architecture-defined lifecycle rejection before destructive
validation, retirement rename, or content mutation:

```text
ordinary DatabaseInstance active before READY
READY DatabaseInstance active
DRAINING or CLOSING owner still retaining exclusivity
same-process owner through the stable root/control claim
cross-process owner through the ordinary database.control lock
creator retaining root-publication ownership
another offline remover
pathname alias of any of the same actual roots
```

The positive fixture begins from `CLOSED` with no ordinary DatabaseInstance, transaction,
BufferPool/database manager, worker, or background producer. Assert that only this offline
lifecycle owner can acquire removal authority; the test must not close or convert a live
instance as an implicit part of removal.

After legitimate offline ownership and adoption, record all service-construction events.
Removal must reach minimum destructive identity validation and retirement without READY,
normal recovery, BufferPool construction, catalog/transaction-status reconstruction, WAL
inventory/redo/undo, transaction admission, or background-service startup.

Use table-driven fixtures for destructive identity. Every allowed case first satisfies
no-follow retained-parent/root lookup, parent-entry/root identity match, regular no-follow
`database.control`, stable process claim, ordinary control lock, and completed adoption.

| Control/root fixture | Expected removal authority |
|---|---|
| complete slot with bytes `0..7 = DBLUSCTL`, little-endian `format_version=1` at `8..9`, and `header_size=88` at `10..11` | may select a retired name; no recovery-usable generation is required |
| one such complete slot plus a corrupt/invalid other slot | may select a retired name |
| exact v1 framing with bad CRC, invalid generation/range, invalid flags/reserved bytes, or failed cross-field validation | may select a retired name; ordinary open remains subject to its stricter validation |
| recognizable v1 control plus corrupt catalog, missing required managed file, or torn/malformed WAL | may select a retired name without catalog/WAL interpretation or recovery |
| missing root or `database.control` | `NOT_A_DATABASE`/the existing missing-target result; no destructive mutation |
| control symlink or wrong object type | `NOT_A_DATABASE`; no destructive mutation and no followed target |
| unreadable control with no established identity | `IO_OR_DURABILITY_FAILURE`; no destructive mutation |
| no exact supported control-family framing or unrelated ordinary directory | `NOT_A_DATABASE`; no destructive mutation |
| exact `DBLUSCTL` family with `format_version > 1` in either slot | `UNSUPPORTED_DATABASE_FORMAT`; no destructive mutation or fallback to another v1 slot |

Pair a damaged exact-v1 fixture with an otherwise similar future-version fixture. The first
must become retirement-eligible after exclusive authority; the second must be refused before
retired-name selection or mutation. Also place plausible database filenames in an unrelated
directory without exact control framing and compare a recursive tree manifest before/after
the refused operation. U1 authority must never arise from superficial filenames.

### Retired-root classification and retirement publication

Table-test external-parent basename classification for exact
`D.dblusblus-removing-<token>`, where `<token>` is exactly 32 lowercase hexadecimal digits.
Accept exact length/lowercase-hex only. Reject shorter/longer, uppercase, nonhex, empty-token,
missing-token, and wrong-suffix variants. Classify `D.dblusblus-creating`, ordinary valid
final names, and retired names as three distinct cases; both private forms are non-openable,
and a caller-selected final basename matching either reserved grammar is rejected.

Precreate the first selected retired name to force a deterministic collision. Assert the
existing artifact is not overwritten, the no-replace retirement fails for that destination,
another token is selected, and old `D` remains complete/live until a later no-replace rename
actually succeeds. No statistical randomness test is required.

Pause removal before retirement rename while the remover owns `D`. Same-process, alias, and
cross-process open/removal competitors must encounter the applicable namespace or control
ownership gate and perform
no inspection/recovery/mutation. After runtime retirement rename but before parent sync,
ordinary `OpenDatabase(D)` must not attach to or search for the retired root; it observes no
live target unless a distinct `D` exists. This runtime observation is not evidence of durable
absence.

The publication test releases the retirement sequence one boundary at a time:

```text
no-replace rename D -> exact retired sibling succeeds
assert no semantic-success result and no internal unlink/rmdir
external-parent fsync succeeds
assert semantic removal of captured old D is now publishable
```

Inject rename failure and parent-sync failure separately. Rename failure leaves complete
live `D`. Parent-sync failure after rename produces `IO_OR_DURABILITY_FAILURE`, no semantic
success, no internal cleanup, and a durability-uncertain namespace; the live remover retains
authority while reconciling. The test oracle must not claim that old `D` is definitely
durably absent merely because runtime lookup fails.

During the same sequence, verify that the retained root/control descriptors have unchanged
inodes across retirement rename and that the ordinary control lock and process claim remain
continuous without release/reacquire through successful retirement parent sync. Capture all
unlink/replace operations: `database.control` must not be removed or made independently
recreatable while the captured old root remains live at `D`, and any control deletion must
follow durable retirement and the terminal durability/emptiness prerequisites in
the **Durable empty terminal root finalization** procedure below.

Attempt creation of a new `D` after runtime retirement rename but before its parent sync.
It cannot acquire the namespace gate while the remover retains it. After owner death or
an incomplete-operation handoff, a creator that acquires the gate still cannot acknowledge
publication from runtime absence alone: its trace must establish/reconcile prior-name
absence through the external-parent durability barrier before its own final rename.
Repeat after successful retirement parent sync and deferred-cleanup ownership release with
the old retired tree intact; creation may then publish a distinct root/control inode pair.

### Whole-database removal crash matrices

Use separate process-termination and machine-crash/durable-prefix matrices. At every
boundary, record final/retired names, retained identities, lock release/ownership, semantic
result, and whether cleanup was permitted.

| Boundary | Process-death procedure and next action | Machine-crash durable outcome |
|---|---|---|
| before ownership | terminate before claim/lock; retry as a fresh operation | complete live `D` |
| after ownership, before retirement rename | terminate owner; next actor reacquires/adopts | complete live `D` |
| after rename, before retirement parent sync | terminate without cleanup; next actor reconciles exact external-parent namespace | live `D` or recognized complete retired root; never partially deleted content |
| after retirement parent sync | terminate before cleanup | old `D` durably absent; retired root durable |
| during recursive cleanup, before other-entry durability | terminate after selected contained unlinks while control is retained | unsynchronized retired content may reappear; retry requires control authority; old `D` remains absent |
| after other-entry durability, before control unlink | terminate after successful control-only validation | only the expected control entry remains; normal control-authorized cleanup can finish |
| after control unlink, before retired-root sync | terminate before control-unlink durability acknowledgement | control may survive or be absent; no other content can reappear; choose control-present or empty-terminal retry |
| after retired-root sync, before rmdir | terminate after durable control absence | empty no-control root; only terminal finalization is authorized |
| after retired-root `rmdir`, before final parent sync | terminate before residue-durability acknowledgement | retired name may reappear after machine crash; semantic removal remains complete |
| after final parent sync | terminate after physical-completion barrier | retired residue durably absent |

For each prefix, retain a pre-removal structural manifest of the captured root. If restart
finds the captured old root inode at live `D`, assert its files/directories and contents were
not partially deleted by whole-root removal and that the event log contains no cleanup
operation. If restart finds a retired root, it must match the exact retired grammar and never
become an ordinary open target. Absence may be acknowledged as semantic removal only where
the retirement parent-sync barrier succeeded.

### Contained retired-root reclamation and recreation

First prove the authority distinction. Add an unknown regular file and unknown directory to
a live supported-v1 root; ordinary §4.7.6 orphan classification must preserve both. In a
separate fixture, acquire exclusive removal authority, durably retire the root, and run
private cleanup; the same unknown contained forms may be deleted without catalog ownership
classification.

Exercise containment with sentinels:

- place a symlink in the retired root pointing to an external file and directory; cleanup
  unlinks only the symlink entry and both external targets remain byte-for-byte intact;
- where safely supported, add a FIFO and local socket and verify only their contained
  entries are removed; privileged device-node creation is not required;
- place unrelated files/directories and another database beside `D` and the retired sibling;
  compare their identities and contents before/after cleanup;
- in an isolated integration harness, place a mounted/cross-device directory beneath the
  retired root and require traversal refusal with cleanup left pending. Where mount setup is
  unavailable, inject a child filesystem/device-identity mismatch at the descent decision
  and assert no child open/unlink occurs.

Inject cleanup failure after durable retirement. The API-independent semantic result must
remain successful removal of old `D`; no rename-back occurs, the exact retired root remains
recognized cleanup residue, and no background-cleanup promise is assumed. A fresh
`RemoveDatabase(D)` with no live `D` follows the existing missing/non-database result rather
than re-reporting capture of the old database; explicit cleanup may still target the exact
retired artifact.

Build partially reclaimed trees retaining recognizable control and retry explicit cleanup
after ordinary failure and after process crash. Observe reacquisition of namespace gate,
local claim, and control ownership, destructive-identity validation and parent durability
before mutation. Already-absent entries are accepted idempotently; remaining entries are
removed bottom-up under retained no-follow descriptors. No catalog/WAL/recovery service is
constructed, and no live `D` or external sibling is touched. Crash-restored entries before
the terminal barrier remain control-authorized private residue. No-control fixtures use
the separate terminal procedure below, never this recursive retry expectation.

After retired-root `rmdir`, crash once before and once after the external-parent `fsync`.
Before that sync the retired name may reappear; afterward physical residue absence is
durable. Semantic removal remains successful in both cases because retirement publication
preceded cleanup.

For recreation isolation, keep the old retired root, create and durably publish a new `D`,
and record distinct root/control inodes. Continue old cleanup using only descriptors bound to
the old retired root. Place sentinel data in the new root and prove cleanup cannot resolve
through or mutate it. If convenient, allocate equal numeric database-local FileIds in the
old and new roots and confirm no cross-root lookup; inode/root isolation remains the required
oracle even when equal FileIds are not induced. Direct open or generic discovery of the old
retired basename must always reject it as a live database.

The root-removal result matrix is:

| Condition | Semantic result | Retirement mutation | Private cleanup |
|---|---|---:|---:|
| owner contention | `DATABASE_BUSY` | forbidden | forbidden |
| missing/unrecognizable root or wrong control type | `NOT_A_DATABASE`/existing missing-target result | forbidden | forbidden |
| unreadable identity input | `IO_OR_DURABILITY_FAILURE` | forbidden | forbidden |
| unsupported future control format | `UNSUPPORTED_DATABASE_FORMAT` | forbidden | forbidden |
| recognizable damaged v1 after ownership/adoption | eligible to proceed | allowed | only after durable retirement |
| adoption sync/identity failure | existing identity or `IO_OR_DURABILITY_FAILURE` | forbidden | forbidden |
| retirement rename failure | `IO_OR_DURABILITY_FAILURE` | not published | forbidden |
| retirement parent-sync failure | `IO_OR_DURABILITY_FAILURE`; durability uncertain | rename may be runtime-visible but removal is unacknowledged | forbidden |
| cleanup failure after durable retirement | semantic removal remains successful; cleanup residue is diagnostic/pending | already published | retry only through the matching control-present or empty-terminal authority path |

### Durable empty terminal root finalization

For Architecture §4.7.9, build a supported-control retired root with independently
enumerated nested directories, managed files, unknown contained entries, and
external sibling sentinels. Record a test-owned manifest, the expected control
inode, namespace/control ownership events, all unlink/rmdir events, directory
sync completion, and exact root/parent identities. Drive reclamation one barrier
at a time. Before permitting control unlink, independently assert that every
other entry's removal has a successful covering durability operation and the
immediately preceding directory enumeration contains only that owned control
entry. A final empty tree alone cannot prove this ordering.

Activate a crash/fault at each recursive unlink/sync, the terminal enumeration,
control unlink, retired-root sync, root rmdir, and external-parent sync. Require
the injection log to identify the exact reached boundary. Use the durable-prefix
model to enumerate survivors from completed operations, independently of the
cleaner's own classification: before the other-entry barrier, control survives;
after it, no other content can reappear. After control unlink, restart in both
allowed control-present and empty/no-control states. Inject failure of each sync:
no premature physical-absence acknowledgement is permitted, and semantic removal
does not revert. Deliberately suppress a covering sync or unlink control early
in the negative fixture; the event oracle must reject the execution even if a
normal process restart happens to see an empty tree.

For empty/no-control restart, independently construct the exact retired name in
an isolated parent. Observe gate acquisition, no-follow retained-root checks,
empty enumeration, root synchronization, parent-entry identity revalidation,
empty rmdir, and parent sync. Require no recursive deletion, replacement control
creation, recovery, or new durable authority record. Pair it with:

- a nonempty/no-control root containing a sentinel file, including a fabricated
  reserved name: refusal preserves the manifest;
- a nonempty root with wrong-type, unreadable, unrecognizable, or unsupported
  control: gate ownership does not permit recursive reclamation;
- an externally inserted entry before terminal enumeration or after the empty
  check but before rmdir: transition/removal refuses and preserves that entry;
- a parent-entry replacement with a distinct root inode before identity
  revalidation: prove that the check observes the mismatch, and that refusal
  preserves the replacement tree without recursive fallback. This is an
  observed-change diagnostic under the scope described in **Independent
  final-root adoption**, not a claim of exclusion against an undetected external
  replacement after the last check;
- an unsafe link or filesystem boundary: no followed target is changed.

Run two finalizers for the same exact retired target using explicit gate-attempt
barriers. Prove both contenders actually run and only one owns the gate at a time.
After one completes, the other observes absence idempotently and does not recreate
the root/control or re-remove a live `D`; its durability result still has a parent
sync. Kill the first after rmdir but before parent sync and exercise both allowed
name survivors in the crash model. Also retain a distinct new live `D` and compare
its inode/content manifest plus external sentinels throughout finalization.
Compose the gate/descriptor procedures, destructive-format matrix, and no-follow
containment oracles rather than using the cleaner's success label as evidence.

### Control slots and required recovery inputs

This subsection is the detailed persistent-codec and selection-verification owner for
[`ARCHITECTURE.md`](../ARCHITECTURE.md) §§13.2–13.3. It specializes the independent-oracle
rules already used for WAL without replacing the byte layout, allocator, checkpoint, or
open semantics owned by Architecture. Numeric Exhaustion and Terminal-Boundary Verification
remains the owner of terminal allocator behavior; the Crash Injection Framework remains the
owner of checkpoint-installation and torn-publication prefixes.

#### Independent byte, endian, CRC, and decode oracles

Build each candidate slot in a test-owned 4096-byte array initialized to zero. Write every
field from the mathematical fixture value by explicit byte shifts, not by copying a host
structure or calling a production control encoder. Assemble the complete file by placing
slot 0 at file bytes `0..4095` and slot 1 at `4096..8191`, then assert a file length of
exactly 8192 bytes. Independently reject 4095-byte and 4097-byte slot representations and
whole files shorter or longer than 8192 bytes; a malformed whole-file frame cannot be made
canonical by finding a plausible slot prefix inside it.

The endian oracle compares every uint16, uint32, and uint64 byte individually against
least-significant-byte-first expectations. The decode oracle reads those bytes with a
separate test-side routine and compares the resulting values with the fixture constants.
A production encode/decode round trip is useful additional coverage but is never the byte
or semantic oracle because both directions may share the same offset or endian defect.

The CRC oracle uses an independent CRC32C implementation or independently generated fixed
vectors. First require the standard check value:

```text
CRC32C("123456789") = 0xE3069283
```

Then compute over all 4096 slot bytes while supplying logical zero for offsets `80..83`,
regardless of the stored checksum bytes, and store the result little-endian at offset 80.
Whole-slot CRC detects torn or random changes; exact reserved-zero and relational checks
separately enforce the supported-v1 grammar, so a checksum-valid malformed v1 slot remains
invalid.

Every primary negative fixture changes exactly one independent contract. Recompute and
store a correct CRC after changing a semantic field or reserved byte; retain every semantic
field when testing the CRC itself. This prevents checksum failure from masking a format or
relational validator. Expected classifications come from §§4.14 and 13.2, never from the
production decoder's returned category or validator execution order.

#### Distinctive valid-slot byte fixture

The canonical byte-oracle vector uses the following legal non-palindromic values. Its
checkpoint-bearing form is paired with independently framed retained WAL containing a
complete checkpoint whose BEGIN and END records occur at the listed aligned record starts,
whose END redo value is `0x0000000001020308`, and whose DPT justifies that redo bound. The
slot-level vector can also be tested before the checkpoint usability layer so those two
validation stages remain observable.

| Offset | Width | Field | Encoding | Canonical value/domain | Positive fixture / expected bytes | Single-defect negative fixture | Oracle |
|---:|---:|---|---|---|---|---|---|
| 0 | 8 | family magic | exact ASCII | `DBLUSCTL` | `44 42 4C 55 53 43 54 4C` | changed, shifted, lowercase, or embedded-NUL magic, one case at a time | byte comparison before version dispatch |
| 8 | 2 | `format_version` | LE uint16 | `1` | `01 00` | `0`; separately `2` for future dispatch | byte/endian and version-class oracle |
| 10 | 2 | `header_size` | LE uint16 | `88` | `58 00` | `0`, `87`, `89`, and another representable value separately | exact supported-v1 grammar |
| 12 | 4 | `flags` | LE uint32 | `0` | `00 00 00 00` | one assigned bit set | exact zero mask with recomputed CRC |
| 16 | 8 | `generation` | LE uint64 | `0x0102030405060708` | `08 07 06 05 04 03 02 01` | zero | byte/endian plus nonzero semantic check |
| 24 | 8 | `latest_checkpoint_lsn` | LE uint64 LSN | `0x0000000011223348` | `48 33 22 11 00 00 00 00` | zero while either other checkpoint field remains nonzero | byte/endian, triplet, then referenced-WAL oracle |
| 32 | 8 | `latest_checkpoint_end_lsn` | LE uint64 LSN | `0x0000000022334458` | `58 44 33 22 00 00 00 00` | value below BEGIN | byte/endian, relation, then referenced-WAL oracle |
| 40 | 8 | `checkpoint_redo_lsn` | LE uint64 LSN | `0x0000000001020308` | `08 03 02 01 00 00 00 00` | zero while BEGIN/END remain nonzero | byte/endian, triplet, then END/DPT oracle |
| 48 | 8 | `reserved_txn_id_end` | LE uint64 exclusive end | `1,048,578 = 2 + 2^20` | `02 00 10 00 00 00 00 00` | `1` | byte/endian plus §9.3 reservation oracle |
| 56 | 8 | `txn_status_reclaim_before` | LE uint64 exclusive cutoff | `97,922 = 2 + 3 * 32,640` | `82 7E 01 00 00 00 00 00` | below 2, above reservation end, or misaligned, separately | byte/endian plus §13.2.3 relation oracle |
| 64 | 4 | `next_file_id` | LE uint32 next value | `0x12345678` | `78 56 34 12` | zero | byte/endian plus FileId-domain oracle |
| 68 | 4 | `reserved32` | four zero bytes | `0` | `00 00 00 00` | one bit set | exact reserved-zero check with recomputed CRC |
| 72 | 8 | `next_catalog_object_id` | LE uint64 next value | `0x1122334455667788` | `88 77 66 55 44 33 22 11` | zero | byte/endian plus shared-ID-domain oracle |
| 80 | 4 | `crc32c` | LE uint32 | independent whole-slot checksum | `0x9673EF23` -> `23 EF 73 96` | one stored bit changed | independent CRC oracle; field logically zero during computation |
| 84 | 4 | `reserved32` | four zero bytes | `0` | `00 00 00 00` | one bit set | exact reserved-zero check with recomputed CRC |
| 88 | 4008 | reserved suffix | exact zero byte sequence | every byte zero | `00` repeated 4008 times through byte 4095 | set byte 88, a middle byte, or byte 4095, separately | full-range byte scan with recomputed CRC |

The positive oracle compares all 4096 bytes, not only the listed nonzero runs, and then
compares an independently decoded semantic object with every chosen value. The expected
checksum `0x9673EF23` is derived from exactly this vector, including 4008 suffix zeros and
logical zeros at bytes `80..83` during calculation.

#### Canonical initial-file fixture

Construct the fresh-control state independently from §13.2.2 rather than by invoking a
create path. Slot 0 uses generation 1, three zero checkpoint fields,
`reserved_txn_id_end=2`, `txn_status_reclaim_before=2`, `next_file_id=1`, and
`next_catalog_object_id=1`; all flags/reserved bytes are zero. Its independent CRC32C is
`0x530BD55D`, stored as `5D D5 0B 53`. Slot 1 is exactly 4096 zero bytes and is a
noncandidate, not a reason to reject the valid initial file.

| File region / fact | Exact initial fixture | Independent assertion |
|---|---|---|
| file framing | 8192 bytes | no short/long/trailing data accepted |
| slot 0 | canonical v1 bytes at `0..4095` | full byte comparison; CRC `5D D5 0B 53` |
| checkpoint state | BEGIN/END/redo all zero | no installed checkpoint relation accepted |
| allocator state | TxnId end 2, reclaim cutoff 2, FileId next 1, catalog-object next 1 | decoded values equal the four architecture constants |
| slot 1 | zero at every byte `4096..8191` | invalid/noncandidate slot without corrupting slot 0 |
| selected state | slot 0, generation 1 | open/control selection has one semantic authority |

#### Field grammar, checkpoint usability, and high-water procedures

Magic tests compare all eight bytes exactly. Mutate one byte, shift the sequence within the
slot, use lowercase, and inject an embedded NUL as separate fixtures; no fuzzy prefix or
substring match establishes ownership. Contrast a wrong-magic candidate paired with a valid
v1 slot against an exact-magic positive version greater than 1 paired with an older valid
v1 slot. The former permits ordinary independent-slot fallback; the latter returns
`UNSUPPORTED_DATABASE_FORMAT` before supported-slot fallback. This fail-closed future
dispatch prevents an owner from selecting and later overwriting older state after a
recognized newer owner has published control metadata.

For supported version 1, vary `header_size`, `flags`, generation, both reserved32 fields,
and the three suffix locations independently. Version zero is an invalid/corrupt claimed
control slot. Generation zero is likewise invalid; generation `UINT64_MAX` is a valid
selected terminal value, but every operation requiring another control publication must
fail before wrap, slot write, or dependent state publication. The existing control-
generation procedure under Numeric Exhaustion and Terminal-Boundary Verification remains
the no-wrap owner.

Checkpoint testing has two explicit layers:

1. **Structural slot validity.** Accept exactly the all-zero no-checkpoint triplet, or three
   nonzero fields with `latest_checkpoint_lsn <= latest_checkpoint_end_lsn`. Reject each
   mixed-zero combination and END-before-BEGIN with a recomputed valid CRC.
2. **Recovery usability.** For a structurally valid checkpoint-bearing slot, independently
   decode the referenced retained BEGIN/DATA/END sequence and apply §§13.5–13.9: record
   starts and cross-links, complete contiguous DATA indexes, totals, CRC/framing, END redo
   value, and valid WAL range must agree. Test a complete sequence, an incomplete or
   malformed sequence, an unaligned/non-record-start pointer, and a pointer beyond valid WAL.

A newer structurally valid but checkpoint-unusable slot may fall back to an older candidate
only when all recovery objects referenced by the older candidate are retained and valid.
The checkpoint codecs in WAL Persistent Codec Verification and the checkpoint boundaries in
the Crash Injection Framework remain the complete sequence and installation procedures;
this subsection supplies their exact control-slot bytes and selection input.

Exercise high-water fields with the following matrix. “Persisted maximum” is a legal
terminal next-value state; the next allocation/update is the separately tested exhaustion
operation, not persisted wraparound. TxnId writers additionally prove exact `2^20` block
progression even though slot validation's local lower-bound check is stated separately.

| Field / case | Values | Byte-valid? | Relationally valid? | Slot usable? | Required observation / exhaustion owner |
|---|---|---:|---:|---:|---|
| TxnId initial minimum | `reserved_txn_id_end=2` | yes | yes | yes | no range reserved yet |
| TxnId normal block | `1,048,578 = 2 + 2^20` | yes | yes | yes | durable exact block precedes issue; lost suffix is never reused |
| TxnId maximum block end | `18,446,744,073,708,503,042` | yes | yes | yes | next exact block returns `TXN_ID_EXHAUSTED` without partial block or wrap |
| TxnId below minimum | `reserved_txn_id_end=1` | yes | no | no | invalid supported-v1 slot |
| reclaim initial | cutoff `2`, reservation end `2` | yes | yes | yes | first normal boundary |
| reclaim normal aligned | cutoff `32,642 = 2 + 32,640`, reservation end at least cutoff | yes | yes | yes | one complete status-page range retired at slot level |
| reclaim maximum aligned under terminal reservation | cutoff `18,446,744,073,708,474,242`; reservation end `18,446,744,073,708,503,042` | yes | yes | yes | largest representable page-aligned cutoff not above authority |
| reclaim below minimum | cutoff `1` | yes | no | no | invalid supported-v1 slot |
| reclaim misaligned | cutoff `3`, reservation end greater | yes | no | no | modulo-32,640 relation rejected |
| reclaim beyond reservation | cutoff `32,642`, reservation end `2` | yes | no | no | cutoff cannot outrun reserved TxnId authority |
| FileId initial/normal | `next_file_id=1` / another nonzero value | yes | yes | yes | durable next value precedes returned candidate |
| FileId exhausted next state | `next_file_id=UINT32_MAX` | yes | yes | yes | last returned ID was `UINT32_MAX-1`; next request returns `FILE_ID_EXHAUSTED` |
| FileId invalid | `next_file_id=0` | yes | no | no | invalid supported-v1 slot |
| catalog ID initial/normal | `next_catalog_object_id=1` / another nonzero value | yes | yes | yes | one shared TableId/IndexId/ConstraintId sequence |
| catalog ID exhausted next state | `next_catalog_object_id=UINT64_MAX` | yes | yes | yes | last returned ID was `UINT64_MAX-1`; next request returns `ID_EXHAUSTED` |
| catalog ID invalid | `next_catalog_object_id=0` | yes | no | no | invalid supported-v1 slot |

The byte fixture proves representation and slot-level relations. The persistent high-water
crash matrix proves durable consumption and nonreuse; full status-history reclamation proof
remains with the Chapter-14 procedures rather than being inferred from a control value.

#### CRC and format-classification matrices

| CRC fixture | Independent expected CRC | Stored CRC | CRC-valid? | Semantic-valid? | Required result |
|---|---|---|---:|---:|---|
| distinctive canonical bytes | `0x9673EF23` | `0x9673EF23` | yes | yes | valid candidate, subject to checkpoint usability |
| covered field bit flipped, checksum unchanged | independently changed `C'` | `0x9673EF23` | no | otherwise yes | invalid candidate / CRC corruption |
| stored CRC bit flipped | `0x9673EF23` | one-bit-different value | no | otherwise yes | invalid candidate / CRC corruption |
| semantic field invalid, CRC recomputed | independently changed `C'` | same `C'` | yes | no | invalid supported-v1 candidate; checksum does not legalize grammar |
| suffix byte invalid, CRC recomputed | independently changed `C'` | same `C'` | yes | no | invalid supported-v1 candidate; proves full suffix validation |
| checker includes stored bytes instead of logical zeros | `0x9673EF23` under canonical rule | implementation's self-referential result | no oracle agreement | otherwise yes | test fails; canonical slot is not redefined by the wrong calculation |

The format matrix uses “fallback” only for another independently valid/usable supported-v1
slot. A short/long whole file fails control-file framing even if one prefix looks plausible.

| Fixture | Owning format recognized? | CRC valid under applicable grammar? | v1 semantic grammar valid? | Candidate valid/usable? | Fallback allowed? | Required result |
|---|---:|---:|---:|---:|---:|---|
| canonical v1 slot | yes | yes | yes | yes, then checkpoint usability | N/A | accept candidate |
| wrong magic | no | N/A | N/A | no | yes | invalid/non-owning candidate; lifecycle identity rule decides if none remain |
| exact magic, version 0 | yes | valid v1-shaped checksum in isolated fixture | no | no | yes | invalid/corrupt candidate |
| exact magic, positive version greater than 1 | yes | not interpreted with v1 CRC | N/A | no | no | `UNSUPPORTED_DATABASE_FORMAT` |
| wrong `header_size` | yes | yes | no | no | yes | invalid/corrupt candidate |
| flags nonzero | yes | yes | no | no | yes | invalid/corrupt candidate |
| generation zero | yes | yes | no | no | yes | invalid/corrupt candidate |
| checkpoint triplet structurally inconsistent | yes | yes | no | no | yes | invalid/corrupt candidate |
| checkpoint sequence missing/malformed/beyond valid WAL | yes | yes | yes at slot layer | no for recovery | yes, only to retained valid older authority | control/checkpoint corruption if no usable candidate |
| `reserved_txn_id_end < 2` | yes | yes | no | no | yes | invalid/corrupt candidate |
| reclaim cutoff below 2, misaligned, or above reservation | yes | yes | no | no | yes | invalid/corrupt candidate |
| `next_file_id=0` | yes | yes | no | no | yes | invalid/corrupt candidate |
| reserved field at 68 nonzero | yes | yes | no | no | yes | invalid/corrupt candidate |
| `next_catalog_object_id=0` | yes | yes | no | no | yes | invalid/corrupt candidate |
| stored CRC invalid | yes | no | otherwise yes | no | yes | invalid/corrupt candidate |
| reserved field at 84 nonzero | yes | yes | no | no | yes | invalid/corrupt candidate |
| any suffix byte nonzero | yes | yes | no | no | yes | invalid/corrupt candidate |
| 4095/4097-byte standalone slot or non-8192-byte file | insufficient canonical framing | N/A | no | no | no whole-file salvage | control framing/open failure |
| two valid slots, equal generation, identical bytes | yes | yes | yes | equivalent authority | N/A | deterministic semantic acceptance; no physical-slot choice is observable |
| two valid slots, equal generation, different legal contents | yes | yes | yes individually | no unique authority | no | `CORRUPT_DATABASE`/control corruption |

#### Slot selection, update, and crash procedures

Construct each slot independently, then assemble the pair in both physical orders where
applicable. Selection is by supported validity, checkpoint usability, and generation—not by
slot number.

| Slot 0 | Slot 1 | Selected authority | Open allowed? | Classification / reason |
|---|---|---|---:|---|
| valid generation G | all-zero initial noncandidate | slot 0 G | yes | canonical initial/one-valid case |
| valid G | valid G+1 | slot 1 G+1 | yes | highest valid usable generation |
| valid G+1 | valid G | slot 0 G+1 | yes | physical position is irrelevant |
| valid G | CRC-invalid current-v1 candidate | valid G | yes | ordinary independent-slot fallback |
| valid G | semantic-invalid current-v1 candidate | valid G | yes | ordinary independent-slot fallback |
| invalid current-v1 candidate | invalid current-v1 candidate | none | no | no usable control authority; no default recreation |
| valid G, bytes X | valid G, identical bytes X | semantic state X; either copy equivalent | yes | equal generation has one byte-identical authority |
| valid G, legal bytes X | valid G, different legal bytes Y | none | no | equal-generation split authority is corruption |
| valid older v1 G | exact-magic positive future version | none | no | `UNSUPPORTED_DATABASE_FORMAT`; downgrade fallback forbidden |
| valid G | wrong-magic candidate | valid G | yes | non-owning invalid candidate differs from future owner |
| older valid/usable G | newer structurally valid but checkpoint-unusable G+1 | older G only if all its objects remain retained and valid | conditional | descending usable-candidate rule; otherwise control/checkpoint corruption |

Map alternating publication to the persistent high-water crash matrix and add byte-exact
assertions at each boundary:

```text
select the nonselected physical slot
derive every unchanged field from the latest selected in-memory state
checked generation + 1
encode the complete 4096-byte vector and independent expected CRC
one exact-position 4096-byte write
successful fdatasync(database.control)
only then runtime publication of the new generation
```

Pause before the write, before the CRC field, inside the reserved suffix, after all bytes
but before known synchronization, after successful synchronization, and before/after runtime
publication. A partial slot is invalid and the old slot remains authority. If a crash occurs
after complete bytes but before the caller knows synchronization, select only from the bytes
that independently survive as a complete valid slot; either old or new authority is legal
according to that exact durable image, never according to the interrupted call's return path.
The old slot remains a fallback until a complete newer slot survives. Two independently
validated slots are necessary because a torn candidate update must not destroy the previous
durable authority.

The control-write event trace also proves §13.3: ordinary transaction begin/commit/abort
within an already reserved TxnId block causes no control write or sync, while TxnId block
reservation, FileId/catalog-object allocation, and successful checkpoint installation use
the serialized alternating path. The test observes architecture events and does not require
a particular mutex or production function name.

#### Cross-owner exhaustion and checkpoint mappings

The existing procedures remain primary owners for these dimensions:

| Control-field/protocol concern | Existing detailed procedure | Byte-exact specialization supplied here |
|---|---|---|
| generation terminal value | Numeric Exhaustion — control generation | exact generation bytes, valid `UINT64_MAX` selected slot, and no attempted wrapped slot |
| FileId terminal allocation | Numeric Exhaustion — FileId specialization | exact `next_file_id` bytes and zero-field corruption case |
| shared catalog-object terminal allocation | Numeric Exhaustion — shared catalog-object IDs | exact uint64 bytes, zero-field corruption, and one shared carrier |
| TxnId durable reservation | Numeric Exhaustion — TxnId terminal block and persistent crash matrix | exact `reserved_txn_id_end` bytes and relations to reclaim cutoff |
| alternating/torn update | Persistent high-water crash procedure and Crash Injection Framework | complete independent slot bytes, CRC, physical placement, and partial-prefix cases |
| checkpoint record validity | WAL persistent checkpoint codecs | exact control triplet bytes and structural-versus-usable distinction |
| checkpoint installation | Crash Injection Framework and shutdown/recovery lifecycle procedures | WAL-durable-before-control and control-sync-before-runtime-selection assertions |

#### Atomic control-file obligation coverage map

The inventory below splits §§13.2–13.3 when a separately corruptible byte range,
classification, durability boundary, or observable allocator result exists. No target count
is imposed. This subsection or the named existing owner must supply an
independent deterministic procedure, not merely that Architecture states the rule.

| # / domain | Atomic obligation | Architecture owner | Verification owner | Deterministic procedure/reference |
|---:|---|---|---|---|
| 1 A | control basename is exactly `database.control` | §13.2 | this subsection — file fixture | exact owner-path fixture |
| 2 A | complete control file is exactly 8192 bytes | §13.2 | independent byte oracle | exact/short/long whole-file fixtures |
| 3 A | file contains exactly two slots | §13.2 | independent byte oracle | two-region assembly and no third/trailing region |
| 4 A | slot 0 and slot 1 occupy `0..4095` and `4096..8191` | §13.2 | independent byte oracle | absolute-offset comparison |
| 5 A | each slot representation is exactly 4096 bytes | §13.2.1 | independent byte oracle | 4095/4096/4097 cases |
| 6 B | family magic occupies bytes `0..7` exactly | §13.2.1 | slot-format matrix | exact ASCII and four isolated mutations |
| 7 B | format version occupies bytes `8..9` | §13.2.1 | slot-format matrix | distinctive LE uint16 vector |
| 8 B | header size occupies bytes `10..11` and equals 88 | §13.2.1 | slot-format/classification matrices | 0/87/88/89 cases |
| 9 B | flags occupy bytes `12..15` and equal zero | §13.2.1 | slot-format matrix | recomputed-CRC one-bit mutation |
| 10 B | generation occupies bytes `16..23` | §13.2.1 | distinctive vector | independent LE uint64 bytes |
| 11 B | checkpoint BEGIN LSN occupies bytes `24..31` | §13.2.1 | distinctive vector / checkpoint procedure | independent LE uint64 bytes |
| 12 B | checkpoint END LSN occupies bytes `32..39` | §13.2.1 | distinctive vector / checkpoint procedure | independent LE uint64 bytes |
| 13 B | checkpoint redo LSN occupies bytes `40..47` | §13.2.1 | distinctive vector / checkpoint procedure | independent LE uint64 bytes |
| 14 B | reserved TxnId end occupies bytes `48..55` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint64 bytes |
| 15 B | status reclaim cutoff occupies bytes `56..63` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint64 bytes |
| 16 B | next FileId occupies bytes `64..67` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint32 bytes |
| 17 B | bytes `68..71` are reserved zero | §13.2.1 | slot-format matrix | one-bit defect with valid recomputed CRC |
| 18 B | next catalog-object ID occupies bytes `72..79` | §13.2.1 | distinctive vector / high-water matrix | independent LE uint64 bytes |
| 19 B | CRC field occupies bytes `80..83` | §13.2.1 | CRC matrix | exact expected LE CRC bytes |
| 20 B | bytes `84..87` are reserved zero | §13.2.1 | slot-format matrix | one-bit defect with valid recomputed CRC |
| 21 B | bytes `88..4095` are all zero | §13.2.1 | slot-format matrix | first/middle/final isolated defects |
| 22 C | every uint16 is explicit little-endian | §13.2.1 | endian oracle | non-palindromic uint16 values |
| 23 C | every uint32 is explicit little-endian | §13.2.1 | endian oracle | non-palindromic uint32 values, including CRC |
| 24 C | every uint64 is explicit little-endian | §13.2.1 | endian oracle | non-palindromic uint64 values |
| 25 C | codec does not depend on host struct layout/endianness | §§4.2, 13.2.1 | byte oracle | byte-shift builder and full-array comparison |
| 26 C | decoded semantic values are independently checked | §13.2.1 | decode oracle | test-side read compared with chosen constants |
| 27 D | CRC32C covers all 4096 slot bytes | §13.2.1 | CRC oracle | distinctive fixed vector |
| 28 D | CRC bytes are logically zero during calculation | §13.2.1 | CRC oracle | canonical-versus-self-referential calculation |
| 29 D | CRC implementation is independent | §13.2.1 | CRC oracle | standard check vector plus test-side implementation |
| 30 D | stored CRC is little-endian | §13.2.1 | slot-format/CRC matrices | `23 EF 73 96` and initial `5D D5 0B 53` |
| 31 D | covered-byte or stored-CRC corruption is detected | §§13.2.1, 13.2.3 | CRC matrix | separate payload/stored-bit flips |
| 32 D | CRC-valid malformed v1 remains invalid | §§4.14, 13.2.3 | single-defect policy / CRC matrix | semantic mutation with recomputed CRC |
| 33 E | exact magic plus version 1 selects v1 grammar | §§4.14.2, 13.2.3 | format-classification matrix | canonical positive slot |
| 34 E | exact magic plus version 0 is invalid/corrupt | §§4.14.2, 13.2.3 | format-classification matrix | isolated zero-version slot |
| 35 E | exact magic plus positive future version is unsupported | §§4.14.2, 4.14.6, 13.2.3 | format-classification matrix | version-2 discriminator fixture |
| 36 E | future owning version blocks fallback to older | §§4.14.6, 13.2.3 | slot-selection matrix | future-plus-valid-v1 pair |
| 37 E | wrong magic/current-v1 invalidity differs from future ownership | §§4.14.6, 13.2.3 | magic procedure / selection matrix | wrong-magic-plus-valid-v1 contrast |
| 38 F | supported-v1 flags and all reserved bytes are exact zero | §§4.14.3, 13.2.1–13.2.3 | slot-format matrix | isolated recomputed-CRC mutations |
| 39 F | generation must be nonzero | §13.2.3 | format-classification matrix | generation-zero fixture |
| 40 F | highest valid usable generation is selected | §13.2.3 | slot-selection matrix | G/G+1 in both positions |
| 41 F | physical slot number does not determine authority | §13.2.3 | slot-selection matrix | reversed generation pair |
| 42 F | equal-generation identical slots are one equivalent authority | §13.2.3 | slot-selection matrix | byte-identical valid pair |
| 43 F | equal-generation different contents are corruption | §13.2.3 | slot-selection matrix | CRC-correct legal X/Y pair |
| 44 F | generation never wraps | §§4.3.2, 13.2.3 | Numeric Exhaustion — control generation | selected `UINT64_MAX`, next update rejected |
| 45 G | no-checkpoint triplet is exactly all zero | §13.2.3 | checkpoint structural procedure | all-zero positive and mixed-zero negatives |
| 46 G | installed-checkpoint triplet is entirely nonzero | §13.2.3 | checkpoint structural procedure | complete positive and each mixed-zero case |
| 47 G | checkpoint BEGIN is not after END | §13.2.3 | checkpoint structural procedure | END-before-BEGIN fixture |
| 48 G | checkpoint-bearing slot is usable only with a valid retained sequence | §§13.2.3, 13.5–13.9 | checkpoint usability procedure / WAL codec | complete, malformed, absent, and out-of-tail cases |
| 49 G | older fallback requires all referenced objects retained and valid | §13.2.3 | slot-selection matrix / Crash Injection Framework | usable and unusable older-candidate variants |
| 50 H | initial slot 0 has the exact canonical field values | §13.2.2 | initial-file matrix | full 4096-byte vector and CRC |
| 51 H | initial slot 1 is all zero and a noncandidate | §13.2.2 | initial-file matrix | full 4096-byte zero comparison |
| 52 I | reserved TxnId end is at least 2 | §13.2.3 | high-water matrix | values 2 and 1 |
| 53 I | TxnId reservation uses durable exact blocks and forbids reuse | §§9.3, 13.2.4 | Numeric Exhaustion — TxnId / high-water mapping | first, normal, terminal, and crash-gap cases |
| 54 I | reclaim cutoff is at least 2 | §13.2.3 | high-water matrix | values 2 and 1 |
| 55 I | reclaim cutoff does not exceed reserved TxnId end | §13.2.3 | high-water matrix | isolated beyond-end fixture |
| 56 I | reclaim cutoff obeys `(value-2) mod 32640 = 0` | §13.2.3 | high-water matrix | aligned and misaligned fixtures |
| 57 I | next FileId is nonzero | §§13.2.3, 13.2.5 | high-water matrix | values 1 and 0 |
| 58 I | FileId next value is durable before return and never reused | §13.2.5 | Numeric Exhaustion — persistent high-water | crash before/after sync and terminal state |
| 59 I | next catalog-object ID is nonzero | §§13.2.3, 13.2.6 | high-water matrix | values 1 and 0 |
| 60 I | catalog IDs share one durable no-reuse sequence | §13.2.6 | Numeric Exhaustion — shared catalog IDs | alternating typed requests and crash gaps |
| 61 J | one valid and one zero/invalid slot selects the valid slot | §§13.2.2–13.2.3 | slot-selection matrix | zero, CRC-invalid, semantic-invalid variants |
| 62 J | two invalid slots provide no default/recreated authority | §13.2.3 | slot-selection matrix / lifecycle open faults | paired invalid fixtures |
| 63 J | checkpoint-unusable newer candidate uses descending fallback rules | §13.2.3 | slot-selection/checkpoint usability procedures | newer bad reference plus older variants |
| 64 K | updates target the nonselected slot | §13.2.4 | alternating update procedure | physical-slot event trace |
| 65 K | update derives from latest state so unrelated fields are preserved | §13.2.4 | alternating update procedure | concurrent distinct-field transitions |
| 66 K | update uses checked old generation plus one | §§13.2.3–13.2.4 | alternating update / Numeric Exhaustion | normal and terminal generation cases |
| 67 K | update encodes a complete slot and canonical CRC | §13.2.4 | alternating update / byte oracle | prewrite vector comparison |
| 68 K | control write is one exact-position 4096-byte transfer request | §13.2.4 | alternating update procedure | exact/short transfer instrumentation |
| 69 K | control file is synchronized before runtime publication | §13.2.4 | alternating update / crash framework | barriers before/after `fdatasync` |
| 70 K | torn candidate preserves prior valid authority | §13.2.4 | alternating update procedure | stops before CRC/in suffix and reopen |
| 71 K | complete uncertain write is judged from surviving bytes, not call outcome | §§13.2.3–13.2.4 | persistent high-water crash procedure | exact surviving old/new images |
| 72 K | concurrent control transitions are serialized without lost fields | §13.2.4 | alternating update procedure | TxnId/FileId/checkpoint/reclaim contenders |
| 73 L | generation exhaustion prevents dependent publication | §§4.3.2, 13.2.3 | Numeric Exhaustion — control generation | selected maximum fixture |
| 74 L | FileId exhaustion preserves terminal next value | §§4.3.2.1, 13.2.5 | Numeric Exhaustion — FileId | last candidate and next failure |
| 75 L | catalog-object exhaustion preserves terminal next value | §§4.3.2.1, 13.2.6 | Numeric Exhaustion — shared IDs | last candidate and next failure |
| 76 L | TxnId exact-block exhaustion preserves nonreuse | §§4.3.2.1, 9.3 | Numeric Exhaustion — TxnId | terminal block and next failure |
| 77 M | ordinary transactions do not rewrite/sync control within a reserved block | §13.3 | update-frequency event trace | ordinary begin/commit/abort positive control |
| 78 M | global high-water/checkpoint transitions use durable control publication | §13.3 | update-frequency event trace / existing crash owners | each listed transition routed through alternating path |
| 79 N | checkpoint WAL is durable before control installation | §13.5 | Crash Injection Framework / checkpoint codec | before/after END durability boundary |
| 80 N | control sync precedes in-memory checkpoint/FPI publication | §§13.2.4, 13.5 | Crash Injection Framework / alternating update | before/after control sync and runtime epoch boundary |
| 81 O | short/long file or slot framing cannot be salvaged | §§13.2–13.2.1 | independent byte oracle | exact 4095/4097 and 8191/8193 fixtures |
| 82 O | corrupt supported-format state, unsupported future format, and checkpoint-unusable state remain distinct | §§4.14, 13.2.3 | classification and selection matrices | three isolated pair/file fixtures |


#### Required recovery inputs

Separately remove, replace, truncate, or corrupt each architecture-required recovery input:

```text
required WAL segment/range
immutable bootstrap locator and bootstrap system files
catalog.dat
txn_status.dat
files required by recovered committed catalog state
```

Assert that missing or invalid required state never reaches READY or becomes an orphan
shortcut. For recoverable torn transaction-status state, instrument status lookup and prove
that recovery repair and loser resolution complete before ordinary status lookup is
enabled.

### Open fault matrix and READY admission

Exercise a failure or process crash on both sides of every stable transition in §3.3.3's
ordered open protocol, using §3.3.7 for the required structured result. For each point:

1. inject the selected validation, allocation, I/O, synchronization, worker-start, or
   publication failure;
2. assert no transaction, statement, maintenance task, ordinary page/file operation, or
   normal background task is admitted before the single READY publication;
3. assert no partly recovered status/catalog/ownership state is visible;
4. inspect and cleanly account for every resource acquired before the failure;
5. retry open/recovery when the architecture permits a later owner to do so.

Failed-open cleanup assertions include stopped/joined recovery workers, quiesced and
destroyed partial BufferPool state, released guards/pins, closed file registrations and
descriptors, released logical gates/locks, and removal of the process-local identity claim
only after the OS lock descriptor is released. If any worker, guard, append/publication
outcome, or cleanup result cannot be established, assert `NONCONTINUABLE` and retained
exclusivity rather than a false `CLOSED` result.

### Orphan classification

Create exact managed pending/final names in every architecture-defined required,
retired, and unowned state, plus unrelated names and unsafe object types. Run open and
recovery classification, then assert:

- proven database-owned orphans follow the namespace reconciliation/cleanup protocol;
- required committed/bootstrap files are never reclassified as disposable orphans;
- unknown names are recorded/ignored and never deleted by pattern guessing;
- an orphan cleanup failure remains a retryable cleanup task when §3.3 permits READY, while
  failure of synchronization needed for required WAL/status/control/namespace state blocks
  READY.

### Shutdown, draining, and failure injection

Drive `READY -> DRAINING` while transactions occupy each runtime state:

```text
ACTIVE
MUST_ABORT
COMMITTING
ABORTING
```

Require the exact outcomes in §§3.3.6, 15.5, and 15.6. Assert that DRAINING immediately
closes transaction, statement, maintenance, page/file, and scheduled-background admission;
already-admitted terminal and cancellation work may use only the internal facilities that
the shutdown protocol keeps alive.

Instrument each ordered shutdown step and verify this dependency sequence:

```text
close admission
cancel/drain ordinary statements and maintenance
complete required transaction terminal publication and release
stop producer/background services while retaining durability services
quiesce and drain BufferPool guards, pins, writebacks, and dirty pages
append, flush, and install the final checkpoint/control slot
complete required namespace cleanup and directory synchronization
stop BufferPool helpers, then the WAL/group-commit service
destroy managers/descriptors, release the OS lock last, publish CLOSED
```

Inject page-flush, WAL synchronization, checkpoint construction/installation, control-slot
update, namespace mutation, and directory-sync failures independently. For every failure,
assert the returned lifecycle result, retained durable interpretation, database health,
lock lifetime, and next-open recovery behavior. A required failure must never report
successful close, discard an unflushed dirty frame as durable, publish a clean marker, or
reinterpret a durable COMMIT as ABORTED.

### NONCONTINUABLE and durable-commit preservation

Induce `NONCONTINUABLE` through append uncertainty, failed post-append publication, failed
exact restoration, and lifecycle teardown uncertainty. Assert atomically that:

- no new transaction, statement, maintenance operation, ordinary page/file load, WAL
  append, writeback, checkpoint, or ordinary mutation begins;
- in-flight operations fail/cancel at safe ownership boundaries without exposing protected
  provisional state;
- exclusivity remains held until dependency-ordered non-clean teardown is safe or the
  process exits;
- non-clean close does not flush uncertain frames, install a final checkpoint, recycle WAL,
  or claim successful shutdown;
- a subsequent owner performs the full recovery protocol.

For every lifecycle failure after a transaction's commit record becomes durable, reopen
and verify that the transaction is COMMITTED. Repeat with runtime terminal-cache
publication failure, cleanup failure, failed shutdown, and process restart. No lifecycle
or cache error may append, publish, report, or recover that TxnId as ABORTED.

---

## Statement Failure and Transaction-State Tests

Directly exercise every reachable row of the normative statement-error matrix in
`ARCHITECTURE.md` §39.1.3 on both sides of §39.1.2's first transaction-owned published-write
boundary. The architecture owns the exact `FAILED_TRANSACTION_REMAINS_ACTIVE`,
`FAILED_TRANSACTION_MUST_ABORT`, and `DATABASE_NONCONTINUABLE` classifications; tests own
the setup and observations.

For each matrix cell:

1. begin an explicit transaction and one identified CommandId;
2. arrange for the selected error before the boundary, then repeat after a controlled
   transaction-owned mutation has published where that failure remains reachable;
3. record the statement/client error, runtime transaction state, COMMIT eligibility,
   automatic ABORT behavior, database health, and retained logical ownership;
4. inspect logical visibility after terminal handling and after reopen/recovery;
5. verify that the original causal diagnostic is preserved if cleanup adds a stronger
   transaction/database failure.

The matrix includes effect-free user/read errors, parse/bind/type/cast/expression and
constraint errors, cancellation, lock cancellation, deadlock/serialization failures,
`OutOfMemory`, spill failures, persistent-page corruption, known and uncertain
WAL/BufferPool/page I/O outcomes, DDL namespace failure, ANALYZE failure, and invariant
failure. Do not assign one generic transaction outcome to all memory, disk, or I/O errors;
use the exact §39.1 row and the lower-layer §12.12 result.

### Partial DML and exact local rollback

Use explicit tests for:

- a five-row INSERT whose ordinary candidate closure succeeds, then a controlled
  dynamic resource/I/O failure occurs during the fifth mutation after four rows
  publish;
- UPDATE failure after at least one replacement version, old-version marker, or index
  effect publishes;
- DELETE failure after at least one `xmax/cmax` publishes;
- equivalent target-spool, assignment, constraint, OOM, and spill failures before any
  transaction-owned mutation publishes.

Use V31-B/C/G's closure and publication event ledgers to prove each fault's actual
phase and activation. The post-publication UPDATE/DELETE cases likewise require a
dynamic cause excluded from ordinary closure under Architecture §15.1.3. Pair the
INSERT fixture with an ordinary error on the fifth demanded input occurrence;
that error must be found during closure with no published row prefix. An
implementation that publishes four rows before discovering that ordinary error
fails the negative control. Do not force an unreachable post-W ordinary-error case
merely to populate the transaction matrix.

Assert the architecture-defined statement result and transaction transition, prohibit
COMMIT when required, and verify that no successful row prefix becomes logically committed.
Physically retained aborted versions/index entries are allowed only where the architecture
classifies them as transaction garbage.

Test §12.12 exact local rollback in two transactions: one with no earlier successful
statement write and one with an earlier statement-owned published write. Capture and compare
the full provisional physical state as described above. Exact rollback of the failing
primitive must not itself set the current statement's published-write flag, but it also must
not erase an earlier published effect or reset transaction-wide persistent-write state.

Publish an empty page or pure allocation/shape/FSM record with no transaction-owned logical
tuple/index/catalog/statistics effect, then inject a later statement error. Assert that
§39.1.2 classifies the first-published-write boundary by logical transaction ownership, not
by the existence of any WAL record or physical page change. Repeat with a record that does
publish a logical row/index effect to prove the distinction.

### CommandId, snapshot, retry, and RETURNING

Verify all retry rules in §§9.6, 9.9, and 39.1.4:

- every admitted failed statement consumes its CommandId;
- all internal pre-write retries of one logical statement retain that CommandId;
- `FAILED_TRANSACTION_REMAINS_ACTIVE` unregisters the failed READ COMMITTED snapshot and
  the next statement obtains a fresh snapshot;
- transparent same-TxnId retry occurs only while no transaction-owned write has published;
- once publication occurs, retry is forbidden and the required abort path runs;
- no ordinary statement or new gate begins in `MUST_ABORT`, `COMMITTING`, or `ABORTING`.

For DML `RETURNING`, separately inject a retained-result/spool failure during
pre-W closure after earlier return values have been staged, and a dynamic write or
result-readiness failure at its actual reached boundary after W. Reuse V31-H/I to
prove closure/W/C0/R ordering; ordinary RETURNING expression errors cannot first
appear after W. An explicit-transaction failure must expose no partial successful
prefix. In autocommit, retain the complete result through implicit COMMIT C5 and release it
only with the successful post-commit response. Inject pre-durable commit failure and
post-durable connection/transport failure and assert client visibility separately from the
durable transaction outcome. Operator-level spool and Halloween behavior remains covered by
the DML execution section.

---

## COMMIT Fault-Injection Tests

Exercise every C0-C6 boundary defined by `ARCHITECTURE.md` §§15.5 and 39.1.5. Instrument
entry to and completion of each boundary rather than treating “before/after commit flush” as
the complete matrix.

For each injection, record independently:

```text
valid WAL append end and surviving commit record
durable_lsn relative to commit_lsn
resident transaction-status page state
runtime transaction state and terminal cache
database health / NONCONTINUABLE state
logical locks, gates, snapshots, and derived-cache ownership
client acknowledgement or uncertainty
recovery outcome after process restart
```

Required categories are:

- C0/C1 precondition or resource failure before the terminal protocol;
- C2 terminal-record construction and known no-append failure with exact restoration;
- C2 uncertain append outcome;
- valid commit append followed by status-page/frame publication failure;
- first and repeated C3 WAL write/`fdatasync` failure while COMMITTING ownership is retained;
- connection loss before append, after append but before durability, and after durability;
- C4 runtime terminal-state/cache publication failure after durable commit;
- C5 catalog/statistics cache installation or safe-invalidation failure, lock/registry
  cleanup failure, and incoherent cleanup escalation;
- C6 successful acknowledgement and acknowledgement transport failure.

Known append failure and uncertain append outcome must be separate test paths. A validly
appended but not durable commit is uncancellable: cancellation or connection loss must not
redirect it to ABORT. Repeated group-commit flush failure must leave every transaction with
a valid commit record in the architecture-defined COMMITTING/noncontinuable handling, not
selectively abort unacknowledged waiters.

At and after C3, assert the invariant directly:

```text
durable TXN_COMMIT => semantic outcome COMMITTED forever
```

Post-durable runtime publication, cache, cleanup, lock-release, shutdown, and transport
failures may change database health or client knowledge, but must never create or report
ABORTED for that TxnId. Recovery must reach the same COMMITTED outcome.

---

## ABORT Fault-Injection Tests

Exercise every A0-A4 boundary defined by `ARCHITECTURE.md` §§15.6 and 39.1.6. Cover explicit
ROLLBACK, automatic abort from `MUST_ABORT`, deadlock-victim abort, and connection-loss abort.

Inject independently:

```text
A0 state transition and abort-record preparation
A1 PAGE_IMAGE/TXN_ABORT construction and known append failure
A1 append-outcome uncertainty and post-append status-page publication failure
WAL write/failure paths for an appended abort record
A2 runtime ABORTED publication failure
A3 logical-lock/gate/snapshot/orphan/cache cleanup failure
A4 acknowledgement or connection failure
```

For each case assert the original causal error, transaction state, persisted WAL/status
evidence, terminal-cache state, retained/released ownership, database health, client result,
and recovery outcome. Known pre-publication failures retain `ABORTING` ownership and retry
as prescribed; uncertainty or inability to publish safely becomes `NONCONTINUABLE`.
Transaction locks and gates release only after runtime ABORTED publication. Cleanup or
acknowledgement failure must not restore ACTIVE/COMMIT eligibility or turn ABORTED into
COMMITTED.

---

## Recovery Property Tests

For random transaction workloads:

1. execute operations,
2. crash at random instrumented points,
3. reopen/recover,
4. compare logical committed contents against a model that includes only transactions whose commit records became durable.

Repeat across many seeds.

Physical garbage is allowed.

Logical committed results must match.

---

## MVCC Visibility Tests

This section is the detailed procedural owner for `ARCHITECTURE.md` §§10.1–10.6.
It verifies the visibility decision and its error boundary using Chapter 5's validated
physical tuple metadata and Chapter 9's snapshot/status inputs. Transaction-status lookup
precedence remains owned by Transaction identity, snapshot, and status verification;
write-conflict/current-owner decisions remain §11.10.4 and the Locking/UNIQUE sections;
physical reclamation remains the Vacuum and Reclamation Tests.

### Deterministic harness, fixture canonicality, and independent oracle

The table-driven visibility harness controls these architecture-level inputs without
requiring a production function name or return type:

```text
tuple xmin / xmax / cmin / cmax
physical tuple/RID owner and persisted-versus-runtime-only provenance
snapshot owner_txn_id / command_id / xmax / sorted active
canonical Status(xmin/xmax) result or one injected lower-layer failure
scan source: direct heap candidate, sequential scan, or B+ candidate RID
```

The observable result domain is exactly:

```text
VISIBLE
INVISIBLE
ERROR with the canonical error family
```

An implementation may represent that result with a status return, tagged/expected-like
value, or another compatible mechanism. Tests assert semantics and exact propagated error,
not a C++ type, exception choice, or helper name.

Every fixture is canonical except for the single dimension under test. A semantic-status
fixture has a valid HEAP owner, checksum, PageId, NORMAL slot, complete §§5.7–5.13 tuple,
normal referenced TxnId, valid snapshot, and canonical TXN_STATUS/runtime lookup inputs. A
future-command fixture introduces no unrelated status or structure failure. An index case
uses a valid B+ path, exact relation/RID identity, and a structurally valid heap tuple. A
malformed tuple-header case is instead routed to the existing Storage Verification / §4.13.3
validation oracle before Chapter-10 semantic evaluation.

The independent reference evaluator uses mathematical fixture values rather than calling
the production visibility routine. It performs, in order:

1. structural/owner precondition classification by the existing heap validator;
2. SELF command-causality checks, including future fields and same-owner `cmax < cmin`;
3. creator classification as visible, invisible, or exact error;
4. deleter classification only for a visible creator, as visible, invisible, or exact
   error;
5. exact error propagation to the scan/query owner.

The causality precheck occurs before creator invisibility can short-circuit. Status lookup
calls are instrumented so the oracle can also require that FROZEN/SELF cases bypass
persistent lookup and that a lower-layer error is neither converted to a Boolean nor
followed by a different status source. Concurrency, retry, and status-publication fixtures
use deterministic barriers; sleeps and repeated random reproduction are insufficient.

### Creator and deleter valid-state regression

Use direct values around one valid snapshot, for example owner `50`, command `C=10`,
`xmax=100`, and controlled sorted `active`. Use separate TxnIds for every status source so
one lookup result cannot accidentally satisfy another row.

The creator matrix is:

| Creator fixture | Status/snapshot relation | Expected creator result | Deleter consulted? |
|---|---|---|---:|
| `xmin=FROZEN_TXN_ID` | no status-page access | VISIBLE | yes |
| `xmin=owner`, `cmin=C-1` | earlier SELF command | VISIBLE | yes |
| `xmin=owner`, `cmin=C` | current SELF command | INVISIBLE | no, after causality precheck |
| other creator | COMMITTED, `xmin < xmax`, absent from `active` | VISIBLE | yes |
| other creator | COMMITTED, `xmin < xmax`, present in `active` | INVISIBLE | no |
| other creator | COMMITTED, `xmin == xmax` | INVISIBLE | no |
| other creator | COMMITTED, `xmin > xmax` | INVISIBLE | no |
| other creator | IN_PROGRESS | INVISIBLE | no |
| other creator | ABORTED | INVISIBLE | no |

The active-at-capture COMMITTED case first captures the TxnId in `active`, then publishes
COMMITTED through the Chapter-9 barrier. The immutable snapshot must retain membership and
the creator remains invisible. This distinguishes terminal status from snapshot-visible
history.

For every creator-visible row, apply the complete deleter matrix:

| Deleter fixture | Status/snapshot relation | Expected tuple result |
|---|---|---|
| `xmax=INVALID_TXN_ID` | canonical no-delete sentinel; no lookup | VISIBLE |
| `xmax=owner`, `cmax=C-1` | earlier SELF command | INVISIBLE |
| `xmax=owner`, `cmax=C` | current SELF command | VISIBLE |
| other deleter | COMMITTED, `xmax < snapshot.xmax`, absent from `active` | INVISIBLE |
| other deleter | COMMITTED, `xmax < snapshot.xmax`, present in `active` | VISIBLE |
| other deleter | COMMITTED, `xmax >= snapshot.xmax` | VISIBLE |
| other deleter | IN_PROGRESS | VISIBLE; no visibility wait |
| other deleter | ABORTED | VISIBLE |

Compose every creator-visible normal case with each deleter row and separately assert that
creator-invisible normal cases remain invisible without status-dependent deleter lookup.
The causality precheck still runs first, so impossible SELF deleter metadata cannot be hidden
by this short-circuit. These tables preserve every normal Boolean result while the error
tables below exercise the non-Boolean branches.

### Status-result and lookup-failure propagation

Construct separate creator and creator-visible/deleter fixtures for each unusable status.
The TXN_STATUS decoder must succeed for `INVALID` and `RESERVED`; semantic rejection occurs
only after Chapter-9 precedence returns that decoded result to visibility. `RETIRED` is
constructed only by a durably published Chapter-14 cutoff; the negative fixture then
retains a visibility-dependent tuple reference in deliberate violation of the retirement
proof.

| Side | Controlled result/failure | Provenance | Expected result/error | May be treated as invisible/ineffective? | Continuation owner |
|---|---|---|---|---:|---|
| creator | RETIRED | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| creator | INVALID | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| creator | RESERVED | recognized v1 bits plus persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| creator | status-page I/O failure | lower layer | propagate exact I/O error | no | §39.1 and lower owner |
| creator | checksum/owner/corruption failure | lower layer | propagate exact corruption error | no | §39.1 and §§4.13–4.14 |
| creator | recognizable future format | lower layer | propagate exact unsupported-format error | no | §§4.14, 39.1 |
| creator | recovery/storage failure | lower layer | propagate exact lower-layer error | no | Chapters 13 and 39 |
| deleter | RETIRED | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| deleter | INVALID | persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| deleter | RESERVED | recognized v1 bits plus persisted dependent tuple | ERROR `CORRUPT_HEAP` | no | §39.1 |
| deleter | status-page I/O failure | lower layer | propagate exact I/O error | no | §39.1 and lower owner |
| deleter | checksum/owner/corruption failure | lower layer | propagate exact corruption error | no | §39.1 and §§4.13–4.14 |
| deleter | recognizable future format | lower layer | propagate exact unsupported-format error | no | §§4.14, 39.1 |
| deleter | recovery/storage failure | lower layer | propagate exact lower-layer error | no | Chapters 13 and 39 |

For each lookup failure, record one call and the exact returned diagnostic. The creator
case must not become INVISIBLE; the deleter case must not become an ineffective delete and
VISIBLE tuple. The §39.1 first-published-write matrix, rather than this visibility harness,
decides whether a propagated operation failure leaves the transaction active, mandates
ABORT, or makes storage noncontinuable.

The sentinel/status distinction has direct coverage:

| Condition | Decoder/lookup result | Valid? | Visibility consequence |
|---|---|---:|---|
| tuple `xmax == INVALID_TXN_ID` | no status lookup | yes | no deleter; creator-visible tuple is VISIBLE |
| normal `xmin` resolves INVALID | valid `00` decode, required outcome absent | no semantic state | `CORRUPT_HEAP` |
| normal `xmax` resolves INVALID | valid `00` decode, required outcome absent | no semantic state | `CORRUPT_HEAP` |

The RESERVED fixture first proves successful `11` decode through the Chapter-9 byte
oracle, then passes the recognized nonterminal result to creator/deleter visibility and
expects `CORRUPT_HEAP`. It must not report malformed bits or unsupported format. Conversely,
a recognizable future status-file/page format fails during format dispatch and propagates
its exact unsupported-format result; it is not relabeled `CORRUPT_HEAP` merely because no
visibility decision can be made.

The compact error/visibility summary prevents normal Boolean cases and error cases from
sharing an oracle:

| Input class | Side | Expected semantic result | Exact error family | May be treated as ordinary invisible/ineffective? | Architecture owner |
|---|---|---|---|---:|---|
| valid COMMITTED before snapshot | creator | VISIBLE | — | no | §10.2 |
| valid IN_PROGRESS | creator | INVISIBLE | — | yes | §10.2 |
| valid ABORTED | creator | INVISIBLE | — | yes | §§10.1–10.2 |
| valid COMMITTED delete before snapshot | deleter | INVISIBLE tuple | — | yes, as an effective delete | §10.3.3 |
| valid IN_PROGRESS | deleter | VISIBLE tuple | — | yes, as an ineffective delete | §10.3.3 |
| valid ABORTED | deleter | VISIBLE tuple | — | yes, as an ineffective delete | §§10.1, 10.3.3 |
| dependent RETIRED | either | ERROR | persisted `CORRUPT_HEAP` | no | §10.4 / §14.14 |
| dependent INVALID | either | ERROR | persisted `CORRUPT_HEAP` | no | §10.4 |
| dependent RESERVED | either | ERROR after valid decode | persisted `CORRUPT_HEAP` | no | §§9.11.1, 10.4 |
| lower-layer lookup failure | either | ERROR | exact lower-layer family | no | §10.4 / §39.1 |
| SELF `cmin>C` | creator | ERROR | persisted `CORRUPT_HEAP` or runtime invariant | no | §§10.2, 10.4 |
| SELF `cmax>C` | deleter | ERROR | persisted `CORRUPT_HEAP` or runtime invariant | no | §§10.3.2, 10.4 |
| same-owner `cmax<cmin` | precheck | ERROR | persisted `CORRUPT_HEAP` or runtime invariant | no | §10.4 |

### SELF command boundaries and causal precheck

Use explicit `C=10` fixtures rather than arithmetic loops. Separate checked boundary cases
cover CommandId zero/maximum through Numeric Exhaustion and Terminal-Boundary Verification;
no fixture computes `C-1` or `C+1` at an overflow boundary.

| Field | Relation to `C` | Valid? | Ordinary visibility meaning | Invalid error |
|---|---|---:|---|---|
| `cmin=9` | `< C` | yes | SELF creator visible | — |
| `cmin=10` | `== C` | yes | SELF creator invisible to same-command rescan | — |
| `cmin=11` | `> C` | no | no Boolean result | persisted `CORRUPT_HEAP`; runtime-only invariant failure |
| `cmax=9` | `< C` | yes | SELF delete effective; old tuple invisible | — |
| `cmax=10` | `== C` | yes | SELF delete not yet effective; old tuple visible | — |
| `cmax=11` | `> C` | no | no Boolean result | persisted `CORRUPT_HEAP`; runtime-only invariant failure |

Run each invalid relation once as a canonical persisted NORMAL tuple and once through
abstract test-only instrumentation immediately before invalid runtime metadata could be
published. The first must use `CORRUPT_HEAP`; the second must enter §39.1.3's internal
invariant path. No public unsafe-construction API is required.

The same-transaction causality matrix is:

| Case | `xmin` owner? | `xmax` owner? | `cmin` | `cmax` | `C` | Valid? | Expected result |
|---|---:|---:|---:|---:|---:|---:|---|
| earlier insert | yes | no | 9 | — | 10 | yes | creator visible |
| current insert | yes | no | 10 | — | 10 | yes | creator invisible |
| current-command delete of earlier version | no | yes | — | 10 | 10 | yes | old tuple visible |
| later-command delete of self-created version | yes | yes | 8 | 9 | 10 | yes | old tuple invisible |
| future creator | yes | no | 11 | — | 10 | no | error before Boolean result |
| future deleter | no | yes | — | 11 | 10 | no | error before Boolean result |
| delete precedes creation | yes | yes | 10 | 9 | 10 | no | error before creator-current invisibility short-circuit |

The last fixture is the ordering oracle: creator evaluation alone would return INVISIBLE
because `cmin == C`, but the required result is `CORRUPT_HEAP`. This proves command-causality
validation precedes semantic short-circuiting. Do not add causal prohibitions beyond those
listed by §10.4 and the compatible §11.10.4 current-owner rule.

### Statement effects and UPDATE version pairs

Direct physical-pair tests use two NORMAL RIDs and evaluate each candidate independently;
ordinary visibility does not infer a logical row identity from `prev`. Current-command
INSERT (`cmin=C`) is invisible to ordinary rescan, while an earlier-command INSERT
(`cmin<C`) is visible. Current-command DELETE (`cmax=C`) leaves the old tuple visible,
while an earlier-command DELETE (`cmax<C`) hides it.

The mandatory UPDATE matrix is:

| Updater relation | Status/snapshot relation | Old `xmax` interpretation | New `xmin` interpretation | Old visible? | New visible? | Emitted ordinary version |
|---|---|---|---|---:|---:|---|
| SELF current command | owner, `cmax=C`, `cmin=C` | current delete ineffective | current creator hidden | yes | no | old |
| SELF later command | owner, `cmax<C`, `cmin<C` | earlier delete effective | earlier creator visible | no | yes | new |
| other IN_PROGRESS | status IN_PROGRESS | delete ineffective | creator invisible | yes | no | old |
| other COMMITTED before snapshot | TxnId below `xmax`, absent from `active` | delete effective | creator visible | no | yes | new |
| other COMMITTED at/after `xmax` | TxnId `>=xmax` | delete too new | creator too new | yes | no | old |
| other active at capture, later COMMITTED | TxnId below `xmax`, retained in `active` | delete too new to snapshot | creator invisible to snapshot | yes | no | old |
| other ABORTED | status ABORTED | delete ineffective | creator invisible | yes | no | old |

Each row asserts exactly one emitted ordinary version. The active-at-capture case pauses
commit after snapshot capture and proves later terminal publication does not rewrite the
captured set. Aborted cases retain both physical versions and index entries to prove logical
selection does not require physical undo.

### Isolation, retry, recovery, and retirement cross-checks

The existing Snapshot registration, representation, and lifetime procedures plus Isolation
Tests remain the detailed isolation owners. Add Chapter-10 result assertions at their
existing barriers:

| Case | Existing procedure | Chapter-10 result oracle |
|---|---|---|
| READ COMMITTED stable attempt | Chapter-9 RC snapshot lifetime | one snapshot for all tuples/operators; a commit after capture does not appear mid-attempt |
| READ COMMITTED next statement | Isolation Tests | fresh snapshot may expose the later commit |
| READ COMMITTED own writes | SELF matrix | earlier-command writes visible; current-command ordinary rescan hidden as specified |
| allowed pre-write retry | Statement Failure / Chapter-9 retry fixture | fresh snapshot, same logical CommandId, no published SELF metadata to trigger a future-command error |
| conflict after published write | Statement Failure and Transaction-State Tests | no same-TxnId retry; canonical MUST_ABORT/ABORT path |
| REPEATABLE READ first statement | Chapter-9 RR fixture | first ordinary statement captures the transaction snapshot |
| REPEATABLE READ later statements | Isolation Tests | external snapshot fields stable; later external commits remain invisible |
| REPEATABLE READ own later writes | SELF matrix | updated command boundary exposes own earlier commands without changing external membership |
| isolation identity | Chapter-9 isolation identity / Isolation Tests | REPEATABLE READ is snapshot isolation, not SERIALIZABLE |

Recovery Property Tests use known WAL prefixes and add direct tuple outcomes: a crash loser
creator is ABORTED/invisible; a crash loser deleter is ABORTED/ineffective; a durable
committed updater is classified from reconstructed terminal authority without requiring
heap/status-page force; and no precrash active registry or terminal cache is consulted after
reopen. The old/new matrix is rerun after READY against a newly captured snapshot.

Vacuum and Reclamation Tests remain the positive proof owner for §§14.13.1–14.14.3. They
must show that aborted-`xmax` normalization and creator freezing preserve visibility, that
their page mutations follow the existing WAL/page-LSN procedure, and that status retirement
is published only after all surviving visibility-dependent metadata is independent. The
negative RETIRED fixture above deliberately violates that proof and must fail; a missing
status page alone must never synthesize RETIRED. SQL invisibility is not global
reclaimability, and a persisted HEAP `DEAD` slot is not merely a synonym for an invisible
NORMAL version.

### Heap and index candidate error propagation

Sequential scans validate each candidate NORMAL tuple through Chapter 5/§4.13.3 and then
apply the same visibility oracle under the statement's one registered snapshot. A semantic
visibility error terminates through the canonical query/§39 path; it is not a skipped row.
All operators/subqueries in one statement share the same snapshot and command boundary as
the existing Scan, Pipeline, and Subquery tests require.

For B+ scans, §8.22.2 remains authoritative: an index hit is a physical RID candidate. The
index/recheck matrix is:

| B+ candidate | Heap visibility result | Scan/query action | May silently skip? | Expected error |
|---|---|---|---:|---|
| valid RID | VISIBLE | emit decoded row | no | — |
| valid RID | INVISIBLE | skip this physical candidate and continue | yes | — |
| valid RID | creator/deleter RETIRED | stop and propagate | no | `CORRUPT_HEAP` |
| valid RID | creator/deleter INVALID | stop and propagate | no | `CORRUPT_HEAP` |
| valid RID | creator/deleter RESERVED | stop and propagate | no | `CORRUPT_HEAP` |
| valid RID | status lookup I/O/corruption/unsupported failure | stop and propagate | no | exact lower-layer error |
| valid RID | future SELF `cmin/cmax` or invalid causal order | stop and propagate | no | persisted `CORRUPT_HEAP` |

Repeat the error rows through an ordinary sequential scan. Include valid later candidates
after the failing one and assert none are emitted as a successful continuation. An
INVISIBLE or ERROR result for one physical candidate must not cause ordinary visibility to
follow `prev`; ERROR especially must not become “try predecessor.” Chapter 5 owns the
persisted link and Chapter 14 owns maintenance/splicing/reuse. Randomized MVCC and scan
stress remains complementary to these deterministic fixtures.

### Chapter 10 high-level domain/case matrix

| Domain/case | Deterministic fixture/oracle | Architecture reference | Verification owner |
|---|---|---|---|
| creator normal truth table | direct creator matrix plus active-at-capture barrier | §§10.2, 10.6 | MVCC Visibility Tests |
| creator unusable status | canonical RETIRED/INVALID/RESERVED inputs | §§10.2, 10.4 | Status propagation matrix |
| deleter normal truth table | creator-visible tuple crossed with deleter matrix | §§10.3–10.4 | MVCC Visibility Tests |
| deleter unusable status | canonical RETIRED/INVALID/RESERVED inputs | §§10.3.3–10.4 | Status propagation matrix |
| lookup error propagation | one injected lower-layer error per side | §10.4 | Status propagation matrix |
| future `cmin` | persisted/runtime-only `C+1` | §§10.2, 10.4 | SELF command matrix |
| future `cmax` | persisted/runtime-only `C+1` | §§10.3.2, 10.4 | SELF command matrix |
| same-owner causal order | `cmin=C`, `cmax=C-1` precheck oracle | §§10.4, 10.6 | Causality matrix |
| current/later INSERT | SELF `cmin==C` and `<C` | §10.2 | Statement-effects fixtures |
| current/later DELETE | SELF `cmax==C` and `<C` | §10.3.2 | Statement-effects fixtures |
| UPDATE old/new pair | all seven updater rows | §§10.1–10.4 | UPDATE matrix |
| READ COMMITTED | stable attempt, next statement, retry | §§9.9, 10.2–10.4 | Snapshot and Isolation Tests cross-reference |
| REPEATABLE READ | retained external snapshot plus SELF writes | §§9.10, 10.2–10.4 | Snapshot and Isolation Tests cross-reference |
| index candidate recheck | visible/invisible/error rows | §§8.22.2, 10.4 | Index recheck matrix / Scan Tests |
| heap scan | valid NORMAL candidates with Boolean/error outcomes | §§5.17, 10.4 | Heap scan propagation fixture |
| recovery | loser/committed updater after READY | §§10.1, 13.13–13.19 | Recovery Property Tests cross-reference |
| retirement boundary | positive proof plus dependent-RETIRED negative | §§10.4–10.5, 14.13–14.14 | Vacuum/Reclamation cross-reference |
| no predecessor fallback | invisible and error candidates with valid `prev` | §§5.7.4, 10.4, 14.10 | Heap/index propagation fixture |

### Chapter 10 architecture-obligation coverage map

A deterministic procedure
or a precise existing procedural owner supplies an independent oracle; an architecture
statement alone is not coverage.

| # | Domain | Atomic obligation | Architecture owner | Verification owner / procedure/reference |
|---:|---|---|---|---|
| 1 | STRUCTURAL PRECONDITION | A candidate is a validated NORMAL tuple before semantic visibility | §§4.13.3, 5.17, 10.4 | Fixture canonicality plus Storage Verification |
| 2 | STRUCTURAL PRECONDITION | `xmin` structural domain is FROZEN or normal TxnId | §§4.13.3, 5.7.1 | Heap validation cross-reference |
| 3 | STRUCTURAL PRECONDITION | `xmax` domain is INVALID sentinel or normal non-FROZEN TxnId | §§4.13.3, 5.7.2 | Heap validation cross-reference |
| 4 | STRUCTURAL PRECONDITION | Snapshot owner, command, horizon, and active inputs are canonical | §§9.7–9.8, 10.4 | Chapter-9 snapshot fixture |
| 5 | STRUCTURAL PRECONDITION | Structural/owner failure precedes status semantics | §10.4 | Malformed-header routing fixture |
| 6 | STRUCTURAL PRECONDITION | Visibility observable domain is VISIBLE/INVISIBLE/ERROR | §10.4 | Harness result-domain assertion |
| 7 | STRUCTURAL PRECONDITION | API representation remains implementation-free | §10.4 | Semantic-result assertion only |
| 8 | SELF COMMAND CAUSALITY | SELF causality validation precedes creator short-circuit | §10.4 | `cmin=C,cmax=C-1` ordering oracle |
| 9 | CREATOR / XMIN | FROZEN creator is visible without status lookup | §§10.2, 10.6 | Creator matrix plus lookup-call assertion |
| 10 | CREATOR / XMIN | SELF creator with `cmin<C` is visible | §10.2 | SELF creator trichotomy |
| 11 | CREATOR / XMIN | SELF creator with `cmin==C` is invisible | §10.2 | SELF creator trichotomy |
| 12 | SELF COMMAND CAUSALITY | SELF creator with `cmin>C` is error | §§10.2, 10.4 | Future-cmin persisted/runtime fixtures |
| 13 | CREATOR / XMIN | Other creator consumes canonical Chapter-9 status lookup | §§9.13, 10.2 | Controlled lookup source/assertion |
| 14 | CREATOR / XMIN | COMMITTED creator below `xmax` and absent from active is visible | §10.2 | Creator normal matrix |
| 15 | CREATOR / XMIN | COMMITTED creator captured active remains invisible after commit | §§9.7.2, 10.2 | Active-at-capture barrier |
| 16 | CREATOR / XMIN | COMMITTED creator at `xmax` is invisible | §10.2 | Exact equality fixture |
| 17 | CREATOR / XMIN | COMMITTED creator above `xmax` is invisible | §10.2 | Above-horizon fixture |
| 18 | CREATOR / XMIN | Other IN_PROGRESS creator is invisible | §10.2 | Creator normal matrix |
| 19 | CREATOR / XMIN | ABORTED creator is invisible while bytes may remain | §§10.1–10.2 | Creator matrix plus retained-bytes fixture |
| 20 | CREATOR / XMIN | Normal creator invisibility suppresses deleter semantic lookup | §§10.2, 10.4 | Instrumented short-circuit fixture |
| 21 | CREATOR STATUS ERROR | Dependent creator RETIRED is persisted corruption | §§10.2, 10.4 | Creator RETIRED fixture |
| 22 | CREATOR STATUS ERROR | Dependent creator INVALID is persisted corruption | §§10.2, 10.4 | Creator INVALID fixture |
| 23 | CREATOR STATUS ERROR | Creator RESERVED decodes but is semantic corruption | §§9.11.1, 10.2, 10.4 | Decoder-then-visibility fixture |
| 24 | ERROR PROPAGATION | Creator status I/O failure propagates exactly | §10.4 | Creator lookup-failure matrix |
| 25 | ERROR PROPAGATION | Creator status corruption/ownership failure propagates exactly | §10.4 | Creator lookup-failure matrix |
| 26 | ERROR PROPAGATION | Creator unsupported format propagates distinctly | §§4.14, 10.4 | Future-format dispatch fixture |
| 27 | ERROR PROPAGATION | Creator recovery/storage failure is not Boolean invisibility | §10.4 | Lower-layer injected-failure fixture |
| 28 | DELETER / XMAX | `xmax=INVALID_TXN_ID` is valid no-delete sentinel | §10.3.1 | Deleter normal/sentinel matrix |
| 29 | DELETER / XMAX | SELF deleter with `cmax<C` hides tuple | §10.3.2 | SELF deleter trichotomy |
| 30 | DELETER / XMAX | SELF deleter with `cmax==C` leaves tuple visible | §10.3.2 | SELF deleter trichotomy |
| 31 | SELF COMMAND CAUSALITY | SELF deleter with `cmax>C` is error | §§10.3.2, 10.4 | Future-cmax persisted/runtime fixtures |
| 32 | DELETER / XMAX | COMMITTED deleter below `xmax` and absent from active is effective | §10.3.3 | Deleter normal matrix |
| 33 | DELETER / XMAX | COMMITTED deleter captured active is ineffective | §10.3.3 | Active-at-capture deleter fixture |
| 34 | DELETER / XMAX | COMMITTED deleter at/above `xmax` is ineffective | §10.3.3 | At/above-horizon fixtures |
| 35 | DELETER / XMAX | Other IN_PROGRESS deleter is ineffective without visibility wait | §10.3.3 | Deleter normal matrix |
| 36 | DELETER / XMAX | ABORTED deleter is ineffective while `xmax` may remain | §§10.1, 10.3.3 | Deleter matrix plus retained-header fixture |
| 37 | DELETER STATUS ERROR | Dependent deleter RETIRED is persisted corruption | §§10.3.3–10.4 | Deleter RETIRED fixture |
| 38 | DELETER STATUS ERROR | Dependent deleter INVALID is persisted corruption | §§10.3.3–10.4 | Deleter INVALID fixture |
| 39 | DELETER STATUS ERROR | Deleter RESERVED decodes but is semantic corruption | §§9.11.1, 10.3.3–10.4 | Decoder-then-visibility fixture |
| 40 | ERROR PROPAGATION | Deleter lookup I/O/corruption/unsupported/storage failure propagates | §10.4 | Deleter lookup-failure matrix |
| 41 | DELETER STATUS ERROR | INVALID status is distinct from INVALID_TXN_ID sentinel | §§10.3.1, 10.3.3 | Explicit sentinel/status matrix |
| 42 | SELF COMMAND CAUSALITY | Same-owner `cmax<cmin` is causally impossible | §§10.4, 10.6 | Same-owner causality matrix |
| 43 | SELF COMMAND CAUSALITY | Persisted impossible command metadata yields `CORRUPT_HEAP` | §10.4 | Persisted future/causal fixtures |
| 44 | SELF COMMAND CAUSALITY | Runtime-only impossible command metadata uses internal-invariant path | §§10.4, 39.1.3 | Abstract prepublication instrumentation |
| 45 | SELF COMMAND CAUSALITY | Valid equal command identities remain permitted | §§10.2–10.4 | Current INSERT/DELETE/UPDATE fixtures |
| 46 | CREATOR / XMIN | Creator result composes before deleter result | §10.4 | Cartesian normal composition |
| 47 | UPDATE OLD/NEW PAIR | Current-command INSERT is invisible to ordinary rescan | §10.2 | Statement-effects fixture |
| 48 | UPDATE OLD/NEW PAIR | Later-command INSERT is visible | §10.2 | Statement-effects fixture |
| 49 | UPDATE OLD/NEW PAIR | Current-command DELETE leaves old tuple visible | §10.3.2 | Statement-effects fixture |
| 50 | UPDATE OLD/NEW PAIR | Later-command DELETE hides old tuple | §10.3.2 | Statement-effects fixture |
| 51 | UPDATE OLD/NEW PAIR | SELF current UPDATE selects old, not new | §§10.2–10.3 | UPDATE matrix |
| 52 | UPDATE OLD/NEW PAIR | SELF later UPDATE selects new, not old | §§10.2–10.3 | UPDATE matrix |
| 53 | UPDATE OLD/NEW PAIR | IN_PROGRESS updater selects old, not new | §§10.2–10.3 | UPDATE matrix |
| 54 | UPDATE OLD/NEW PAIR | COMMITTED-before updater selects new, not old | §§10.2–10.3 | UPDATE matrix |
| 55 | UPDATE OLD/NEW PAIR | COMMITTED at/after horizon selects old, not new | §§10.2–10.3 | UPDATE matrix |
| 56 | UPDATE OLD/NEW PAIR | Active-at-capture updater remains old after later commit | §§9.7.2, 10.2–10.3 | Barrier plus UPDATE matrix |
| 57 | UPDATE OLD/NEW PAIR | ABORTED updater selects old, not new | §§10.1–10.3 | UPDATE matrix |
| 58 | UPDATE OLD/NEW PAIR | Every valid UPDATE row emits exactly one ordinary version | §§10.1–10.4 | Pair-output cardinality assertion |
| 59 | READ COMMITTED | One statement attempt uses one stable snapshot | §§9.9, 10.4 | Chapter-9 RC fixture plus scan assertion |
| 60 | READ COMMITTED | Commit after capture does not appear during the attempt | §§9.9, 10.2–10.3 | Mid-attempt terminal barrier |
| 61 | READ COMMITTED | Next statement captures fresh state | §9.9 | Isolation Tests cross-reference |
| 62 | READ COMMITTED | Own earlier/current command effects follow SELF rules | §§9.6, 10.2–10.3 | SELF and statement-effects matrices |
| 63 | RETRY | Pre-write retry refreshes snapshot and retains CommandId | §§9.9, 15.7.1 | Existing retry fixture plus no-future-error assertion |
| 64 | RETRY | Post-write retry cannot continue in same TxnId | §§15.7.2, 39.1 | Statement Failure tests |
| 65 | REPEATABLE READ | First ordinary statement captures RR snapshot | §9.10 | Chapter-9 RR fixture |
| 66 | REPEATABLE READ | Later statements retain external snapshot fields | §9.10 | RR multi-statement fixture |
| 67 | REPEATABLE READ | External commits after capture remain invisible | §§9.10, 10.2–10.3 | Isolation Tests barrier |
| 68 | REPEATABLE READ | Own later writes become visible through updated command boundary | §§9.10, 10.2–10.3 | RR plus SELF matrix |
| 69 | REPEATABLE READ | RR is snapshot isolation, not SERIALIZABLE | §§9.5, 10.2–10.4 | Isolation identity/write-skew tests |
| 70 | SCAN / SNAPSHOT CONSISTENCY | SeqScan validates then evaluates every NORMAL candidate | §§5.17, 10.4 | Heap scan fixture |
| 71 | ERROR PROPAGATION | Heap-scan visibility error terminates rather than skips | §10.4 | Heap error-propagation fixture |
| 72 | INDEX HEAP RECHECK | B+ entry is only physical candidate RID | §8.22.2 | Scan Tests plus index matrix |
| 73 | INDEX HEAP RECHECK | Visible candidate is emitted | §§8.22.2, 10.4 | Index matrix visible row |
| 74 | INDEX HEAP RECHECK | Invisible candidate alone is silently skipped | §§8.22.2, 10.4 | Index matrix invisible row |
| 75 | INDEX HEAP RECHECK | Candidate visibility error propagates and stops success | §§8.22.2, 10.4 | Index error rows |
| 76 | CROSS-OWNER | Visibility does not follow `prev` after INVISIBLE or ERROR | §§5.7.4, 10.4, 14.10 | No-predecessor-fallback fixture |
| 77 | SCAN / SNAPSHOT CONSISTENCY | All candidates/operators in one attempt share snapshot/command | §§9.9–9.10, 10.4 | Scan/Pipeline/Subquery cross-reference |
| 78 | RECOVERY | Aborted INSERT bytes may remain but version is invisible | §§10.1, 13.15 | Retained-bytes recovery fixture |
| 79 | RECOVERY | Aborted DELETE header may remain but delete is ineffective | §§10.1, 13.15 | Retained-header recovery fixture |
| 80 | RECOVERY | Aborted UPDATE selects old and ignores new | §§10.1, 13.15 | Post-READY UPDATE matrix |
| 81 | RECOVERY | Crash loser creator/deleter resolves through ABORTED authority | §§13.15–13.19 | Recovery Property Tests specialization |
| 82 | RECOVERY | Durable COMMIT remains visible without heap/status-page force | §§7.11, 10.1, 13.13 | Durable-WAL/unflushed-pages fixture |
| 83 | RECOVERY | Precrash runtime active/cache state is not trusted after reopen | §§9.13, 13.19 | Reopen lookup-source instrumentation |
| 84 | STATUS / RECLAMATION BOUNDARY | Aborted-`xmax` normalization preserves visibility | §§10.5, 14.13.1 | Vacuum normalization cross-reference |
| 85 | STATUS / RECLAMATION BOUNDARY | Creator freezing preserves visibility and removes lookup need | §§10.5, 14.13.2 | Freeze fixture |
| 86 | STATUS / RECLAMATION BOUNDARY | Hint cleanup mutation follows WAL/page-LSN rules | §§10.5, 12.10–12.12 | Vacuum crash/fault cross-reference |
| 87 | STATUS / RECLAMATION BOUNDARY | RETIRED requires durable Chapter-14 proof | §§10.4, 14.14.1–14.14.3 | Positive retirement procedure |
| 88 | STATUS / RECLAMATION BOUNDARY | Dependent metadata surviving RETIRED is negative corruption case | §§10.4, 14.14.3 | RETIRED visibility fixture |
| 89 | STATUS / RECLAMATION BOUNDARY | Missing status page alone never means RETIRED | §§9.13, 14.14.3 | Chapter-9 lookup plus retirement fixture |
| 90 | STATUS / RECLAMATION BOUNDARY | Snapshot invisibility is not global reclaimability/DEAD | §§5.4.3, 10.5, 14.3–14.12 | Vacuum horizon and slot-state tests |
| 91 | CROSS-OWNER | Ordinary visibility acquires no tuple-write lock | §§10.4, 10.6 | Lock instrumentation on read fixtures |
| 92 | CROSS-OWNER | Chapter 11 remains write-conflict/UNIQUE owner | §§10.3.3, 11.10.4 | Locking/UNIQUE cross-reference |
| 93 | CROSS-OWNER | HeapPage supplies metadata but does not decide visibility | §§5.17, 10.4, 10.6 | Direct tuple versus visibility component fixture |
| 94 | CROSS-OWNER | Chapter 9 status/snapshot is the sole visibility input authority | §§9.7–9.13, 10.4 | Controlled input and lookup-call assertions |
| 95 | CROSS-OWNER | Read-only transactions cannot create persistent status-dependent tuple metadata | §§9.15, 10.4 | Read-only Chapter-9 specialization cross-reference |
| 96 | FAILURE TAXONOMY | §39.1 owns continuation and no new visibility error enum is invented | §§10.4, 39.1 | Error matrix plus statement-failure cross-reference |

---

## Isolation Tests

READ COMMITTED:

- second statement sees newly committed rows,
- one statement does not change snapshot mid-scan,
- writer waiting on updated row restarts/re-evaluates correctly.

Statement-error retry, CommandId consumption, and first-published-write restrictions are
verified by the Statement Failure and Transaction-State Tests above.

REPEATABLE READ / snapshot isolation:

- repeated reads use same snapshot,
- committed concurrent insert remains invisible,
- concurrent update of target causes serialization failure,
- write skew is demonstrably possible and documented as non-serializable.

---

## Locking and Gate Tests

Preserve the basic logical-lock cases:

```text
tuple writer blocks tuple writer
different tuple writers proceed
same unique key serializes
different unique keys proceed
logical lock waits hold no page latches
deadlock cycle detection
highest normal TxnId per cyclic strongly connected component
lock release on commit
lock release on abort
cancelled waiter cleanup
```

Use deterministic barriers, scheduler gates, or semantic event hooks rather than timing
sleeps. Random stress may supplement these fixtures but cannot establish their required
ordering.

### Deterministic logical-lock harness and independent oracles

The harness records and can pause the following implementation-independent events:

```text
request created / exact LockKey resolved
queue and current owners observed
holder installed / waiter enqueued
blocking owners and queue predecessors captured
wait-for node/edges installed / synchronous deadlock scan completed
requester begins waiting / requester becomes ineligible / request canceled
holder terminal state published / resource released
waiter selected / requester eligibility rechecked / grant installed / wake delivered
target, key range, descriptor, and transaction status revalidated
queue entry and wait-for edges removed
```

The fixture controls every event with barriers or explicit scheduler gates and forces both
legal orders at cancellation, terminal publication, release, and grant boundaries. A test
must fail if its required event is observable only by an elapsed-time timeout. Timeout
events may be observed diagnostically, but they are never the deadlock or queue-order
oracle.

Test-side models independently compute:

- exact `LockKey` equality, including complete tuple and encoded unique-key identities;
- exclusive compatibility and the §11.13.3 retained-gate matrix;
- FIFO order for each §11.12 LockManager queue and any represented gate predecessor;
- same-owner reentrant, idempotent, subsumed, and rejected-transition cases;
- requester eligibility and the cancellation linearization;
- all blocking owner/predecessor edges and cyclic strongly connected components;
- the highest normal TxnId victim in each cyclic component;
- legal transaction state, terminal release point, and required post-grant revalidation;
- tuple current-owner outcomes and UNIQUE current-state candidate outcomes.

Production grant, conflict, SCC, and uniqueness decisions are observations, not their own
oracles. Every fixture is valid except for its named mutation: physical page/RID/key and
snapshot inputs remain canonical in a status test; queue and transaction states remain
canonical in an ordering test; and queue/graph state is coherent immediately before an
allocation fault.

### TUPLE_WRITE current-owner and failure verification

Drive the §11.4 current-owner decision after exclusive
`TUPLE_WRITE(TableId,physical RID)` grant and a fresh physical re-fetch. This is a direct
write-admission test, not an inference from Chapter-10 visibility. UPDATE and DELETE run
each applicable row, and no row may mutate the pre-wait target until identity, header,
visibility, `xmax/cmax`, and required status have been revalidated.

| Owner relation | Status/command fixture | Isolation/boundary | Wait? | Retry? | MUST_ABORT? | Expected error/result | May mutate original target? | Required revalidation |
|---|---|---|---:|---:|---:|---|---:|---|
| No competing owner | `xmax == INVALID_TXN_ID` sentinel; no status lookup | RC and RR | no | no | no | proceed if other checks pass | yes | full post-grant target check |
| SELF, earlier command | `cmax < current CommandId` | RC and RR | no | target reacquisition as owned by DML | no | stale/not-current target | no | current row/command semantics |
| SELF, same command | `cmax == current CommandId` | RC and RR | no | no independent writer retry | no | same-command result | only if same-operation rules authorize | exact operation/target context |
| SELF, future command | `cmax > current CommandId` | persisted or runtime-only | no | no | no | persisted `CORRUPT_HEAP`; otherwise internal-invariant result | no | fail before conflict classification |
| SELF, impossible order | `xmin == xmax == SELF`, `cmax < cmin` | persisted or runtime-only | no | no | no | corruption/internal-invariant result | no | fail before conflict classification |
| Other owner | `IN_PROGRESS` | RC and RR | yes | after wake, by owning isolation rule | only if resulting rule requires | wait, then classify fresh state | no | release physical protection; full re-fetch |
| Other owner | COMMITTED | RC, before first statement write | no after wake | fresh RC snapshot, same CommandId | no if retry remains clean | canonical pre-write retry/failed-active path | no | rediscover target and predicates |
| Other owner | COMMITTED | RC, after first statement write | no after wake | no | yes | failed transaction / original write conflict | no | preserve causal result through abort |
| Other owner | COMMITTED after retained snapshot | RR | no after wake | no | yes | serialization/write-conflict result | no | fixed snapshot remains unchanged |
| Other owner | ABORTED | RC and RR | no | no | no | aborted `xmax` is ineffective | yes, after validation | overwrite `xmax/cmax` only through write path |
| Other owner | RETIRED while outcome is required | RC and RR | no | no | no | corruption/invariant result | no | prove retirement contract violation |
| Other normal TxnId | persisted `INVALID` status | RC and RR | no | no | no | canonical error | no | distinguish status from no-owner sentinel |
| Other normal TxnId | recognized `RESERVED` status | RC and RR | no | no | no | canonical error | no | decoder succeeds; semantic use fails |
| Other owner lookup | injected I/O failure | RC and RR | no | no | per exact §39 row | exact lower-layer I/O result | no | no status guess |
| Other owner lookup | checksum, owner, or supported-v1 framing corruption | RC and RR | no | no | per exact §39 row | exact corruption/lower-layer result | no | no conflict conversion |
| Other owner lookup | recognizable unsupported format | RC and RR | no | no | per exact §39 row | exact unsupported-format result | no | no corruption/no-conflict guess |

The persisted `INVALID` row deliberately uses a normal competing TxnId. It is distinct from
the valid `INVALID_TXN_ID` tuple sentinel, which performs no status lookup. RETIRED is
constructed only through a valid Chapter-14 retirement fixture whose proof is then made
inconsistent with the still-live tuple reference. The future-SELF and `cmax < cmin` rows
cross-check Chapter 10, but the assertion here is that write-conflict code cannot convert
those failures into an ordinary conflict or admission.

For lookup failures, inject one exact status-layer outcome at a time through the canonical
transaction-status fixture: raw I/O, checksum/framing/owner corruption, and recognizable
future format. Assert exact error propagation, zero target mutation, no wait, and no
fallback to ABORTED or no owner. This prevents unavailable or malformed ownership metadata
from becoming write permission.

### Lock-table, waiter, and wait-for-graph resource exhaustion

Provide fault points immediately before allocation or reservation of a new lock entry,
waiter record, graph node when separately allocated, and graph edge. Capture queue and graph
snapshots before and after the fault. A blocking request may begin sleep only after every
required dependency is represented; partial request, queue, or graph state must be removed
atomically on failure.

| Resource | Fault point | First statement write? | Permitted partial queue/graph state | Statement / transaction result | Cleanup and ownership oracle |
|---|---|---:|---|---|---|
| Lock entry | before exact-key entry becomes reachable | no | none | `OutOfMemory`/owning allocation error; `FAILED_TRANSACTION_REMAINS_ACTIVE` | no entry or holder; prior locks unchanged |
| Lock entry | same | yes | none | same causal error; `FAILED_TRANSACTION_MUST_ABORT` | abort retains prior ownership through A2/A3 |
| Waiter | before queue publication | no | none | allocation error; `FAILED_TRANSACTION_REMAINS_ACTIVE` | no waiter, edge, or later grant; holder unchanged |
| Waiter | same | yes | none | allocation error; `FAILED_TRANSACTION_MUST_ABORT` | no phantom waiter; prior ownership terminally released |
| Graph node | transaction registration, or lazy materialization before request publication | no | no sleeping request | owning allocation error under §39.1 | transaction/request registration is absent or coherently canceled |
| Graph node | same reachable post-write path, if representation allocates lazily | yes | no sleeping request | owning allocation error; `FAILED_TRANSACTION_MUST_ABORT` | abort from coherent pre-fault graph state |
| Graph edge | before one required blocker edge becomes visible | no | entire failed edge set rolled back | allocation error; `FAILED_TRANSACTION_REMAINS_ACTIVE` | request canceled; no sleep on hidden dependency |
| Graph edge | same | yes | entire failed edge set rolled back | allocation error; `FAILED_TRANSACTION_MUST_ABORT` | request canceled; abort retains owned resources to A3 |

If transaction registration preallocates graph nodes, the graph-node rows inject at that
registration owner and prove that no transaction capable of requesting a gate is published
without its node capacity. They do not require a second lazy allocation path. If graph
coherence cannot be established or restored after any fault, assert the existing internal-
invariant/`DATABASE_NONCONTINUABLE` path instead of ordinary allocation failure. This is
separate from an incompatible lock wait and from deadlock detection.

### FIFO, same-owner, cancellation, and terminal lifetime

For one §11.12 exclusive LockManager key, install holder H and enqueue W1, W2, and W3 in
barrier-controlled order. H's terminal release makes W1 the first eligible owner; W1's and
W2's terminal releases then admit W2 and W3. Repeat for `TUPLE_WRITE` and `UNIQUE_KEY`.
Compatible gate requests need not be delayed merely to mimic this exclusive queue; if an
implementation elects a queue predecessor that prevents a gate grant, verify that the
predecessor is represented in the graph.

| Scenario | Queue before event | Controlled event | Expected next owner/result | Graph effect | Late grant? | Revalidation? |
|---|---|---|---|---|---:|---:|
| Ordinary FIFO | H; W1, W2, W3 | H releases after terminal publication | W1, then W2, then W3 after their releases | current blocker/predecessor edges rebuilt | no bypass | yes |
| Canceled head | H; W1, W2 | W1 cancellation linearizes before H release | W2 after H release | all W1 edges removed | W1: no | W2: yes |
| Deadlock-victim head | H; W1, W2 | W1 receives `DEADLOCK_DETECTED` and enters `MUST_ABORT` | W2 only after actual holder state permits | W1 outgoing wait removed; owned-resource edges persist until release | W1: no | W2: yes |
| Same-owner tuple reacquire | T owns TUPLE_WRITE K | T requests identical K | immediate idempotent/reentrant success | no self-edge | not applicable | no new wait conclusion |
| Same-owner unique reacquire | T owns UNIQUE_KEY K | T requests identical K | immediate idempotent/reentrant success | no self-edge | not applicable | no new wait conclusion |
| MUST_ABORT waiter | H; W | W enters MUST_ABORT before H release | request canceled; ordinary statement cannot resume | waiter edges removed | no | not applicable |
| ABORTING/ABORTED waiter | H; W | state transition precedes grant | request canceled/ineligible | waiter edges removed | no | not applicable |
| COMMITTING/COMMITTED waiter | H; W | state transition precedes ordinary grant | request canceled/ineligible | waiter edges removed | no | not applicable |

Same-owner reacquisition must not create an independently releasable acquisition or shorten
the original terminal lifetime; tests assert ownership remains until C5 or A3 without
requiring a reference-count implementation. Exercise every same-owner `ALLOW`/subsumption
case in §11.13.3 and the two proactive rejections in §11.13.2, including same-table
`SHARED_WRITER -> EXCLUSIVE_DDL`; a rejected transition creates no wait or self-edge.

At the cancel-versus-release boundary, force both orders: cancellation completes before
release, and release selects the request immediately before cancellation tries to
linearize. Exactly one legal result may win. Once transaction ineligibility or cancellation
has linearized, no later grant or wake may resume an ordinary statement. Observe requester
eligibility immediately before installing a grant, not only when enqueuing it.

### Deadlock SCC, terminal release, and wake-up revalidation

In addition to the cross-family schedules below, construct one SCC containing overlapping
simple cycles and a second graph containing two disjoint cyclic SCCs. The independent graph
oracle computes SCCs from the complete blocker/predecessor edge set. Exactly the highest
normal TxnId in each cyclic SCC is selected; overlapping simple cycles within one SCC do
not authorize separate cycle-local choices.

Pause a selected victim after `MUST_ABORT` and before A2. Survivors must remain blocked on
victim-owned resources through A2 ABORTED publication and may be selected/granted only by
A3 release. The victim's outgoing wait is canceled immediately, but its held-resource
ownership remains represented until release. An ordinary deadlock yields
`DEADLOCK_DETECTED`, not timeout, resource exhaustion, or database noncontinuability.

For a woken `TUPLE_WRITE` waiter, mutate the physical target state while it sleeps, then
assert fresh RID/header/visibility/current-owner validation before any write. For a woken
`UNIQUE_KEY` waiter, invalidate every cached range position and candidate result and require
a complete `(K,MIN_RID)` rescan under fresh RID protection. Gate waiters perform the exact
descriptor/manifest/object revalidation in §11.13.4. A grant is never proof that pre-wait
physical conclusions remain valid.

### Runtime lock-state recovery and admission

Extend Recovery Property Tests with a deterministic pre-crash state containing a
`TUPLE_WRITE` holder, a `UNIQUE_KEY` holder, waiters, wait-for edges, and a cross-resource
dependency. Crash at the controlled point, reopen, and observe LockManager/registered-gate
state before ordinary admission.

| Pre-crash runtime state | Durable terminal state | Runtime lock state after reopen | SQL lock replay? | UNIQUE predicate replay? | READY/admission | Expected transaction outcome |
|---|---|---|---:|---:|---|---|
| Active holder, no terminal record | none | no holder, waiter, queue, or edge | no | no | only after loser resolution and READY | canonical recovered ABORTED loser |
| Durable committed holder | COMMIT record durable | empty | no | no | after committed state/data reconstruction and READY | COMMITTED |
| Durable aborted holder | ABORT record durable | empty | no | no | after aborted state reconstruction and READY | ABORTED |
| Holder plus waiter | holder terminal or nonterminal as fixture selects | empty; waiter does not survive | no | no | no pre-READY transaction request | outcomes from WAL/status, never queue |
| Cyclic graph | any valid WAL prefix | no graph nodes/edges reconstructed from old waits | no | no | after recovery reaches READY | per durable terminal/loser analysis |
| Unique-key holder plus waiter | holder terminal or nonterminal as fixture selects | no key holder/waiter | no | no | fresh post-READY requests only | current state checked by new SQL work |

Instrument redo dispatch and assert it never requests `TUPLE_WRITE`, `UNIQUE_KEY`,
`SchemaLock`, `TableWriterGate`, `STATS_PUBLISH`, `MANIFEST_CHANGE`, an SQL wait, or the
§11.10 SQL UNIQUE predicate merely because historical DML/DDL held or checked them.
Recovery may use its separately owned physical page/structure coordination. No ordinary
transaction or logical-lock request is admitted until READY; after READY, fresh
transactions build fresh process-local coordination state. This proves that lock queues
and graph edges are runtime coordination rather than durable transaction truth.

In addition, exercise every `ALLOW`, `WAIT + DEADLOCK GRAPH`, and `REJECT BEFORE WAIT`
combination in `ARCHITECTURE.md` §11.13.3 and every wait-edge/recheck row in §11.13.4 for
the complete transaction-level resource registry in §11.13.1:

```text
SchemaLock
TableWriterGate SHARED_WRITER
TableWriterGate EXCLUSIVE_DDL
STATS_PUBLISH object-publication mode
MANIFEST_CHANGE object-publication mode
TUPLE_WRITE
UNIQUE_KEY
```

For each blocking combination, install an edge to every owner or queue predecessor that
prevents grant, then assert that all resource families participate in one graph. Include
cycles spanning at least three different resource families. Victim selection must be the
deterministic highest TxnId in the cyclic component rather than a timeout or scheduling
artifact.

Directly execute every adversarial timeline in §11.13.7 without copying that list here.
For each timeline:

1. use barriers to establish the exact ownership/wait sequence;
2. inspect the unified graph before victim selection;
3. assert the required victim and automatic ABORT path;
4. retain all protective ownership until ABORTED terminal publication and canonical A3
   cleanup;
5. wake the survivor and require resource-specific descriptor, target, key, visibility, or
   manifest revalidation before grant/continuation.

Repeat the shared-writer manifest case with DDL already terminal before the writer
requests its gate, so acquisition is uncontended. Retain the pre-DDL descriptor
(and the REPEATABLE READ snapshot in that variant); the same current committed
plus transaction-local physical-maintenance set must be selected before writes.
The test-owned DDL ledger determines required index entries/UNIQUE checks; absence
of a wait event cannot authorize the cached old manifest or change SQL visibility.

Cancelled/disconnected waiters must remove all graph edges and queue state without leaking
gates. No transaction-level wait may retain a heap/B+ page latch, BufferPool frame
transition, or read-epoch guard. Proactive same-owner gate-transition rejection must happen
before waiting and must not create a self-edge or be reported as a deadlock.

---

## UNIQUE and PRIMARY KEY Enforcement Tests

Exercise every row and aggregate-outcome rule in `ARCHITECTURE.md` §11.10 through a
table-driven candidate harness. Architecture owns the exact current-owner truth tables;
tests construct the candidate heap/index/status/CommandId state, run the ordinary
UNIQUE/PRIMARY KEY operation, and assert the specified outcome and transaction consequence.

Run the matrix over:

```text
ordinary UNIQUE keys containing NULL
PRIMARY KEY NULL rejection
composite keys
FLOAT64 canonical zero/NaN and ordering/equality cases
committed and frozen creators
aborted and nonterminal creators
absent, committed, aborted, and nonterminal deleters
same-transaction earlier-command creators
same-transaction current-command creators
earlier-command self-delete reuse
current-command self-delete/supersession of another row
```

The harness must probe the complete physical `(user_key,RID)` range and validate every
candidate's protected heap identity and re-encoded key. Include stale terminal entries,
aborted versions, exact duplicate physical keys, dangling/mismatched RIDs, and protected RID
reuse attempts. Corruption candidates must not be hidden by early exit after finding an
ordinary conflict.

### Current-state status, failure, and race fixtures

The normal-regression table includes no candidate, a committed/frozen live creator, a
committed creator invisible to the checking transaction's old RR snapshot, an active
creator, an aborted creator, an aborted deleter, a committed effective deleter, and an
active deleter. It also includes exact `SELF_EXCLUDED` old/replacement RIDs, another live
row from the same transaction, current-command and earlier-command self deletes,
NULL-containing ordinary UNIQUE keys, and PRIMARY KEY NULL rejection. The independent
oracle enumerates the complete exact-key range and applies §11.10.4 aggregate precedence;
ordinary MVCC visibility is recorded only to prove it is not the admission oracle.

Run each nonterminal creator/deleter case to both terminal outcomes. The checker first
observes `WAIT_THEN_RECHECK`, publishes nothing, and waits without physical latches. After
COMMITTED or ABORTED publication and release, it restarts the whole range check. In
particular, a committed live creator is `UNIQUE_CONFLICT` even when invisible to the
caller's retained RR snapshot, while an aborted creator is ignorable only after canonical
status recheck.

The following negative matrix uses a structurally valid index candidate, matching protected
heap RID/relation/key, valid snapshot/operation context, and exactly one altered status or
identity dimension:

| Candidate status/result | Decoder valid? | Logical ownership resolvable? | Predicate/result | May proceed? | Exact error family/oracle |
|---|---:|---:|---|---:|---|
| RETIRED while candidate still requires outcome | yes | no | error, never candidate success | no | corruption/internal-invariant result; retirement proof contradicted |
| Persisted `INVALID` for allocated normal owner | yes | no | error | no | canonical invalid-status/current-owner error |
| Persisted `RESERVED` | yes | no | error | no | recognized encoding, semantically rejected state |
| Status I/O failure | not reached/available | no | propagated failure | no | exact lower-layer I/O result |
| Supported-v1 checksum/framing/owner corruption | no trusted status | no | propagated failure | no | exact corruption/lower-layer result |
| Recognizable unsupported status format | format dispatch rejects | no | propagated failure | no | exact unsupported-format result |
| Dangling, reused, wrong-relation, non-NORMAL, or key-mismatch RID | status is irrelevant after identity failure | no | `CORRUPTION_OR_INTERNAL_ERROR` conceptual branch | no | exact owning heap/index corruption result |

For `RESERVED`, independently observe successful two-bit decoding before semantic rejection.
For unsupported format, observe version dispatch before any status interpretation. For I/O
and corruption, assert the exact lower-layer diagnostic survives §39 classification. None
of these rows may become `NO_CONFLICT`, `WAIT_THEN_RECHECK`, or `UniqueViolation`; a
constraint violation requires a valid current logical owner.

Use two transactions for the same fully non-NULL key to cover INSERT/INSERT and
UPDATE/INSERT in both lock-acquisition orders. If the first holder commits, the waiter
rescans and reports `UniqueViolation`; if it aborts, the waiter may proceed only after a
complete no-other-owner rescan. For DELETE/INSERT, the inserter waits under the key lock: a
committed delete may free the key after recheck, while an aborted delete leaves the original
owner conflicting. The holder's key lock remains present through C4 or A2 and is released
only at C5 or A3; the waiter cannot begin its deciding rescan earlier.

### Same-statement and UPDATE cases

Test duplicate values within one multirow INSERT for both sequential and batch/pending-set
checking. For UPDATE, cover:

- retaining the same logical key while excluding only the exact revalidated old RID;
- another row in the same transaction/current command already owning the target key;
- target-order permutations producing the same result;
- immediate conflicting key swaps;
- non-NULL-to-NULL, NULL-to-non-NULL, and composite partial-NULL transitions.

The test oracle must use the §11.10 logical current-owner decision, not mere physical index
presence or ordinary statement-snapshot visibility.

### Wait, recheck, and transaction outcome

Hold `UNIQUE_KEY(IndexId,K)` in one transaction and block another without physical latches.
Complete the owner once as COMMITTED and once as ABORTED. After wakeup, assert that the
waiter discards every prior range position, status result, RID dereference, and read-epoch
guard; restarts from `(K,MIN_RID)` under a fresh guard; and performs a full current-state
recheck before deciding.

Exercise stale physical entries and exact protected RID identity during this recheck. A
wakeup must not assume either that the conflict persists or that it disappeared.

For every violation, apply `ARCHITECTURE.md` §39.1.3 separately before and after the
statement's first published write. Assert the statement/client result and transaction state
without defining a second UNIQUE-specific error policy. All `UNIQUE_KEY`, tuple, writer,
schema, and publication ownership remains held until COMMITTED or ABORTED runtime terminal
publication and releases only in C5 or A3.

---

## Chapter 11 Procedural Matrices and Coverage

The matrices below consolidate procedure ownership; they do not replace the architecture's
normative decisions.

### Error and result matrix

| Observed condition | Expected result/state | Retry? | Ownership/release | Procedure owner |
|---|---|---:|---|---|
| Incompatible owner or represented predecessor | WAIT; transaction remains eligible | after grant only | existing ownership retained | Locking deterministic harness |
| Clean RC stale-target conflict before first statement write | architecture-permitted same-CommandId fresh-snapshot retry or failed-active result | yes, only at §15.7 boundary | retained transaction locks remain terminal-duration | TUPLE current-owner matrix / Statement Failure tests |
| RC stale-target conflict after first statement write | `FAILED_TRANSACTION_MUST_ABORT` | no | release through A2/A3 | TUPLE current-owner matrix / §39 tests |
| RR post-snapshot committed writer | transaction-fatal serialization/write conflict | no | release through A2/A3 | TUPLE current-owner matrix / Isolation Tests |
| Deadlock victim | `DEADLOCK_DETECTED`, then `MUST_ABORT` | no ordinary same-TxnId statement retry | held resources remain until A2/A3 | Deadlock SCC procedure |
| Valid current logical UNIQUE owner | `UniqueViolation`; §39 determines failed-active versus must-abort | no internal post-write retry | key lock remains terminal-duration | UNIQUE current-state procedures |
| Impossible persisted owner/RID/command state | exact corruption result | no | §39/lower-layer terminal handling | TUPLE/UNIQUE negative matrices |
| Impossible runtime-only invariant | internal-invariant result | no | ordinarily `DATABASE_NONCONTINUABLE` | TUPLE/UNIQUE negative matrices |
| Status/index/heap I/O failure | exact lower-layer result | only if owning layer explicitly permits | never guessed free/nonconflicting | Status-failure fixtures |
| Recognizable future format | exact unsupported-format result | no semantic retry invented here | no write/constraint admission | Status-failure fixtures |
| Coherent runtime allocation failure | `OutOfMemory` or exact owning resource error; FA before / MA after first write | per §39 only | partial request/graph removed; prior ownership preserved | Resource-exhaustion matrix |
| Uncertain queue/graph/ownership coherence | internal-invariant / `DATABASE_NONCONTINUABLE` | no | controlled teardown/recovery; no guessed release | Resource-exhaustion matrix |
| Waiter becomes transaction-ineligible | lock cancellation with original stronger cause preserved | no ordinary continuation | outgoing wait state removed; held resources terminally released | Queue/lifetime matrix |

### High-level domain and case matrix

| Domain/case | Deterministic fixture | Controlled event/fault | Independent oracle | Architecture | Verification owner |
|---|---|---|---|---|---|
| TUPLE_WRITE normal owner | Canonical target/current-owner rows | grant and re-fetch | owner/status/isolation matrix | §§11.2–11.7 | TUPLE current-owner procedure |
| TUPLE_WRITE invalid owner/status | RETIRED/INVALID/RESERVED/future SELF | one status/command mutation | no-guessing matrix | §§11.4, 11.15 | TUPLE negative rows |
| TUPLE_WRITE status failure | I/O/corruption/unsupported format | status lookup | exact injected result | §§11.4, 39.1 | TUPLE lookup rows |
| RC conflict | Competing committed updater | pre/post first-write boundary | fixed §15.7/§39 state model | §§11.5, 15.7 | TUPLE/Statement Failure tests |
| RR conflict | Retained snapshot plus later commit | holder terminal publication | fixed snapshot oracle | §11.6 | TUPLE/Isolation tests |
| UNIQUE normal owner | Complete exact-key candidate range | candidate terminal states | independent current-state classifier | §§11.8–11.10 | UNIQUE normal regression |
| UNIQUE invalid owner/status | RETIRED/INVALID/RESERVED | one status mutation | negative matrix | §11.10.4 | UNIQUE status fixtures |
| UNIQUE lookup failure | I/O/corruption/unsupported format | candidate status lookup | exact injected result | §§11.10.4, 39.1 | UNIQUE status fixtures |
| Lock-entry exhaustion | New exact key | pre-publication allocation | pre/post state snapshots | §§11.12, 39.1 | Resource-exhaustion matrix |
| Waiter exhaustion | Contended exact key | enqueue allocation | queue/graph model | §§11.12–11.13, 39.1 | Resource-exhaustion matrix |
| Graph-edge exhaustion | Blocking request | dependency installation | complete-edge model | §11.13.4 | Resource-exhaustion matrix |
| FIFO | H/W1/W2/W3 | terminal releases | independent ordered queue | §11.12 | Queue/lifetime matrix |
| Same-owner reacquire | Tuple/key current owner | identical request | owner identity model | §§11.12–11.13 | Queue/lifetime matrix |
| Cancellation/no-late-grant | Queued eligible waiter | state/cancel versus release | eligibility linearization | §§11.12–11.13.5 | Queue/lifetime matrix |
| Deadlock SCC | Overlapping and disjoint cycles | final edge addition | independent SCC/victim model | §11.13.4 | Deadlock SCC procedure |
| Terminal release | Waiting competitor | C4/C5 and A2/A3 gates | event-order model | §§11.11, 15.5–15.6 | COMMIT/ABORT and locking tests |
| Wake-up revalidation | Target/key/descriptor changes while waiting | release/grant | fresh-state oracle | §§11.3, 11.9, 11.13.4 | Wake-up procedures |
| Recovery reset | Holders/waiters/edges at crash | reopen/recovery/READY | durable WAL/status model | §§11.12–11.13, 13.11–13.19 | Runtime lock-state recovery |
| No SQL lock replay | DML WAL requiring redo | redo dispatch | event log excludes SQL locks | §§11.10.8, 13.13 | Runtime lock-state recovery |
| No UNIQUE replay | Authorized historical unique DML | redo dispatch | event log excludes SQL predicate | §§11.10.8, 13.13 | Runtime lock-state recovery |

### Atomic architecture-obligation coverage map

A deterministic procedure above or a precise existing owner must supply the
fixture, controlled event/fault, independent oracle, and expected observation. Architecture
text alone is not counted as coverage.

| ID | Primary domain | Atomic obligation | Architecture owner | Verification procedure/reference |
|---:|---|---|---|---|
| 1 | LOCK IDENTITY | `TUPLE_WRITE` equality uses `(TableId,physical RID)` | §11.2 | Harness key oracle plus tuple-blocking cases |
| 2 | LOCK IDENTITY | Different table/RID tuples do not alias | §11.2 | Exact-key positive/negative vectors |
| 3 | LOCK IDENTITY | `UNIQUE_KEY` equality uses `(IndexId,complete encoded key)` | §11.8 | Harness key oracle plus unique races |
| 4 | LOCK IDENTITY | Hash collision cannot replace complete unique-key equality | §11.8 | Forced-collision key vectors |
| 5 | LOCK IDENTITY | Composite/scalar semantic equality matches encoded lock equality | §11.8 | UNIQUE key-codec matrix |
| 6 | LOCK IDENTITY | NULL-containing ordinary UNIQUE key has no duplicate lock identity | §§11.8, 11.10.2 | UNIQUE normal regression |
| 7 | LOCK MODE | Ordinary readers take no shared tuple lock | §11.1 | Isolation plus lock event log |
| 8 | COMPATIBILITY | TUPLE_WRITE and UNIQUE_KEY are exclusive | §§11.1, 11.12 | Same/different-key blocking cases |
| 9 | COMPATIBILITY | Different logical tuple/unique keys proceed independently | §§11.2, 11.8, 11.12 | Parallel exact-key fixtures |
| 10 | CROSS-OWNER | Registered schema/writer/publication domains use their exact scopes/modes | §11.13.1 | Complete §11.13.3 matrix execution |
| 11 | DEADLOCK GRAPH | Separate resource containers still share one graph | §§11.1, 11.13.1 | Cross-family cycle fixtures |
| 12 | RECOVERY RESET | Lock table/queues are process-local runtime state | §§11.12, 11.13 | Runtime lock-state recovery |
| 13 | ACQUISITION | Unowned exact logical key grants immediately | §11.12 | Harness grant event |
| 14 | ACQUISITION | Another owner of an exclusive key causes represented wait | §§11.12, 11.13.4 | Holder/waiter fixture |
| 15 | REENTRANCY | Same-owner exact key does not become a competing owner | §11.12 | Same-owner tuple/unique rows |
| 16 | QUEUE ORDER | Exclusive LockManager waiters follow FIFO | §11.12 | H/W1/W2/W3 fixture |
| 17 | ACQUISITION | UPDATE/DELETE acquires tuple key before owner mutation | §§11.4, 11.9 | DML revalidation integration |
| 18 | ACQUISITION | Unique locks are requested only after key derivation and latch release | §11.9 | Unique wait event sequence |
| 19 | LOCK ORDER | UPDATE old/new unique keys use total `(IndexId,key)` order when known | §11.9 | Opposite-expression-order fixture |
| 20 | LOCK ORDER | Multiple known unique constraints use the same total order | §11.9 | Multi-index key-set fixture |
| 21 | DEADLOCK GRAPH | Incrementally discovered multirow keys remain protected and graph-visible | §11.9 | Streaming multirow cross-key cycle |
| 22 | LOCK ORDER | DML/DDL/ANALYZE gate order follows operation table | §§11.13.2, 11.13.6 | Every operation-order row |
| 23 | COMPATIBILITY | Every §11.13.3 ALLOW/WAIT/REJECT cell is observed | §11.13.3 | Existing complete gate matrix procedure |
| 24 | CANCELLATION | Ineligible transaction states cannot acquire a new ordinary resource | §§11.13.3, 11.13.6 | Queue/lifetime state rows |
| 25 | LOCK/LATCH ORDER | TUPLE_WRITE wait retains no heap/B+/frame latch | §§11.3, 11.14 | Latch ownership event assertions |
| 26 | LOCK/LATCH ORDER | UNIQUE_KEY wait retains no physical latch/read epoch | §§11.3, 11.9 | Unique wait/recheck fixture |
| 27 | LOCK/LATCH ORDER | Registered gate wait retains no prohibited physical ownership | §§11.13.2, 11.13.7 | Cross-family schedules |
| 28 | LOCK/LATCH ORDER | Page latches never become transaction locks or graph nodes | §11.14 | Instrumented owner-domain assertion |
| 29 | WAKE-UP REVALIDATION | TUPLE_WRITE grant causes fresh target/owner validation | §§11.2–11.4 | TUPLE wake-up fixture |
| 30 | WAKE-UP REVALIDATION | UNIQUE_KEY grant causes complete fresh exact-key rescan | §§11.9–11.10 | UNIQUE wake-up fixture |
| 31 | WAKE-UP REVALIDATION | Gate grant causes resource-specific descriptor/manifest revalidation | §11.13.4 | Existing adversarial timelines |
| 32 | REVALIDATION | A pre-wait RID cannot authorize post-wait mutation | §§11.3, 11.10.8 | Mutated-target fixture plus read-epoch checks |
| 33 | TUPLE CURRENT OWNER | No-xmax sentinel permits write after ordinary checks | §11.4.1 | TUPLE matrix row |
| 34 | TUPLE CURRENT OWNER | SELF earlier-command target is stale/not blindly mutable | §11.4.2 with Ch. 10 | TUPLE matrix row |
| 35 | TUPLE CURRENT OWNER | SELF current-command uses same-command semantics | §11.4.2 with Ch. 10 | TUPLE matrix row |
| 36 | TUPLE CURRENT OWNER | Future SELF command is corruption/invariant failure | §§11.4.2, 11.10.4 | TUPLE negative fixture |
| 37 | TUPLE CURRENT OWNER | Same-owner `cmax < cmin` is not an ordinary conflict | §11.10.4 with §10.4 | TUPLE negative fixture |
| 38 | TUPLE CURRENT OWNER | Other IN_PROGRESS owner is waited upon | §11.4.4 | TUPLE matrix row |
| 39 | LOCK/LATCH ORDER | IN_PROGRESS wait releases physical protection first | §§11.3, 11.4.4 | Event-order assertion |
| 40 | RC WRITE CONFLICT | Committed owner before first write permits canonical RC retry | §11.5 | TUPLE matrix / Statement Failure tests |
| 41 | RC WRITE CONFLICT | Committed owner after first write requires MUST_ABORT | §11.5 | TUPLE matrix / §39 boundary |
| 42 | RR WRITE CONFLICT | Post-snapshot committed owner is transaction-fatal | §11.6 | TUPLE matrix / Isolation tests |
| 43 | TUPLE CURRENT OWNER | ABORTED xmax is ineffective after revalidation | §11.4.3 | TUPLE matrix row |
| 44 | FAILURE PROPAGATION | Required RETIRED owner outcome is corruption/invariant | §§11.10.4, 14.14.3 | TUPLE RETIRED fixture |
| 45 | FAILURE PROPAGATION | Persisted INVALID owner status is an error | §11.10.4 with §9.13 | TUPLE INVALID fixture |
| 46 | TERMINOLOGY | Persisted INVALID status differs from `INVALID_TXN_ID` sentinel | §§11.4.1, 11.10.4 | Explicit paired TUPLE rows |
| 47 | FAILURE PROPAGATION | Recognized RESERVED owner status is semantically rejected | §11.10.4 with §9.12 | TUPLE RESERVED fixture |
| 48 | FAILURE PROPAGATION | Current-owner I/O failure propagates exactly | §§11.4, 39.1 | TUPLE lookup fixture |
| 49 | FAILURE PROPAGATION | Current-owner corruption propagates exactly | §§11.4, 39.1 | TUPLE lookup fixture |
| 50 | FAILURE PROPAGATION | Unsupported status format propagates exactly | §§11.4, 39.1 | TUPLE lookup fixture |
| 51 | WRITE CONFLICT | UPDATE follows lock/re-fetch/current-owner protocol | §11.4 | DML plus TUPLE matrix |
| 52 | WRITE CONFLICT | DELETE follows the same conflict protocol | §11.4 | DML plus TUPLE matrix |
| 53 | CONCURRENCY | Two writers cannot silently overwrite one target version | §11.7 | Deterministic competing-writer fixture |
| 54 | CROSS-OWNER | Write admission is distinct from snapshot visibility | §§11.4–11.7 | TUPLE matrix plus MVCC Visibility tests |
| 55 | RETRY / MUST_ABORT | Pre-write internal RC retry retains one CommandId | §§11.5, 15.7 | CommandId/snapshot retry tests |
| 56 | RETRY / MUST_ABORT | RC retry captures a fresh statement snapshot | §§11.5, 15.7 | Isolation/Statement Failure tests |
| 57 | RETRY / MUST_ABORT | Published-write RC attempt cannot transparently retry | §§11.5, 39.1 | Pre/post boundary fixture |
| 58 | RR WRITE CONFLICT | RR never refreshes its retained snapshot after conflict | §11.6 | Isolation fixture |
| 59 | FAILURE PROPAGATION | Metadata/status errors are not ordinary write conflicts | §§11.4, 11.10.4 | TUPLE negative assertions |
| 60 | RETRY / MUST_ABORT | Statement/transaction result follows exact §39 class | §§11.5–11.6, 39.1 | Error/result matrix |
| 61 | UNIQUE CURRENT STATE | Empty exact-key range is NO_CONFLICT | §11.10.4 | UNIQUE normal regression |
| 62 | UNIQUE CURRENT STATE | Committed/frozen live creator conflicts | §§11.10.4–11.10.6 | UNIQUE normal regression |
| 63 | UNIQUE CURRENT STATE | Snapshot-invisible committed live owner still conflicts | §§11.10.1, 11.10.4 | Old-RR-snapshot fixture |
| 64 | UNIQUE CURRENT STATE | Active creator commit becomes conflict after rescan | §11.10.7 | Active-owner terminal fixture |
| 65 | UNIQUE CURRENT STATE | Active creator abort may free key after rescan | §11.10.7 | Active-owner terminal fixture |
| 66 | UNIQUE CURRENT STATE | Aborted creator is ignorable only after status check | §11.10.4 | UNIQUE normal regression |
| 67 | UNIQUE CURRENT STATE | Active deleter commit may free key after rescan | §§11.10.4, 11.10.7 | Delete/insert race |
| 68 | UNIQUE CURRENT STATE | Active deleter abort preserves conflict after rescan | §§11.10.4, 11.10.7 | Delete/insert race |
| 69 | UNIQUE CURRENT STATE | Aborted deleter is ineffective | §11.10.4 | UNIQUE normal regression |
| 70 | UNIQUE CURRENT STATE | Committed effective deleter removes old ownership | §11.10.4 | UNIQUE normal regression |
| 71 | UNIQUE CURRENT STATE | Exact current UPDATE old RID is SELF_EXCLUDED | §§11.10.3, 11.10.6 | UPDATE exact-RID fixture |
| 72 | UNIQUE CURRENT STATE | Published replacement exclusion is exact/context-local | §§11.10.3–11.10.4 | Continuation-context fixture |
| 73 | UNIQUE CURRENT STATE | Same transaction's other live row conflicts | §§11.10.5–11.10.6 | Multirow/same-Txn fixture |
| 74 | UNIQUE CURRENT STATE | Earlier-command self delete permits later reuse | §§11.10.4–11.10.6 | CommandId fixture |
| 75 | UNIQUE CURRENT STATE | Current-command other-row self delete does not free key | §§11.10.4–11.10.6 | Same-command fixture |
| 76 | UNIQUE CURRENT STATE | NULL-containing ordinary UNIQUE key skips duplicate admission | §11.10.2 | NULL matrix |
| 77 | UNIQUE CURRENT STATE | PRIMARY KEY rejects NULL before unique predicate | §11.10.2 | PRIMARY KEY NULL fixture |
| 78 | LOCK IDENTITY | Unique predicate equality uses canonical complete encoded bytes | §§11.8, 11.10.2 | Codec/equality vectors |
| 79 | UNIQUE CURRENT STATE | Complete physical exact-key range is enumerated | §11.10.4 | Candidate harness event/count oracle |
| 80 | UNIQUE CURRENT STATE | Corruption precedence survives an ordinary conflicting candidate | §11.10.4 | Conflict-plus-corrupt range fixture |
| 81 | FAILURE PROPAGATION | RETIRED required by a unique candidate is an error | §11.10.4 | UNIQUE RETIRED fixture |
| 82 | FAILURE PROPAGATION | INVALID required by a unique candidate is an error | §11.10.4 | UNIQUE INVALID fixture |
| 83 | FAILURE PROPAGATION | RESERVED required by a unique candidate is an error | §11.10.4 | UNIQUE RESERVED fixture |
| 84 | FAILURE PROPAGATION | Unique candidate status I/O failure propagates | §§11.10.4, 39.1 | UNIQUE failure matrix |
| 85 | FAILURE PROPAGATION | Unique candidate status corruption propagates | §§11.10.4, 39.1 | UNIQUE failure matrix |
| 86 | FAILURE PROPAGATION | Unique candidate unsupported format propagates | §§11.10.4, 39.1 | UNIQUE failure matrix |
| 87 | REVALIDATION | Dangling/reused/wrong-key candidate RID is corruption | §§11.10.4, 11.10.8 | UNIQUE identity-failure fixture |
| 88 | UNIQUE RACES | Concurrent same-key INSERTs cannot both commit | §§11.9, 11.10.7 | Both-holder-outcomes race |
| 89 | UNIQUE RACES | Unique UPDATE versus INSERT cannot both own one key | §§11.9, 11.10.6–11.10.7 | Both lock-order/outcome fixtures |
| 90 | UNIQUE RACES | DELETE/INSERT result follows deleter terminal outcome | §§11.9–11.10 | Delete/insert race |
| 91 | TERMINAL RELEASE | UNIQUE_KEY remains held through C4/A2 and releases C5/A3 | §11.11 | Unique release-boundary fixture |
| 92 | UNIQUE CURRENT STATE | Nonunique index maintenance does not invoke logical uniqueness admission | §§11.8–11.10 | Nonunique event-log fixture |
| 93 | QUEUE ORDER | Ordinary exclusive queue admits W1/W2/W3 in FIFO order | §11.12 | Queue/lifetime matrix |
| 94 | CANCELLATION | Canceled head is atomically removed and W2 can advance | §§11.12, 11.13.4 | Queue/lifetime matrix |
| 95 | CANCELLATION | Deadlock-victim head cannot later receive grant | §§11.12, 11.13.4 | Queue/lifetime matrix |
| 96 | REENTRANCY | Same-owner TUPLE_WRITE reacquire succeeds without wait | §11.12 | Queue/lifetime matrix |
| 97 | REENTRANCY | Same-owner UNIQUE_KEY reacquire succeeds without wait | §11.12 | Queue/lifetime matrix |
| 98 | REENTRANCY | Same-owner reacquire creates no self-edge/self-holder conflict | §§11.12–11.13 | Graph event assertion |
| 99 | TERMINAL RELEASE | Reacquire cannot shorten or split terminal ownership | §§11.11–11.12 | Same-owner retained-lifetime fixture |
| 100 | REENTRANCY | Identical/stronger same-owner gate requests are subsumed | §§11.13.2–11.13.3 | Complete gate matrix |
| 101 | ACQUISITION | Unsupported same-owner gate transitions reject before wait | §11.13.2 | Rejection/no-edge fixtures |
| 102 | NO-LATE-GRANT | MUST_ABORT waiter is canceled and cannot resume | §§11.12, 11.13.4 | Queue/lifetime matrix |
| 103 | NO-LATE-GRANT | ABORTING/ABORTED waiter cannot receive ordinary grant | §§11.12–11.13 | Queue/lifetime matrix |
| 104 | NO-LATE-GRANT | COMMITTING/COMMITTED waiter cannot receive ordinary grant | §§11.12–11.13 | Queue/lifetime matrix |
| 105 | CANCELLATION | Cancel-versus-release admits only its linearized legal winner | §§11.12–11.13 | Two-order barrier fixture |
| 106 | CANCELLATION | Canceled/disconnected request removes all outgoing wait state | §11.13.4 | Queue/graph snapshot assertion |
| 107 | DEADLOCK GRAPH | Every registered transaction resource family uses one graph | §11.13.1 | Cross-family cycles |
| 108 | DEADLOCK GRAPH | Waiter has an edge to every incompatible owner | §11.13.4 | Independent blocker-set comparison |
| 109 | DEADLOCK GRAPH | Elected queue predecessors that prevent grant have edges | §11.13.4 | FIFO/fairness predecessor fixture |
| 110 | DEADLOCK GRAPH | Relevant edge addition/replacement triggers synchronous detection | §11.13.4 | Scan-complete-before-sleep event order |
| 111 | DEADLOCK VICTIM | Overlapping simple cycles in one SCC yield one SCC decision | §§11.13.4, 11.15 | Overlapping-cycle graph fixture |
| 112 | DEADLOCK VICTIM | Disjoint cyclic SCCs each yield a victim | §11.13.4 | Two-SCC graph fixture |
| 113 | DEADLOCK VICTIM | Victim is highest normal TxnId in each cyclic SCC | §11.13.4 | Independent SCC/max oracle |
| 114 | DEADLOCK VICTIM | Victim receives DEADLOCK_DETECTED and enters MUST_ABORT | §11.13.4 | Victim state/result assertion |
| 115 | CANCELLATION | Victim's outgoing wait is canceled for abort progress | §11.13.4 | Victim queue/edge snapshot |
| 116 | TERMINAL RELEASE | Victim-held resources remain until A2/A3 | §§11.11, 11.13.4 | Paused-abort survivor fixture |
| 117 | WAKE-UP REVALIDATION | Survivor wakes only on actual release and revalidates | §11.13.4 | Release/grant/recheck event sequence |
| 118 | DEADLOCK VICTIM | Timeout is diagnostic and cannot choose the victim | §11.13.4 | Timeout-versus-SCC oracle fixture |
| 119 | RESOURCE EXHAUSTION | Lock-entry allocation failure before first write is coherent FA | §§11.12, 39.1 | Resource-exhaustion matrix |
| 120 | RESOURCE EXHAUSTION | Lock-entry allocation failure after first write is MA | §§11.12, 39.1 | Resource-exhaustion matrix |
| 121 | RESOURCE EXHAUSTION | Waiter allocation failure before first write leaves no request/edge | §§11.12–11.13, 39.1 | Resource-exhaustion matrix |
| 122 | RESOURCE EXHAUSTION | Waiter allocation failure after first write is coherent MA | §§11.12–11.13, 39.1 | Resource-exhaustion matrix |
| 123 | RESOURCE EXHAUSTION | Graph-node capacity is established before request-capable registration | §§11.13.1, 39.1 | Registration/lazy-node fault fixture |
| 124 | RESOURCE EXHAUSTION | Lazy post-write graph-node failure, if reachable, cannot publish a waiter | §§11.13.1, 39.1 | Representation-appropriate node fixture |
| 125 | RESOURCE EXHAUSTION | Graph-edge allocation failure before first write cancels coherently | §§11.13.4, 39.1 | Resource-exhaustion matrix |
| 126 | RESOURCE EXHAUSTION | Graph-edge allocation failure after first write is coherent MA | §§11.13.4, 39.1 | Resource-exhaustion matrix |
| 127 | DEADLOCK GRAPH | No request sleeps without every represented dependency | §11.13.4 | Sleep-entry graph snapshot |
| 128 | FAILURE PROPAGATION | Uncertain graph/ownership coherence is noncontinuable | §§11.13.4, 39.1 | Graph-coherence failure fixture |
| 129 | TERMINAL RELEASE | Commit release occurs at C5 after C4 publication | §§11.11, 15.5 | COMMIT Fault-Injection plus wait observer |
| 130 | TERMINAL RELEASE | Abort release occurs at A3 after A2 publication | §§11.11, 15.6 | ABORT Fault-Injection plus wait observer |
| 131 | TERMINAL RELEASE | MUST_ABORT retains owned tuple/key/gates until ABORTED | §§11.11, 11.13.4 | Paused-abort fixture |
| 132 | WAKE-UP REVALIDATION | Holder terminal release never substitutes for fresh state checks | §§11.3, 11.13.4 | Tuple/unique/gate wake fixtures |
| 133 | CANCELLATION | Every transaction-ineligible waiter is canceled before ordinary continuation | §§11.12–11.13.5 | State-transition queue matrix |
| 134 | SHUTDOWN / LIFETIME | DRAINING rejects new ordinary statements/gates | §11.13.5 with §3.3.6 | Database Lifecycle Tests plus lock event log |
| 135 | SHUTDOWN / LIFETIME | DATABASE_NONCONTINUABLE admits no new ordinary acquisition | §11.13.5 | Lifecycle/noncontinuable fixture |
| 136 | SHUTDOWN / LIFETIME | Lock/gate manager outlives holders/waiters through controlled drain | §§11.12–11.13.5 | Drain/destruction event ordering |
| 137 | RECOVERY RESET | Reopen creates empty transaction lock/gate wait state | §§11.12–11.13 | Recovery reset matrix |
| 138 | RECOVERY RESET | Pre-crash holders do not survive | §§11.12–11.13 | Recovery reset matrix |
| 139 | RECOVERY RESET | Pre-crash waiters do not survive | §§11.12–11.13 | Recovery reset matrix |
| 140 | RECOVERY RESET | Pre-crash wait-for edges do not survive | §11.13.4 | Recovery reset matrix |
| 141 | RECOVERY RESET | Redo does not replay SQL locks or gate waits | §§11.10.8, 13.13 | Redo-dispatch event assertion |
| 142 | RECOVERY RESET | Redo does not rerun SQL UNIQUE current-state admission | §§11.10.8, 13.13 | Redo-dispatch event assertion |
| 143 | RECOVERY RESET | Ordinary admission begins only after READY | §§11.13.5, 13.19 | READY gate fixture |
| 144 | RECOVERY RESET | Loser outcome reconstruction needs no runtime-lock replay | §§11.11–11.13, 13.15 | Loser recovery row |
| 145 | CROSS-OWNER | Physical B+ candidate presence is not UNIQUE authority | §§11.10.1, 11.10.4 | Candidate/status harness |
| 146 | FAILURE PROPAGATION | Status/identity failure becomes neither free key nor conflict | §§11.4, 11.10.4 | TUPLE/UNIQUE negative matrices |
| 147 | RECOVERY RESET | Exact physical replay is distinct from SQL conflict admission | §§11.10.8, 11.15 | No-UNIQUE-replay fixture |
| 148 | OTHER | Queue/container/mutex choice is not a verification oracle | §11.12 | Semantic event/oracle harness |
| 149 | CROSS-OWNER | Published RETIRING claim rejects/cancels without a transaction edge | §§11.13.4–11.13.5 | Object-state revalidation fixture |
| 150 | CROSS-OWNER | Nontransaction maintenance owners remain outside the transaction graph | §11.13.5 | Owner-domain event assertions |
| 151 | SHUTDOWN / LIFETIME | Deadlock victim selection never authorizes physical unlink | §§11.13.5, 11.13.7 | DROP-victim drain fixture |
| 152 | WAKE-UP REVALIDATION | Writer/publication wake uses current manifest/object state | §11.13.4 | Gate wake adversarial timelines |
| 153 | CROSS-OWNER | Ordinary SELECT uses MVCC/descriptors without transaction gates | §§11.1, 11.13.6 | SELECT lock-event fixture |
| 154 | LOCK MODE | Deferred table/intention/schema/key-range LockManager families are not silently added | §11.1 | Registry enumeration and unsupported-scope checks |
| 155 | FAILURE PROPAGATION | UniqueViolation is emitted only for a valid logical owner | §§11.10.3–11.10.4, 39.1 | UNIQUE error-versus-violation assertions |
| 156 | OTHER | Implementation remains parallel-ready without container-specific expectations | §§11.12, 11.15 | Concurrency semantics across alternate harness scheduling |


---

## Vacuum and Reclamation Tests

This section is the detailed procedural owner for `ARCHITECTURE.md` §§14.1–14.18. It
specializes, without redefining, the independent owners for slots and tuple headers
(§§5.3–5.8), FSM (§6), pins/latches/writeback (§§7.6–7.12), index RID references
(§§8.20–8.23), transaction status/snapshots/lifecycle (§§9.4–9.15), visibility and
`RETIRED` (§§10.2–10.5), `TUPLE_WRITE` (§§11.2–11.13), WAL/page publication
(§§12.8–12.17), control/checkpoint/recovery (§§13.2–13.19), DML handoff (§§15.3–15.6),
and failures (§39.1). Correctness procedures remain separate from Vacuum Benchmarks.

### Deterministic reclamation harness and independent oracles

Core races use barriers at semantic events, never elapsed-time sleeps. Instrumentation may
observe these conceptual events without requiring production function or container names:

```text
TxnId issued / transaction registration begins / write-status dependency becomes visible
transaction admitted / C4 or A2 publishes / dependency releases at C5 or A3
SQL snapshot or StatusHistoryGuard registers/releases / status lookup begins/reads result
tuple validated / creator or deleter status resolved / freeze or normalization proven
maintenance WAL privately encoded / authorized / durable / page mutation and page_lsn publish
cutoff candidate computed / dependency proof revalidated / required WAL target fixed
control-slot write begins / control fdatasync completes / runtime cutoff publishes
status frame retirement begins / sparse deallocation begins/completes
read epoch requested/visible/exits / RID retires at E / grace becomes true
TUPLE_WRITE request begins/becomes queued/granted/canceled/releases
index cleanup / prev_RID splice / NORMAL -> DEAD / zero-claim check / DEAD -> UNUSED
same-slot allocation / crash / recovery / READY
```

Crash fixtures construct exact persistent prefixes: choose the complete WAL prefix, data
page images, valid or torn control slot, and sparse page presence independently; omit later
operations; then reopen through the normal recovery gate. Runtime races use barriers and
state snapshots. Stress/random scheduling is complementary, never the correctness oracle.

Every negative fixture is canonical except for one missing proof or injected defect. In
particular, the epoch-only test clears lock/index/predecessor barriers, the lock-only test
clears epochs and persistent references, and the durability test completes semantic
normalization in memory while withholding only required WAL durability.

The independent oracles are:

- **visibility:** evaluate §§10.2–10.4 directly from fixture snapshot/status/header values;
- **status mapping:** compute `ordinal=T-2`, `PageNo=1+ordinal/32640`, and entry position
  with widened checked arithmetic, independently of production helpers;
- **cutoff alignment:** enumerate whole status pages and choose the greatest legal
  page-aligned exclusive cutoff satisfying every independently modeled bound;
- **epoch grace:** for retire epoch `R`, require no active `E <= R`;
- **RID reuse:** explicitly conjoin global deadness, index absence, persistent DEAD,
  predecessor absence, epoch grace, zero live `TUPLE_WRITE` claims, frame/page eligibility,
  and canonical UNUSED publication;
- **WAL prefix:** derive `durable_lsn` and retained reconstruction records from exact
  persisted complete records and DPT `rec_lsn`, not a production flush result;
- **control authority:** decode/CRC-check both §13.2 slots and independently select the
  greatest valid usable generation.

The harness must not require one vacuum thread, mutex, claim counter, epoch container,
checkpoint cadence, scheduling interval, or sparse-deallocation API. Observable ordering
and persistent results are the oracle.

### Protection-domain matrix

| Protection | What the fixture protects | Lifetime | Kind | Logical-deadness effect | Cutoff effect | Same-RID reuse effect | Crash | Substitute? |
|---|---|---|---|---|---|---|---|---|
| SQL snapshot | logical visibility history | RC attempt or retained RR snapshot | runtime | may block | contributes persistent-proof horizon | only through deadness | not replayed | no |
| page latch | page bytes/structure | short critical section | runtime | none after release | none | not a substitute | absent | no |
| BufferPool pin | resident frame binding | guard lifetime | runtime | none | coordinates status-frame retirement | not a substitute | absent | no |
| read epoch | retained physical RID identity | RID-retaining operation | runtime | none | none | blocks grace/reuse | absent | no |
| live `TUPLE_WRITE` request | one exact writer-target RID | queued/granted request through cancellation or C5/A3 | runtime | none by itself | none by itself | blocks reuse | absent | no |
| `StatusHistoryGuard(G)` | status lookups for normal TxnIds `>=G` | maintenance lookup/recheck | runtime | none | blocks cutoff above `G` | none | absent | no |
| write-status dependency | transaction's own possible future TxnId publication | registration through C5/A3 | runtime | none | blocks retirement of own TxnId | none | absent | no |

No row substitutes for another outside its stated domain.

### Write-status registration, lifetime, and snapshot contrast

Pause write-capable transaction registration before and after its own write-status
dependency becomes coordinator-visible and before transaction admission. Assert that no
persistent write can begin before both active registration and the dependency are visible;
there is no first-write admission path. Race the pause with cutoff calculation in both
orders:

1. dependency first: any candidate that would retire `T` is clamped to the greatest legal
   aligned cutoff not passing `T`;
2. cutoff `C` first: the subsequently issued/admitted TxnId is `>=C` and registers before
   write admission;
3. pause between assignment and registration: exactly one of those orders wins, never
   `T<C` followed by unpinned write-capable admission.

For an idle READ COMMITTED transaction, unregister the statement snapshot while retaining
write capability; cutoff still cannot pass its own TxnId, while
`global_oldest_snapshot_xmin` excludes it. An explicitly read-only transaction has no
write-status dependency, may separately hold an RC/RR snapshot, and rejects every
persistent-write attempt rather than changing mode after admission.

Pause commit at C3, C4, and before/after C5, and abort at MUST_ABORT, A2, and before/after
A3. The dependency remains through C4/A2 and failed ACTIVE statements and disappears only
during C5/A3. After release, prohibit any new self-TxnId publication, but independently
retain the cutoff blocker while an existing tuple/catalog `xmin` or effective `xmax` still
needs the outcome.

The mandatory race/lifetime matrix is:

| Case | SQL snapshot | Write-status dependency | Cutoff may pass own T? | Existing persistent dependency | Expected |
|---|---|---|---|---|---|
| write-capable T registers first | optional | yes | no | optional | aligned candidate clamps |
| cutoff C publishes first | none yet | registered before admission | only because new `T>=C` | none yet | no retired-self TxnId |
| assignment/registration race | optional | one legal linearization | never while `T<C` is admitted | optional | no gap |
| idle RC write-capable T | none | yes | no | optional | own status pinned only |
| RR write-capable T | retained | yes | no | optional | two independent bounds |
| explicit read-only T | optional | no | according to snapshot/persistent proof | none creatable | write rejected |
| C4 before C5 | none required | yes | no | maybe | terminal publication insufficient |
| after C5, persisted xmin | none required | no | no | yes | persistent proof still blocks |
| A2 before A3 | none required | yes | no | maybe | terminal publication insufficient |
| after A3, persisted metadata | none required | no | no | yes | normalization/removal still required |

The Chapter-9 snapshot procedures remain the owner of RC/RR fields. This matrix asserts
only that snapshot-horizon and status-history bounds are never conflated.

### StatusHistoryGuard, lookup, and `RETIRED`

Acquire `StatusHistoryGuard(G)` before a status-dependent scan and pause before reading a
terminal value. A candidate cutoff greater than `G`, runtime publication, and physical page
retirement must remain blocked until guard release and revalidation. In the opposite order,
publish `C`, then begin lookup for `T<C`; lookup returns `RETIRED` from cutoff precedence
without page access and no new guard resurrects retired history. Boundary fixtures use
`T=G-1`, `T=G`, and larger values to prove the architecture's lower-bounded protected
range, not an invented global or half-open interval.

For otherwise valid persisted metadata that still requires `Status(T)`, force `RETIRED`
separately for creator `xmin` and effective deleter `xmax`. Both take the §9.13/§10.4
reclamation-invariant failure path; neither outcome is guessed. Repeat with persisted
`INVALID`, recognized `RESERVED`, status I/O failure, checksum corruption, and unsupported
status format. Assert no freeze, normalization, DEAD decision, or page mutation is
authorized. The Transaction-status lookup and MVCC Visibility sections remain the detailed
owners of precedence and exact failure classification.

### Creator freezing, aborted-xmax normalization, and status independence

Creator-freeze fixtures use a valid committed creator at the exact §14.13.2 horizon:
just eligible, equality boundary, and one relation too new. Independently evaluate every
legal future snapshot before and after WAL-backed `xmin=FROZEN_TXN_ID,cmin=0`; visibility
must be identical and only the creator dependency disappears. A normal `xmax` remains a
separate dependency. ABORTED, IN_PROGRESS, required-RETIRED, INVALID, RESERVED, and lookup
failure creator results prohibit freeze and leave bytes unchanged.

Normalization fixtures use a valid still-live tuple with `Status(xmax)=ABORTED`; publish
the WAL-backed `xmax=INVALID_TXN_ID,cmax=0` rewrite and independently prove pre/post
visibility equivalence. Creator dependency is unchanged and deleter dependency disappears.
COMMITTED, IN_PROGRESS, unsafe SELF, required-RETIRED, INVALID, RESERVED, and lookup
failure cases prohibit normalization. Pre-authorization failure restores/leaves the old
canonical bytes; post-authorization failure follows the existing §12.12 publication and
§39.1 rules without fabricated rollback.

The existing PAGE_DELTA/PAGE_IMAGE procedures remain the owner of record-family bytes and
assert that each authorized maintenance mutation installs its authorizing record-start LSN
as `page_lsn`, updates dirty/DPT state, and obeys WAL-before-data. Reclamation assumes no new
WAL record family or maintenance-commit record.

The status-independence matrix is:

| Persisted state | Creator dependency | Deleter dependency | Freeze? | Normalize? | Removal needed for represented dependency? | Cutoff may pass referenced T? | `RETIRED` legal afterward? |
|---|---|---|---|---|---|---|---|
| normal committed xmin | yes | as xmax says | when §14.13.2 eligible | no | freeze or removal | no before action | yes after crash-recoverable action |
| FROZEN xmin | no | as xmax says | already frozen | no | no creator removal | yes for old creator if other bounds clear | yes |
| aborted xmin | yes until version removal | as xmax says | no | no | yes | no before removal | yes after removal |
| normal committed xmax | as xmin says | yes | creator only | no | removal/authorized independence | no | yes only afterward |
| normal aborted xmax | as xmin says | yes | creator only | yes | normalization or removal | no | yes after recoverable action |
| normalized aborted xmax | as xmin says | no | creator only | already normalized | no deleter removal | yes for deleter if other bounds clear | yes |
| physically removed version | no | no | no | no | complete | yes if no other reference | yes |
| catalog MVCC tuple | same tuple rules | same tuple rules | when eligible | when aborted xmax | same as user tuple | only after equivalent proof | only afterward |

Freezing is also tested against a live physical tuple: it does not mark DEAD, remove an
index, satisfy epoch grace, or clear a lock claim. Normalizing aborted `xmax` generally
makes a tuple semantically more live and never authorizes DEAD/reuse. A two-axis fixture
permits status retirement for status-independent metadata while a stale index, epoch, or
lock claim still forbids physical RID reuse.
A write-capable transaction's own write-status dependency, when it retains no RID, likewise
does not block reuse of an unrelated RID.

### Cutoff domain, prerequisite durability, and second-crash retention

For candidate `C`, build the dependency set independently, round down to the greatest legal
exclusive status-page boundary using the 32,640-entry mapping, and test `T=C-1`, `T=C`, a
candidate inside a page, the greatest legal aligned cutoff, and attempted overflow. `T<C`
is retired only after proof; `T=C` remains outside the retired range. No arithmetic wraps
or partially retires a status page.

For every freeze, normalization, or removal prerequisite at authorizing record-start LSN
`L`, select exactly one proof:

```text
durable page: the status-independent page image is stably stored and no longer needs L
durable WAL:  the old disk image remains, but the complete reconstructive WAL is durable
              and retained through the page's DPT rec_lsn dependency
```

With multiple prerequisites at `L1<L2<L3`, independently exclude already-durable pages and
set `required_wal_target` to the maximum authorizing LSN among the remaining WAL-dependent
pages. Pause WAL flush below and at that complete-record target. A successful
`fdatasync(database.control)` is never accepted as evidence that unrelated WAL reached
durability.

The mandatory durability matrix is:

| State | Semantically independent in memory? | Crash-recoverable? | Cutoff may publish? | WAL retained afterward? | READY after crash? |
|---|---:|---:|---:|---:|---:|
| dirty freeze, WAL undurable | yes | no | no | required but absent | old cutoff only |
| dirty freeze, WAL durable/retained | yes | yes | yes if other proofs hold | yes | yes after redo |
| durable frozen page | yes | yes | yes | only ordinary remaining dependencies | yes |
| dirty normalized xmax, WAL undurable | yes | no | no | required but absent | old cutoff only |
| dirty normalized xmax, WAL durable/retained | yes | yes | yes if other proofs hold | yes | yes after redo |
| control write pending | according to page/WAL proof | yes | not authoritative | yes | old valid slot selected |
| control durable, old data page | yes | yes via WAL | already published | yes | redo before READY |
| recycling crosses required rec_lsn | yes | would become no | forbidden | yes | not constructible |
| recovered dirty page before second crash | yes | yes only while WAL retained | already published | yes until dependency clears | yes |

Direct fixtures include:

1. volatile-only freeze and volatile-only normalization: control publication remains old;
2. durable status-independent page: publication needs neither a page force nor a checkpoint;
3. dirty old disk page plus durable retained WAL: publish `C`, crash, redo before READY;
4. required WAL durable but control publication fails: old cutoff remains authoritative and
   authorized maintenance progress remains valid;
5. control `C` durable plus old disk page plus missing/recycled WAL: impossible because the
   recycling floor refuses to cross the page's `rec_lsn`;
6. second crash: recover the dirty page after the first crash, leave it dirty under
   NO-FORCE, attempt recycling, crash again, and require the same reconstruction source.

WAL flush failure before control publication leaves the old cutoff. WAL authority
uncertainty follows the existing noncontinuable/no-guessing procedure. A torn or failed
new control slot is independently decoded on reopen; a complete durable new slot may be
selected only because every prerequisite was already recoverable. Runtime cutoff
publication is observed after durable control publication and before any physical status
page retirement that relies on it.

The checkpoint/retention matrix composes the Checkpoint/Recovery and WAL procedures:

| Case | Required `rec_lsn`/WAL | Cutoff publication | Recycling | Crash oracle |
|---|---|---|---|---|
| dirty frozen heap page | freeze dirty-interval `rec_lsn` | allowed after WAL durable | no crossing dependency | redo freeze |
| dirty normalized heap/catalog page | normalization dirty-interval `rec_lsn` | allowed after WAL durable | no crossing dependency | redo normalization |
| dirty TXN_STATUS page, `F/T` | `rec_lsn=F`, `page_lsn=T` | cutoff alone changes nothing | retain F until durable or safely retired | reconstruct from F then T |
| checkpoint before cutoff | captured DPT floor | later cutoff uses it | only installed floor | old/new cutoff both recoverable |
| checkpoint after cutoff | every still-dirty prerequisite represented | already authoritative | no premature advance | selected pair recoverable |
| cutoff durable, page dirty | prerequisite `rec_lsn` retained | yes | forbidden past it | old disk page repaired |
| WAL segment candidate | minimum of all DPT/writer/checkpoint needs | irrelevant by itself | only if below complete floor | no lost source |
| page durable/dependency cleared | no page need | yes | ordinary recycling allowed | stable page sufficient |

Race freeze, normalization, and status-page retirement independently against checkpoint
DPT capture. Force both legal linearizations and assert that the installed checkpoint
either captures the old recovery dependency or the new dirty interval. Then combine durable
cutoff, dirty page, checkpoint, recycling attempt, and second crash. No fixture introduces
a cutoff WAL record, forces a checkpoint, or forces the affected data page.

### Read epochs, `TUPLE_WRITE` handoff, and claim lifetime

Pause read-epoch registration before and after the reader becomes visible to the reclaimer,
then race RID retirement/final reuse. Registration first blocks grace; reuse first means the
reader cannot validate/retain the old identity. For retire epoch `R`, independently test
active epochs `R-1`, `R`, and `R+1`: either of the first two blocks, while `R+1` alone does
not. A long-lived epoch may cause wait or deferral but never forced reuse. After the last
blocking exit, the candidate is revalidated before final publication; no notification
mechanism is assumed.

At `current_epoch=UINT64_MAX-1`, retire once and observe the last legal increment state;
the next retirement attempt fails/requires maintenance restart before reuse and never wraps
to zero or aliases an old reader. Page latches and pins are tested separately: a latch
protects bytes, a pin protects frame binding, and neither replaces an epoch once a RID is
retained outside that operation.

For a writer, pause request registration while the discovery epoch remains active. Reuse is
blocked by the epoch until the request becomes a canonical live queued or granted claim;
only then may the epoch release, and it releases before blocking. Cover immediate grant,
one queued waiter, three queued waiters, same-owner effective ownership, and resource
failure before the request becomes live. Failure leaves the epoch held and permits only the
existing fail/retry path.

The mandatory `TUPLE_WRITE` matrix is:

| State | Epoch active? | Live claim? | Grant possible? | Reuse? | Claim release | Revalidation? |
|---|---:|---:|---:|---:|---|---:|
| no request, no retained RID | no | no | no | other barriers decide | n/a | at reuse |
| registration in progress | yes | not yet | no | no | n/a | yes |
| immediate grant before epoch exit | yes, then no | yes | granted | no | C5/A3 | yes |
| queued waiter | no after registration | yes | yes | no | cancellation or grant | yes after grant |
| multiple waiters | no | yes for each | yes | no until all gone | individually linearized | yes |
| granted holder | no | yes | granted | no | C5/A3 | yes |
| C4 before C5 | no | yes | granted | no | C5 | yes |
| A2 before A3 | no | yes | granted | no | A3 | yes |
| cancellation/grant race | no | exactly winner's state | exactly winner | no if grant wins | cancellation or C5/A3 | if granted |
| queued deadlock victim | no | until canonical removal | until removal | no while possible | removal with no late grant | no continuation |
| granted deadlock victim | no | yes | granted | no | A3 | no ordinary continuation |
| crash with waiter | process ends | no after reopen | no | persistent barriers decide | process death | fresh discovery only |

Cancellation and grant are forced in both orders. Grant first converts continuously to
owner claim; cancellation first simultaneously removes grant eligibility and the claim.
No ended request may receive a late grant. A granted deadlock victim and every MUST_ABORT
holder remain through A3. Commit remains pinned through C4 until C5.

Race request registration against the reclaimer's zero-claim/reuse publication while the
writer still holds its epoch. Registration first blocks reuse. If reuse has already won,
epoch-protected identity revalidation prevents old-identity request admission. Post-wait
revalidation remains mandatory: mutate `xmax`, visibility, current-owner, or key state while
the waiter sleeps and require the existing Chapter-11/15 outcome despite stable RID
identity.

The read-epoch matrix is:

| Case | DEAD allowed? | UNUSED allowed? | Same-RID reuse? | Reclaimer action | Proof |
|---|---:|---:|---:|---|---|
| reader registered before retirement | yes after index cleanup | no | no | wait/defer | active `E<=R` |
| registration races reuse | legal winner only | only reuse-first | only reuse-first | revalidate | shared linearization |
| old epoch active | yes | no | no | wait/defer | minimum active `<=R` |
| grace complete | yes | other barriers decide | other barriers decide | continue/revalidate | no active `E<=R` |
| no old epoch, lock claim remains | yes | no | no | wait/defer | independent claim |
| long reader | yes | no | no | wait/defer indefinitely | no forced reuse |
| crash | persistent DEAD remains | only after fresh retirement/grace and other proof | only after every proof | re-enqueue | runtime epochs absent |

### Index cleanup, predecessor links, DEAD/UNUSED, and complete reuse

For one reclaimable NORMAL RID referenced by at least two indexes, derive every historical
key, remove exact `(key,RID)` entries through ordinary B+ MTRs, and publish DEAD only after
all are known absent. Crash after zero, one, or all removals: a recovered NORMAL repeats
idempotent cleanup; recovered DEAD proves cleanup preceded publication. Any index failure
leaves the candidate NORMAL or DEAD-but-unreused according to the already-authorized prefix;
it never permits first-index-only success. Include a UNIQUE index to prove vacuum removes
the physical stale entry without rerunning SQL uniqueness admission.

Construct removable middle/head-adjacent/consecutive chain versions and surviving direct
successors. WAL-backed splicing must replace each `S.prev=V` with `S.prev=V.prev`; state
change or cleanup failure defers reuse. A stale index or `prev_RID` is isolated as the sole
remaining barrier and must prevent new-identity aliasing.

The complete one-missing-barrier matrix is below. “DEAD published” means complete
WAL-backed runtime publication under §12.12, not an additional page force or WAL
synchronization before reuse. Crash fixtures use the surviving valid WAL prefix;
§14.14 separately owns the stronger durable proof needed for status-cutoff
advancement.

| Fixture | Global dead | Indexes absent | DEAD published | `prev_RID` absent | Epoch grace | Zero lock claims | Frame/page valid | DEAD allowed? | UNUSED allowed? | Same-RID reuse? |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| not globally dead | no | yes | no | yes | yes | yes | yes | no | no | no |
| one stale index | yes | no | no | yes | yes | yes | yes | no | no | no |
| DEAD not published | yes | yes | no | yes | yes | yes | yes | publication pending | no | no |
| stale predecessor | yes | yes | yes | no | yes | yes | yes | yes | no | no |
| old epoch | yes | yes | yes | yes | no | yes | yes | yes | no | no |
| queued/granted claim | yes | yes | yes | yes | yes | no | yes | yes | no | no |
| frame/page transition ineligible | yes | yes | yes | yes | yes | yes | no | yes | no | no |
| every barrier clear | yes | yes | yes | yes | yes | yes | yes | yes | yes | yes |

For the positive row, compact while still DEAD if needed, assert zero tuple coordinates,
publish exactly one free-list link and `UNUSED`, then allocate a new tuple into that same
SlotId. The independent reuse oracle proves no old reader, waiter, index entry, predecessor,
or resident-frame transition can treat the numeric RID as the old tuple. `NORMAL->UNUSED`
bypass and direct allocation from DEAD are rejected.

Crash fixtures cover: waiter present with DEAD persisted; legal UNUSED before crash; and a
WAL-valid new same-RID allocation. Reopen has no pre-crash epoch, guard, write-status claim,
or lock request. DEAD remains DEAD and is freshly retired before reuse; UNUSED retains its
canonical free-list semantics; new allocation never resurrects an old runtime identity.

Hold an old RR snapshot to prevent global deadness, then separately hold only a long read
epoch after logical death. The first blocks tuple garbage eligibility; the second permits
status-independent maintenance but blocks physical reuse. Old committed normal `xmin` and
surviving normal committed `xmax` independently block status cutoff until freeze,
normalization/removal, or another authorized status-independent representation becomes
crash-recoverable. Positive cases then allow retirement after FROZEN creator, normalized
aborted `xmax`, or fully removed version, subject to all other cutoff bounds.

### Status-page reclamation, control crashes, and sparse deallocation

Use the independent status mapping to select a page wholly below `C`, the boundary page
containing `C`, and an interior page followed by later populated pages. Only the wholly
retired page may proceed through guard exclusion, pin/I/O drain, frame retirement,
BufferPool/DPT removal, and optional sparse deallocation. Later PageNos and file length
remain address-stable. `T<C` returns `RETIRED` without page access; a missing page required
for `T>=C` follows the owning I/O/corruption result.

Run two abstract storage capabilities:

1. keep-size sparse deallocation succeeds for an interior whole-page range; allocated
   backing may disappear while logical size and offsets remain unchanged;
2. deallocation is unavailable or fails; bytes remain safe extra storage, durable cutoff
   stays authoritative, and ordinary retry/resource policy applies.

No test requires a platform syscall or block-allocation granularity. Crash during the
physical optimization may leave any platform-valid allocation state wholly below the
already-authoritative cutoff, but the logical file remains correctly sized/addressable.

The mandatory cutoff/control/status-page crash matrix is:

| Persistent boundary | Selected cutoff | Status page required? | Maintenance WAL required? | Page absence legal? | Recovery action | READY? |
|---|---|---:|---:|---:|---|---:|
| old cutoff, old dependent page | old | yes | according to old DPT | no | ordinary recovery | yes if valid |
| maintenance WAL durable, control old | old | yes | retained/replayable | no premature reclaim | replay extra progress | yes |
| torn new control | older valid slot | yes under old cutoff | retained | no if old cutoff needs it | select old slot | yes if inputs present |
| new cutoff durable, status page present | new `C` | no below C | as pages require | yes but unnecessary | ignore stale status bytes | yes |
| new cutoff durable, page sparsely reclaimed | new `C` | no below C | as pages require | yes | `RETIRED` precedence | yes |
| crash during sparse deallocation | new `C` | no below C | unchanged | yes wholly below C | preserve logical mapping | yes |
| new `C`, dirty frozen heap page | new `C` | no below C | yes | yes | redo heap before READY | yes |
| second crash before heap writeback | new `C` | no below C | retained dirty-page source | yes | redo again | yes |
| failed candidate, old-control fallback | old | yes | old recovery floor | no premature absence | select old slot | yes if inputs remain |

Directly prohibit physical status-page retirement whose legality depends on `C` before the
new control generation is durable. Publish `C` while retaining the old status page and
verify stale storage is harmless; then retire it after every guard/frame/DPT condition and
verify absence is accepted. Pin a status frame and race retirement: the frame remains bound
until canonical drain and cannot later rewrite sparsely deallocated storage. Hold a
`StatusHistoryGuard` and prove the same page cannot be invalidated until guard release.

For a dirty TXN_STATUS page with `rec_lsn=F,page_lsn=T`, cutoff publication alone never
releases `F`. Case A keeps the page semantically required and retains F. Case B makes the
whole page retired below durable `C`, drains pins/I/O, and removes its BufferPool/DPT
ownership through §14.14.2 before its own reconstruction dependency may clear. Heap/catalog
`rec_lsn` dependencies are never cleared by status-page retirement.

### VACUUM manifest admission versus CREATE INDEX publication

Architecture §§11.13.5, 14.9, 14.17.1 and 21.8.2 own this coordination.
Use a committed table T with indexes I0 and a known live row R. A test-owned
logical row/DDL ledger defines the complete committed manifest independently of
VACUUM's retained descriptor. Create a second table U to expose accidental global
maintenance serialization. Instrument actual pass admission, descriptor capture,
publication-exclusion closure, current-unit completion/restore, pass release,
catalog publication and C4/C5/A2/A3 events. Timing and queued-thread count alone
are not proof that either contender reached its ownership boundary.

1. Admit V on T with manifest M0=I0 and pause it before R. Start CREATE INDEX I1;
   allow its private build to include `(key,RID_R)`. Observe its publication
   exclusion close admission and its drain request while V still owns its pass.
   No catalog index publication may occur. Queue V2 after exclusion closes and
   prove it reaches the real admission boundary without gaining mutation authority.
2. Release V to finish or quiesce its current unit and release pass ownership.
   Only then allow catalog publication. Keep the DDL transaction open: V2 still
   cannot acquire a pass, including after the CREATE INDEX statement returns.
   A maintenance pass on U can be admitted and perform a unit throughout this wait.
3. Commit the DDL transaction. After C5 release, V2 must capture M1=I0+I1, even
   if it cached M0 before waiting. Delete R in a subsequent transaction and commit;
   make it globally garbage-eligible, then let V2 reach it. Independently enumerate
   every committed index from the test DDL ledger and decode exact entries. No
   `(key,RID_R)` may survive when NORMAL -> DEAD publishes. Drive every ordinary
   epoch/lock/link barrier to completion, reuse the RID with different bytes/key,
   and prove no old index entry resolves to the new row.
4. Reverse admission order: close exclusion before any pass wins. A stale cached
   descriptor cannot authorize a new pass until terminal disposition and fresh
   manifest resolution. Repeat with an already admitted V finishing a WAL-backed
   unit and with V exactly restoring a provisional unit. Both end pass authority
   before DDL catalog publication; neither resumes M0 after publication.

Use direct negative oracles: a deliberately supplied stale M0 at resumed admission
must not become mutation authority, and a fixture containing a surviving I1 entry
must fail the complete-index-absence oracle even if every I0 entry is gone. These
contrasts prevent agreement between VACUUM's omitted-index list and the test's
expected list from falsely passing. Require actual I1 build entries, target pass
admission attempts, and subsequent garbage eligibility to establish nonvacuity.

Repeat the schedule across these boundaries:

| Boundary / variation | Required observation |
|---|---|
| Cancel/fail while drain is pending, before catalog publication | Attempt is made unable to publish or receive a late grant before attempt-only exclusion releases; V2 can resolve the unchanged committed manifest. |
| Abort after catalog index publication | Exclusion remains through A3; V2 resolves M0, never aborted I1; ordinary orphan-file cleanup remains separate. |
| C4 or A2 reached but C5/A3 release paused | Admission remains excluded; terminal status observation alone is insufficient. |
| Same transaction creates I1 then I2 | Retained exclusion is reused; failure of the later attempt does not release earlier ownership; a post-terminal pass sees exactly the committed resulting set. |
| Connection loss or deadlock victim after publication | Ordinary abort retains exclusion through A3, with no late grant/publication by a canceled request. |
| V would wait for a transaction gate/logical lock held by DDL | V completes/restores its current unit and releases pass ownership before waiting; no hidden transaction-to-maintenance cycle. |
| DROP and ANALYZE contention | Compose existing RETIRING/use-drain and STATS_PUBLISH/MANIFEST_CHANGE cases; publication exclusion does not invent a DROP use-drain wait or bypass the transaction graph. |
| DRAINING or NONCONTINUABLE | Existing lifecycle admission and unit/transaction shutdown rules apply; uncertainty does not reopen ordinary maintenance. |

Crash at pre-publication, uncommitted catalog publication, terminal DDL, index
cleanup prefix, DEAD and post-reuse boundaries using the existing WAL/persistent-
prefix fixture. Independently derive the committed catalog manifest from terminal
records and catalog rows; recovery must discard runtime pass/exclusion claims and
resolve that recovered manifest before admitting maintenance. Check live index
entries against heap identities after each restart. Uncommitted private/final
index files follow ordinary orphan rules and are not a manifest oracle. This
procedure composes with the existing B+ exact-delete, complete RID reuse,
transaction-gate, catalog publication, lifecycle and recovery procedures; it does
not introduce a new persistent exclusion record or require a specific lock type.

### FSM, lifecycle, scheduling, and failure methodology

After authoritative heap reclamation, inject stale-low and stale-high FSM values. Stale-low
may miss space; stale-high must be rejected by heap-page geometry recheck. FSM update
failure leaves legal heap state authoritative and the FSM rebuildable. Page compaction may
move tuple bytes while preserving SlotIds and cannot substitute for DEAD/UNUSED proof.
If retained status blocks contribute to a later storage-full condition, the next legitimate
write receives the existing storage/resource error; the fixture never permits status-page
compaction, renumbering, or a weakened cutoff proof.

Every permitted manual or background trigger runs the same safety procedures. Tests do not
assert cadence, threshold, worker count, fairness, or prompt progress. A guard, epoch, lock
claim, stale reference, or insufficient durability may produce wait, skip, or defer where
§14.17 leaves policy free; the oracle is only that unsafe cutoff/reuse does not publish.

Lifecycle barriers map to Database Lifecycle Tests: maintenance is admitted only in READY;
DRAINING admits no new pass and drives active transactions through ordinary C5/A3 while
maintenance completes/restores its current unit and quiesces; CLOSING, RECOVERING, and
NONCONTINUABLE admit no new reclamation mutation. After crash, epochs, guards,
write-status dependencies, and lock requests are absent; only persistent page/control/WAL/
index/predecessor state governs recovery. No runtime claim is replayed.

The failure matrix is:

| Failure/blocker | Mutation authorized? | Cutoff changes? | RID reusable? | Continuation/result owner |
|---|---:|---:|---:|---|
| candidate not globally reclaimable | no | no | no | benign skip/defer, §14.3 |
| active snapshot | no reclaim action | no past dependency | no | wait/defer, §§14.2–14.3 |
| active epoch | DEAD may exist | unaffected | no | wait/defer, §14.6 |
| live `TUPLE_WRITE` claim | DEAD may exist | unaffected | no | wait/defer, §§11.12, 14.12 |
| status lookup I/O | no dependent rewrite | no | no new permission | owning status I/O result, §39.1 |
| tuple/page corruption | no | no | no | corruption owner, §§4.13, 39.1 |
| required `RETIRED` dependency | no | no | no | reclamation invariant/noncontinuable, §§9.13, 14.17.1 |
| required INVALID/RESERVED | no | no | no | existing invariant result, §§9.11–9.13 |
| WAL failure before authorization | no publication | no | no new permission | existing pre-authorization result, §12.12 |
| WAL failure after authorization | authoritative progress | only if later proofs finish | only after all barriers | §12.12/§39.1 outcome |
| prerequisite WAL flush failure | page progress may exist | no new cutoff | unaffected | maintenance fails/defers |
| WAL authority uncertainty | uncertain unit | no new cutoff | no | existing noncontinuable path |
| control publication failure | prerequisites may remain valid | old cutoff | unaffected | old slot authority / I/O result |
| sparse deallocation failure | cutoff already authoritative | unchanged | unrelated | safe extra storage / retry policy |
| index cleanup failure | partial MTR prefix may exist | unaffected | no | exact B+ error; retry idempotently |
| `prev_RID` splice failure | authorized prefix only | unaffected | no | defer/error owner |
| runtime workspace exhaustion | no unsafe partial publication | no | no | existing resource result; fail/defer |
| claim-registration exhaustion | no live request | unaffected | epoch still blocks | Chapter-11/§39 result |
| shutdown cancellation | only completed/restored unit | no unsafe advance | no unsafe reuse | lifecycle owner |

### High-level reclamation domain/case matrix

| Domain/case | Deterministic fixture/fault | Independent oracle | Architecture | Verification owner |
|---|---|---|---|---|
| write-status registration | pause before admission | coordinator event order | §§9.4, 14.17.1 | registration procedure |
| idle RC writer | no snapshot, candidate past T | separate horizon/dependency sets | §§14.2, 14.17.1 | write-status matrix |
| explicit read-only | retained snapshot/no dependency | mode/write-admission oracle | §§9.15, 14.2 | write-status matrix |
| C5/A3 release | pause each terminal stage | lifecycle stage oracle | §§9.14, 14.14 | lifetime procedure |
| persistent dependency after release | stored normal xmin/xmax | status-dependency table | §14.14 | status matrix |
| creator freeze | eligible committed creator | visibility oracle | §14.13.2 | freeze procedure |
| aborted xmax | ABORTED deleter | visibility oracle | §14.13.1 | normalization procedure |
| required RETIRED | valid dependent field | status precedence oracle | §§9.13, 14.14.3 | negative lookup |
| cutoff alignment | inside-page candidate | widened page arithmetic | §14.14.1 | cutoff procedure |
| candidate clamp | active dependency/guard | independent bound set | §14.17.1 | coordinator races |
| guard race | pause guarded lookup | event order | §14.17.1 | guard procedure |
| volatile freeze | dirty memory, WAL omitted | persistent-prefix oracle | §14.14.2 | durability matrix |
| dirty freeze, durable WAL | old disk page | WAL/DPT oracle | §14.14.2 | crash fixture |
| second crash | recovered page still dirty | retention-floor oracle | §§13.10, 14.14.2 | second-crash fixture |
| epoch registration | registration/reuse barriers | epoch-set oracle | §14.6 | epoch procedure |
| grace boundary | E=R-1/R/R+1 | mathematical predicate | §14.6.3 | epoch matrix |
| queued waiter | live request, epoch released | claim-state oracle | §§11.12, 14.12 | lock matrix |
| granted owner | pause C4/A2 | lifecycle/claim oracle | §§11.11, 14.12 | lock matrix |
| waiter cancellation | cancel/grant both orders | no-late-grant oracle | §11.12 | cancellation procedure |
| DEAD with waiter | all other barriers clear | reuse conjunction | §§14.5, 14.12 | reuse matrix |
| manifest publication | admitted old pass versus CREATE INDEX | independent DDL ledger + admission/terminal events | §§14.17.1, 21.8.2 | VACUUM manifest admission versus CREATE INDEX publication |
| stale index | only index barrier missing | exact B+ entry set | §§8.23, 14.9 | reuse matrix |
| stale predecessor | only link barrier missing | decoded link set | §14.10 | reuse matrix |
| complete reuse | every barrier true | explicit conjunction | §14.12 | positive reuse |
| sparse status reclaim | interior page below C | mapping/file-length oracle | §14.14 | sparse procedure |
| torn control | invalid new slot | independent slot selection | §§13.2, 14.14.2 | crash matrix |
| reclaimed lookup below C | absent page | cutoff precedence | §14.14.3 | lookup fixture |
| missing required page | absent page at/above C | mapping/status oracle | §§9.13, 14.14.3 | negative fixture |

All core negative cases isolate one defect, use barriers or exact persistent prefixes, and
remain valid under any conforming scheduling, lock-table, epoch-registry, BufferPool, WAL,
checkpoint, or sparse-storage implementation.

### Chapter 14 atomic architecture-obligation coverage map

The inventory below is derived from the final live §§14.1–14.18 contract. Each row requires the named direct procedure or precise existing owner to supply a
deterministic fixture and independent oracle.

| # | Domain | Atomic obligation | Architecture owner | Verification owner / deterministic procedure |
|---:|---|---|---|---|
| 1 | A STATUS-DEPENDENCY MODEL | Normal committed xmin requires creator outcome | §§14.13–14.14 | Status-independence matrix |
| 2 | A | Effective normal xmax requires deleter outcome | §§14.13–14.14 | Status-independence matrix |
| 3 | A | Opaque numeric occurrences do not automatically pin status | §14.14 | Status table plus Statistics verification |
| 4 | A | Runtime future-publication and persisted dependencies are distinct | §14.14 | Write-status lifetime contrast |
| 5 | B WRITE-STATUS REGISTRATION | Every write-capable normal transaction registers own dependency | §§9.4, 14.17.1 | Registration barrier fixture |
| 6 | B | Registration precedes persistent-write admission | §§9.4, 14.17.1 | Pre-admission event assertion |
| 7 | B | Registration is not lazy first-write work | §§9.4, 14.17.1 | No-first-write-path oracle |
| 8 | B | Explicit read-only transaction may omit dependency | §§9.15, 14.17.1 | Read-only contrast |
| 9 | B | Assignment-to-registration admits no cutoff gap | §§9.8, 14.17.1 | Two-order barrier fixture |
| 10 | C WRITE-STATUS LIFETIME | ACTIVE and failed-ACTIVE retain dependency | §§9.4, 14.17.1 | Statement-failure pause |
| 11 | C | MUST_ABORT and ABORTING retain dependency | §§9.14, 14.17.1 | Abort-stage fixture |
| 12 | C | COMMITTING retains through C4 | §§9.14, 14.17.1 | Commit-stage fixture |
| 13 | C | Commit release occurs only during C5 | §§9.14, 14.17.1 | C4/C5 cutoff probes |
| 14 | C | Abort release occurs only during A3 | §§9.14, 14.17.1 | A2/A3 cutoff probes |
| 15 | D SQL SNAPSHOT HORIZON | Horizon is minimum registered snapshot xmin | §14.2 | Snapshot registry oracle |
| 16 | D | Idle RC writer does not pin logical horizon | §14.2 | RC no-snapshot contrast |
| 17 | D | Retained RR snapshot pins logical horizon | §14.2 | Long-RR fixture |
| 18 | D | Snapshot and write-status bounds remain independent | §§14.2, 14.17.1 | Three-case contrast matrix |
| 19 | E STATUSHISTORYGUARD | Guard registers before dependent lookup | §14.17.1 | Guard event order |
| 20 | E | Guard clamps cutoff above G | §14.17.1 | Guard-first race |
| 21 | E | Cutoff-first guard cannot resurrect retired history | §14.17.1 | Cutoff-first race |
| 22 | E | Multiple guards use minimum bound | §14.17.1 | Multi-guard boundary fixture |
| 23 | F STATUS LOOKUP / RETIRED | Below-cutoff lookup returns RETIRED without page read | §§9.13, 14.14.3 | Access-instrumented lookup |
| 24 | F | RETIRED dependent xmin is invariant failure | §§10.2, 14.14.3 | Creator negative fixture |
| 25 | F | RETIRED dependent xmax is invariant failure | §§10.3, 14.14.3 | Deleter negative fixture |
| 26 | F | INVALID/RESERVED required outcome is rejected | §§9.13, 14.17.1 | Status negative matrix |
| 27 | F | Status I/O/corruption/format errors propagate | §§9.13, 39.1 | Lookup failure injection |
| 28 | G CREATOR FREEZING | Only committed eligible creator freezes | §14.13.2 | Positive freeze fixture |
| 29 | G | Freeze writes xmin FROZEN and cmin zero | §14.13.2 | Independent tuple-byte oracle |
| 30 | G | Freeze preserves visibility for legal future snapshots | §§10.2, 14.13.2 | Independent visibility comparison |
| 31 | G | Freeze horizon boundary is exact | §§14.2, 14.13.2 | B-1/B/B+1 snapshot fixture |
| 32 | G | Invalid creator statuses never freeze | §§10.2, 14.13.2 | Creator negative matrix |
| 33 | G | Freeze leaves normal xmax dependency independent | §§14.13–14.14 | FROZEN+xmax fixture |
| 34 | H XMAX NORMALIZATION | Only proven ABORTED xmax normalizes | §14.13.1 | Positive normalization fixture |
| 35 | H | Normalization writes INVALID xmax and cmax zero | §14.13.1 | Independent tuple-byte oracle |
| 36 | H | Normalization preserves visibility | §§10.3, 14.13.1 | Independent visibility comparison |
| 37 | H | Normalization removes only deleter dependency | §§14.13–14.14 | Dependency table |
| 38 | H | Committed/in-progress/unsafe SELF xmax does not normalize | §§10.3, 14.13.1 | Xmax negative matrix |
| 39 | H | RETIRED/INVALID/RESERVED/failure does not normalize | §§10.3, 14.17.1 | Xmax negative matrix |
| 40 | I DEADNESS / RECLAIMABILITY | Aborted creator is garbage candidate | §14.3.1 | Status/garbage fixture |
| 41 | I | Committed delete is garbage only below snapshot horizon | §14.3.2 | Long-RR and boundary fixtures |
| 42 | I | IN_PROGRESS creator/deleter is skipped without latch wait | §14.4 | Barrier/latch event assertion |
| 43 | I | Garbage eligibility is global, not vacuum snapshot visibility | §§14.2–14.3 | Divergent-snapshot fixture |
| 44 | I | DEAD is semantically cleaned but not reusable | §§14.5, 14.8 | State-transition fixture |
| 45 | J STATUS-INDEPENDENCE | FROZEN creator removes creator dependency | §§14.13–14.14 | Positive retirement fixture |
| 46 | J | Normalized aborted xmax removes deleter dependency | §§14.13–14.14 | Positive retirement fixture |
| 47 | J | Physical removal removes tuple status dependencies | §14.14 | Removal/dependency fixture |
| 48 | J | Catalog MVCC metadata follows same proof | §14.14 | Catalog tuple specialization |
| 49 | J | Runtime dependency release alone is insufficient | §14.14 | C5/A3 persisted-reference fixture |
| 50 | J | Status-independent in memory is not crash proof | §14.14 | Volatile-page fixture |
| 51 | K STATUS CUTOFF DOMAIN | Cutoff is exclusive | §14.14.1 | T=C-1/C oracle |
| 52 | K | Cutoff is whole-page aligned | §14.14.1 | Independent 32640 arithmetic |
| 53 | K | Initial cutoff is first normal TxnId | §14.14.1 | Initial control fixture |
| 54 | K | Maximum aligned cutoff does not wrap | §§4.3, 14.14.1 | Synthetic exhaustion fixture |
| 55 | K | Cutoff is monotonic | §§14.14, 14.17.1 | C1/C2/control-generation fixture |
| 56 | L CUTOFF COORDINATION | Candidate accounts for every persistent dependency | §§14.14, 14.17.1 | Independent dependency-set oracle |
| 57 | L | Candidate accounts for write-status dependencies | §14.17.1 | Transaction-first race |
| 58 | L | Candidate accounts for SQL snapshots | §§14.2, 14.17.1 | Snapshot clamp fixture |
| 59 | L | Candidate accounts for guards | §14.17.1 | Guard-first race |
| 60 | L | Exactly one cutoff publisher wins | §14.17.1 | Two-reclaimer serialization fixture |
| 61 | L | Candidate is revalidated before publication | §§14.11, 14.17.1 | State-change barrier fixture |
| 62 | M CUTOFF PREREQUISITE DURABILITY | Volatile freeze with undurable WAL cannot authorize cutoff | §14.14.2 | Durability matrix |
| 63 | M | Volatile normalization with undurable WAL cannot authorize cutoff | §14.14.2 | Durability matrix |
| 64 | M | Durable status-independent page is sufficient | §14.14 | Durable-page fixture |
| 65 | M | Durable retained reconstructive WAL is sufficient | §14.14 | Dirty-page crash fixture |
| 66 | M | Checkpoint is not inherently required | §14.14 | No-checkpoint positive fixture |
| 67 | M | Data-page force is not inherently required | §14.14 | Dirty-page positive fixture |
| 68 | M | Required WAL target is max needed authorizing LSN | §14.14 | L1/L2/L3 independent oracle |
| 69 | N CUTOFF CONTROL PUBLICATION | Required WAL durability precedes control sync | §14.14.2 | Held-WAL/control-sync race |
| 70 | N | Control fdatasync does not durabilize WAL | §14.14.2 | Separate persistence fault domains |
| 71 | N | Durable control precedes runtime cutoff publication | §14.14.2 | Event-order assertion |
| 72 | N | Failed control publication leaves old cutoff | §14.14.2 | Alternating-slot fault fixture |
| 73 | N | Torn control selects a safe valid slot | §§13.2, 14.14.2 | Independent control oracle |
| 74 | N | Physical status retirement cannot precede durable C | §14.14.2 | Pre-cutoff reclaim rejection |
| 75 | O WAL RETENTION / SECOND CRASH | Dirty prerequisite retains DPT rec_lsn | §§13.7–13.10, 14.14 | DPT snapshot assertion |
| 76 | O | Cutoff publication does not clear rec_lsn | §14.14.2 | Before/after DPT comparison |
| 77 | O | Required WAL cannot recycle while page depends on it | §§13.10, 14.14.2 | Recycling refusal fixture |
| 78 | O | Recovery before READY reconstructs status independence | §§13.13, 14.14 | Dirty-old-page crash |
| 79 | O | Recovered dirty page retains WAL for second crash | §14.14.2 | Two-crash fixture |
| 80 | O | Durable page may clear ordinary WAL dependency | §§7.10, 13.10, 14.14 | Stable-page fixture |
| 81 | O | TXN_STATUS F remains retention base until cleared/safely retired | §§12.10.5, 13.10, 14.14.2 | F/T matrix |
| 82 | P TXN_STATUS PAGE RECLAMATION | Only page wholly below cutoff is eligible | §§14.14.1–14.14.2 | Whole/boundary page fixture |
| 83 | P | Guard exclusion precedes page retirement | §14.17.1 | Guard/page race |
| 84 | P | Pins and I/O drain before frame retirement | §14.14.2 | BufferPool retirement fixture |
| 85 | P | Retired frame cannot write obsolete contents | §14.14.2 | Delayed-writeback assertion |
| 86 | P | Reclaimed page below C is semantically unnecessary | §14.14.3 | Missing-below-C lookup |
| 87 | P | Missing required page at/above C is error | §§9.13, 14.14.3 | Missing-required-page fixture |
| 88 | Q SPARSE DEALLOCATION | Sparse reclaim preserves file length | §§14.14.2–14.14.3 | File-length oracle |
| 89 | Q | Sparse reclaim preserves absolute PageNos | §§9.12, 14.14 | Interior-page fixture |
| 90 | Q | Later pages are not shifted/renumbered | §14.14.2 | Offset/mapping comparison |
| 91 | Q | Unsupported sparse deallocation permits safe extra storage | §14.14.2 | Capability-absent fixture |
| 92 | Q | Failed sparse deallocation does not roll back cutoff | §14.14.2 | Post-cutoff failure fixture |
| 93 | R READ-EPOCH REGISTRATION | Reader registers before retaining index RID | §14.6.1 | Cursor event-order fixture |
| 94 | R | Registration/reuse race has two legal orders | §§14.6.1, 14.12 | Two-order barrier fixture |
| 95 | R | New reader after retirement cannot obtain removed entry | §14.6.2 | Post-retirement traversal fixture |
| 96 | R | Pure reader needs no TUPLE_WRITE claim | §§14.6.1, 14.7 | Reader-only fixture |
| 97 | S READ-EPOCH GRACE | Retirement records pre-increment epoch | §14.6.2 | Event/value assertion |
| 98 | S | Active E less than retire R blocks | §14.6.3 | Mathematical matrix |
| 99 | S | Active E equal R blocks | §14.6.3 | Mathematical matrix |
| 100 | S | Only E greater than R permits epoch component | §14.6.3 | Mathematical matrix |
| 101 | S | Long epoch may delay but never permit unsafe reuse | §§14.6.3, 14.17 | Long-reader fixture |
| 102 | T TUPLE_WRITE CLAIMS | Every queued grant-eligible request is a RID claim | §§11.12, 14.5 | Queued-waiter fixture |
| 103 | T | Every granted owner is a RID claim | §§11.11–11.12, 14.5 | Granted-holder fixture |
| 104 | T | Queue position does not change claim status | §11.12 | Multi-waiter fixture |
| 105 | T | Empty lock-table container is not a claim | §§11.12, 14.12 | Empty-container semantic fixture |
| 106 | T | Claim protects identity, not visibility/status | §§14.5, 14.7 | Changed-semantics revalidation fixture |
| 107 | U EPOCH-TO-LOCK HANDOFF | Discovery epoch remains through request registration | §§11.3, 14.6.1 | Paused-registration fixture |
| 108 | U | Immediate grant exists before epoch release | §§11.3, 14.6.1 | Immediate-grant fixture |
| 109 | U | Queued claim exists before epoch release | §§11.3, 14.6.1 | Queued-handoff fixture |
| 110 | U | Blocking wait retains no epoch/page latch | §§11.3, 14.7 | Wait-entry ownership assertion |
| 111 | V CLAIM CANCELLATION / RELEASE | Cancellation removes claim with grant eligibility | §§11.12, 14.12 | Cancellation-first fixture |
| 112 | V | Grant-first race preserves owner claim | §11.12 | Grant-first fixture |
| 113 | V | Queued deadlock victim claims until canonical removal | §§11.13.4, 14.12 | Victim-removal fixture |
| 114 | V | Granted deadlock victim retains claim through A3 | §§11.11, 11.13.4 | Paused-abort fixture |
| 115 | V | Commit holder retains through C4 and releases C5 | §§11.11, 14.12 | C4/C5 reuse probes |
| 116 | V | Abort holder retains through A2 and releases A3 | §§11.11, 14.12 | A2/A3 reuse probes |
| 117 | W INDEX RID CLEANUP | Every required exact index entry is absent before DEAD | §14.9 | Multi-index cleanup fixture |
| 118 | W | Exact erase is idempotent across crash/retry | §14.9 | Prefix crash matrix |
| 119 | W | One remaining index blocks DEAD/reuse | §§14.5, 14.9 | One-defect fixture |
| 120 | W | Unique residual cannot alias new tuple | §§8.23, 14.9 | Unique-index alias fixture |
| 121 | W | B+ cleanup occurs without heap latch | §14.9 | Latch event assertion |
| 122 | X PREV_RID CLEANUP | Surviving successor cannot point to reusable RID | §14.10 | Decoded-link oracle |
| 123 | X | Splice rewrites direct successor to predecessor | §14.10 | WAL/page-byte fixture |
| 124 | X | Unproven or failed splice defers reuse | §§14.10–14.12 | Failure/state-change fixture |
| 125 | X | Consecutive removable versions splice safely | §14.10 | Multi-version chain fixture |
| 126 | Y DEAD-TO-UNUSED / REUSE | NORMAL cannot bypass DEAD to UNUSED | §§5.4, 14.5 | Invalid-transition fixture |
| 127 | Y | DEAD persistence precedes retirement/reuse | §§14.5, 14.8 | Crash-prefix fixture |
| 128 | Y | Payload coordinates are zero before UNUSED | §§5.4.3, 14.12 | Independent page-byte oracle |
| 129 | Y | UNUSED publishes exactly one free-list link | §§5.3.2, 14.12 | Free-list graph oracle |
| 130 | Y | Epoch grace is independently required | §§14.6.3, 14.12 | One-missing-barrier matrix |
| 131 | Y | Zero lock claims is independently required | §14.12 | One-missing-barrier matrix |
| 132 | Y | Index and predecessor absence remain required | §§14.9–14.12 | One-missing-barrier matrix |
| 133 | Y | Positive same-slot allocation requires all barriers | §14.12 | Complete conjunction fixture |
| 134 | Y | Whole-page recycling cannot bypass per-RID proof | §§14.5, 14.18 | Whole-page fixture |
| 135 | Z CRASH / RESTART | Pre-crash epochs are not replayed | §14.6.4 | Recovery runtime-state snapshot |
| 136 | Z | Pre-crash guards/write dependencies/locks are not replayed | §§14.6.4, 14.17.1 | Recovery reset fixture |
| 137 | Z | Persistent DEAD remains DEAD after reopen | §14.8 | DEAD crash fixture |
| 138 | Z | Recovered DEAD receives fresh retirement before reuse | §§14.6.4, 14.8 | Re-enqueue event assertion |
| 139 | Z | Legal UNUSED remains canonical after reopen | §§5.4, 14.12 | UNUSED crash fixture |
| 140 | Z | Post-reuse recovery never resurrects old identity | §§14.6.4, 14.12 | New-allocation crash fixture |
| 141 | AA BUFFERPOOL / FRAME | Reclaimer does not invalidate pinned status frame | §§7.9, 14.14.2 | Pin/drain fixture |
| 142 | AA | Retired frame is non-writeback before sparse reclaim | §14.14.2 | Delayed-writeback race |
| 143 | AA | Heap final transition revalidates under page latch | §§14.11–14.12 | State-change barrier fixture |
| 144 | AA | Pin/latch never substitute for RID epoch | §§7.7–7.9, 14.7 | Protection matrix fixtures |
| 145 | AB FSM / FREE SPACE | Heap state remains free-space authority | §§6.10–6.12, 14.16 | Stale-high/low fixtures |
| 146 | AB | FSM update may be batched/stale | §14.16 | Delayed-update fixture |
| 147 | AB | FSM update failure does not roll back heap reclaim | §§6.10, 14.16 | Failure fixture |
| 148 | AB | Compaction preserves SlotIds and is not reuse | §§5.5, 14.12 | Compaction/RID fixture |
| 149 | AC SHUTDOWN / LIFETIME | Maintenance is admitted only in READY | §14.17.1 | Lifecycle admission matrix |
| 150 | AC | DRAINING rejects new work and quiesces active units | §14.17.1 | Drain event sequence |
| 151 | AC | Active transactions retain dependencies through C5/A3 during drain | §14.17.1 | Drain/terminal fixture |
| 152 | AC | NONCONTINUABLE admits no new maintenance mutation | §14.17.1 | Lifecycle failure fixture |
| 153 | AC | Scheduling policy never weakens safety predicates | §§14.16–14.17 | Trigger-equivalence fixture |
| 154 | AD FAILURE / EXHAUSTION | Vacuum workspace exhaustion publishes no unsafe partial state | §§14.11, 39.1 | Resource fault fixture |
| 155 | AD | Claim-registration exhaustion retains epoch | §§11.3, 14.6 | Handoff resource fault |
| 156 | AD | Status-history pressure cannot force cutoff | §§14.14, 14.17 | Pressure/defer fixture |
| 157 | AD | Sparse-storage pressure cannot authorize renumbering | §14.14.2 | Full-storage fixture |
| 158 | AD | Corrupt tuple/page authorizes no maintenance mutation | §§14.11, 39.1 | Single-defect corruption fixture |
| 159 | AD | Pre-authorization failure leaves old canonical state | §§12.12, 14.13 | WAL fault fixture |
| 160 | AD | Post-authorization failure preserves authoritative progress | §§12.12, 14.17.1 | WAL fault fixture |
| 161 | AE OTHER / SEPARATION | Status retirement and RID reuse are independent axes | §§14.7, 14.14 | Two-axis matrix |
| 162 | AE | No new old TxnId can be introduced below cutoff | §§9.4, 14.14 | Post-cutoff write-admission fixture |
| 163 | AE | Maintenance scheduling correctness is trigger-independent | §§14.16–14.17 | Trigger-equivalence fixture |
| 164 | AE | Verification uses semantic events, not implementation containers | §§14.6, 14.17.1 | Harness implementation-freedom assertions |


---

## Group Commit Benchmarks

Benchmark:

```text
1 committing thread
2
4
8
16
32+
```

Measure:

```text
transactions/sec
p50/p95/p99 commit latency
fdatasync calls/sec
average commits per fsync
WAL bytes/sec
```

Compare against a diagnostic mode that forces one fsync per commit to quantify group-commit benefit.

Apply the shared Chapter-42 benchmark evidence policy. Replay a fixed-seed, disjoint-key
persistent-write transaction stream with declared operation/row shape, transactions per
participant, expected commit count, worker count, start coordination, and measured window.
Exclude read-only commits, which elide the durable terminal-WAL path (§15.5). The
one-committer baseline admits exactly one committing participant and verifies ordinary
synchronous C3 durability and C6 success acknowledgement. For multi-committer fixtures,
release participants through a controlled barrier and observe overlapping commit requests;
configured thread count or elapsed time alone does not prove concurrent demand.

Retain the thread-count grid above as Verification fixture choices, not Architecture
constants. At least one multi-committer sample must show an actual shared durability
advancement covering multiple distinct commit waiters; the contention regime must show
concurrent queue/overlap or a CommitCoordinator contention observation. If every commit
has its own synchronization, batching was not activated; if attempts serialize, the
contention regime was not proved. Record owner group-size/coverage and wait observations
rather than inferring them from
throughput. When available, record queue, WAL-write, and synchronization time separately
as diagnostic attribution, not substitutes for commit latency.

Count transactions/sec from successfully C6-acknowledged benchmark commits over the
declared interval; preserve the distinct §40.3 semantic COMMITTED count at durable C3.
Measure each commit latency from the declared commit request to its successful C6
acknowledgement, and derive percentiles only from valid completed samples. Count actual
`fdatasync` calls/sec from WAL durability owner observations, distinguishing calls from
§40.3 WAL-flush attempts and from multi-segment synchronization. Derive commits per sync
from distinct commit targets covered by successful durability advancements and the actual
successful `fdatasync`-call denominator, including multi-segment calls. Source WAL bytes/sec
from canonical valid-prefix append bytes, not payload size. Scope numerator and elapsed
interval together. Abort, WAL/storage failure, C4–C6 failure, uncertain client
outcome, or group-flush failure retains its owner classification and is not a successful
latency/throughput sample.

The forced-one-fsync comparison is a test-only diagnostic batching control, not a
production durability mode or correctness oracle. It retains synchronous C3–C6 semantics;
record the intentional mode change while holding workload, data, and material environment
comparable. No fixed speedup is required.

---

## Checkpoint/Recovery Benchmarks

Measure:

```text
checkpoint duration
checkpoint WAL bytes
full-page-image bytes
dirty-page-table size
WAL retained bytes
restart analysis time
redo time
pages redone
recovery total time
```

Test both:

- mostly clean buffer pool,
- heavily dirty buffer pool.

Apply the shared Chapter-42 benchmark evidence policy. Build replayable page/database
populations with declared BufferPool capacity, seed, durable WAL/control state, and an
independently known page set before checkpoint. For the mostly-clean fixture dirty a
predeclared small subset of the resident population; for the heavily-dirty fixture dirty
a materially larger declared subset under otherwise comparable population and settings.
Observe canonical dirty-frame/DPT state immediately before checkpoint and the captured
DPT at §13.5's synchronized capture boundary. Workload labels or update counts alone do
not prove either regime. Confirm CHECKPOINT_BEGIN/DATA/END and control publication actually
complete; measure checkpoint duration from its start through §13.5 step 8, not an earlier
API return.

Source checkpoint WAL bytes from valid-prefix WAL append records attributable to the
declared checkpoint interval; classify FPI count/bytes from actual canonical WAL record
types in the declared interval rather than multiplying dirty pages by page size. Record
captured DPT entry count from the checkpoint owner, not an arbitrary BufferPool dirty-frame
snapshot. Report retained WAL bytes from the installed retention floor and valid durable
end with segment/LSN accounting (§13.10), not file count alone; distinguish logical
retained bytes from physically allocated segment bytes when both are reported.

For recovery, construct a deterministic crash/reopen state with recorded selected control
generation/checkpoint, exact valid persisted WAL prefix and acknowledged durability
frontier, data and status-page images, transaction outcomes, and expected redo target/work.
Reuse the recovery correctness
oracles; a timing sample is valid only after the canonical §13.19 `RECOVERING -> READY`
gate and expected committed/aborted contents pass. Time analysis and redo from their
respective §13.12/§13.13 owner phase-entry and completion events; time total restart
recovery from declared recovery processing entry through READY, excluding fixture
construction. Count pages actually redone from recovery-owner application events, not
DPT size or record count. Include a positive-control crash state with nonzero redo work
and owner-observed pages redone; an already-clean restart is only a separate baseline.
For both clean/dirty classes record actual DPT, WAL, and redo observations rather than
assuming every dirty page must be redone.

---

## Vacuum Benchmarks

Measure:

```text
dead-version scan rate
index cleanup rate
heap bytes reclaimed
B+ tree merge/split side effects
RID grace-period delay
FSM improvement
status-freezing rate
foreground latency impact
```

Use workloads with:

- update-heavy hot rows,
- delete-heavy tables,
- aborted transactions,
- duplicate secondary keys,
- long-running snapshots.

Apply the shared Chapter-42 benchmark evidence policy. Replay a declared row/version and
index population with fixed generator/seed, known transaction outcomes and physical RIDs,
registered SQL snapshots and read epochs, baseline heap free space/FSM state, B+ state,
and freeze-eligible status references. Reuse the existing Vacuum and Reclamation Tests,
exact B+ cleanup model, and §14 owner predicates for correctness; this benchmark adds
measured work, activation, and metric provenance rather than a second garbage oracle.

- In the update-heavy fixture, repeatedly update a known hot-row set and record created
  old versions, separating globally garbage-eligible versions from those still visible
  at vacuum start. The delete-heavy fixture uses known committed deletions and expected
  stale secondary-index entries. The aborted fixture identifies aborted-created versions
  separately from aborted deleter normalization, proving the actual abort outcome and
  applicable cleanup eligibility. The duplicate-key fixture gives distinct RIDs to a
  declared multiplicity of equal secondary user keys and checks exact `(key,RID)` removal
  while live duplicates remain. The long-running-snapshot fixture holds a registered SQL
  snapshot across a vacuum run, proves its horizon blocks the intended versions, then
  releases it for a follow-up run. No sleep or statement label is an eligibility oracle.
- Report dead-version scan rate with declared versions/pages examined and elapsed vacuum
  interval from canonical vacuum scan events; do not substitute reclaimed count for scan
  work. Report exact index-cleanup rate from owner-confirmed *new* physical
  `(index,user_key,RID)` removals over that interval, checked against independent fixture
  truth. Idempotent `EraseIfPresent` on an already-absent entry counts no new removal.
  Include a positive control with expected exact cleanup > 0. Wrong broad deletion or
  retained stale entries invalidates the sample.
- Report heap bytes made reusable from canonical heap-page before/after free-space and
  payload state, with a positive-control fixture that reclaims > 0 bytes. File length
  need not shrink. Keep byte reclamation separate from `DEAD -> UNUSED` slot/RID reuse:
  compaction can free payload space while grace or another reuse barrier still holds.
- Attribute B+ redistribution, merge, root contraction, or applicable split to canonical
  structural events during vacuum's exact cleanup, not to entry-removal count. Include a
  positive-control fixture whose legal cleanup triggers a measured structural event;
  ordinary valid cleanup with owner-observed zero events may still report zero. Do not
  change the B+ algorithm to manufacture the control.
- Measure RID grace from the §14.6 RID-retirement event following persisted `DEAD` to
  satisfaction of the read-epoch predicate; record both boundary events. Hold a
  registered pre-retirement read-epoch guard; prove it blocks reuse, then release it and
  observe the canonical grace transition and any later `DEAD -> UNUSED` publication
  separately. State the measured delay/work unit and identify any additional predecessor
  or live `TUPLE_WRITE` holder/waiter barrier; an
  epoch-only fixture does not measure lock-claim delay. Wall-clock waiting alone proves
  neither registration nor blocked reuse.
- Report FSM improvement from canonical before/after advisory FSM estimates alongside
  actual heap free-space truth, without requiring exact equality. Count completed
  §14.13 freezing rewrites from owner events against a declared examined/eligible work
  denominator; include a fixture with eligible versions and observed freeze count > 0.
  Do not equate freezing with status-cutoff advancement or sparse status-page reclamation.
- Compare foreground latency with and without vacuum using the same declared operation,
  data, correctness result, and material settings. In the with-vacuum case coordinate and
  observe actual overlap of foreground work and vacuum activity in the measured window;
  starting two threads is insufficient. Record impact without a fixed slowdown target.

Preserve owner WAL, corruption, resource, and unresolved-blocker outcomes as failures or
incomplete evidence under the shared policy, not zero rates or successful samples.
