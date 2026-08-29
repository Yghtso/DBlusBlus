# Chapter 12 — Frozen Architecture Semantic Review Required

Primary scope: [docs/ARCHITECTURE.md, Chapter 12](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:8803), through the line before Chapter 13.

Finding counts:

| Severity | Count |
|---|---:|
| BLOCKING | 0 |
| MAJOR | 1 |
| MINOR | 3 |
| EDITORIAL | 0 |

The WAL architecture is otherwise technically strong and internally coherent. One persistent-format question remains unresolved: whether zero-count `PAGE_DELTA` and `BTREE_MTR` forms are legal. Because different answers produce incompatible recovery decoders, this requires a frozen architecture decision.

## Scope read

Primary:

- Chapter 12, §§12.1–12.18, completely.

Context consulted:

- Chapter 3 lifecycle, READY/DRAINING/NONCONTINUABLE.
- Chapter 4 LSN domains, page framing, namespace durability, extension, checksum, and exhaustion.
- Chapter 5 heap WAL and PAGE_INIT.
- Chapter 6 FSM WAL.
- Chapter 7 BufferPool, raw-WAL I/O, copied writeback, and WAL-before-data.
- Chapter 8 BTREE_MTR and structural publication.
- Chapter 9 terminal transaction protocol.
- Chapter 10 recovered transaction authority.
- Chapter 11 terminal lock release.
- Chapter 13 WAL inventory, tail validation, analysis, redo, loser handling, and recycling.
- Chapter 14 retention/status retirement where referenced.
- Chapter 15 DML publication and COMMIT/ABORT integration.
- §39 failure consequences.
- §41 verification obligations.

Other live documents consulted:

- [docs/VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md:2962)
- [docs/DEVELOPMENT.md](/home/yghtso/Projects/DBlusBlus/docs/DEVELOPMENT.md:801)
- [docs/PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md:291)

No source audit was performed.

## Actual Chapter 12 heading inventory

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 12 | Write-Ahead Logging and Commit Durability | WAL and durability contract | Architecture-appropriate |
| 12.1 | Scope and durability model | STEAL/NO-FORCE and ownership boundary | Architecture-appropriate |
| 12.2 | Logical WAL stream and physical segments | Logical stream, segment size, LSN mapping | Architecture-appropriate |
| 12.2.1 | WAL segment namespace and creation | Naming and namespace durability | Architecture-appropriate |
| 12.3 | Record alignment, physical span, and segment boundary | Framing, padding, no-crossing rule | Architecture-appropriate |
| 12.4 | WAL record header v1 | Exact 48-byte header and CRC | Architecture-appropriate |
| 12.5 | WAL PageId codec | Exact persistent PageId codec | Architecture-appropriate |
| 12.6 | Per-transaction WAL chain | `prev_txn_lsn` chain | Architecture-appropriate |
| 12.7 | Persisted WAL record-type registry | Complete v1 record registry | Architecture-appropriate |
| 12.7.1 | WAL_PAD | Segment-tail padding record | Architecture-appropriate |
| 12.7.2 | TXN_COMMIT and TXN_ABORT | Terminal records | Architecture-appropriate |
| 12.7.3 | Page-record transaction ownership | User/system record ownership | Architecture-appropriate |
| 12.8 | PAGE_DELTA | Delta payload and redo | Architecture with format issue |
| 12.9 | PAGE_INIT and PAGE_IMAGE | Canonical complete images | Architecture-appropriate |
| 12.10 | Full-page-image invariant and torn-page protection | FPI policy | Architecture-appropriate |
| 12.10.1 | BufferPool recLSN consequence | Dirty-interval recovery base | Architecture-appropriate |
| 12.10.2 | B+ tree mini-transactions | BTREE_MTR format and semantics | Architecture with format issue |
| 12.10.3 | MTR no-flush barrier | Structural atomic publication | Architecture-appropriate |
| 12.10.4 | User abort does not reverse B+ shape | System-MTR/user-abort boundary | Architecture-appropriate |
| 12.10.5 | TXN_STATUS full-image and terminal mutation protocol | Status reconstruction protocol | Architecture-appropriate |
| 12.10.5.1 | Canonical mutation order | Image/terminal ordering | Architecture-appropriate |
| 12.10.5.2 | Frame metadata, repeat updates, and durability | `rec_lsn`, `page_lsn`, dirty state | Architecture-appropriate |
| 12.10.5.3 | Checkpoint, redo, and retention consequences | Recovery-base retention | Architecture-appropriate |
| 12.10.5.4 | Canonical crash outcomes | Status crash-prefix outcomes | Architecture-appropriate |
| 12.11 | Heap redo before index MTR | Cross-record redo ordering | Architecture-appropriate |
| 12.12 | WAL append atomicity and runtime page publication | Reservation/authorization/publication | Architecture-appropriate |
| 12.12.1 | LSN reservation and no-hole rule | Candidate reservation | Architecture-appropriate |
| 12.12.2 | Atomic append success and physical-tail failures | Valid append boundary | Architecture-appropriate |
| 12.12.3 | Canonical provisional-mutation and publication protocol | Page publication sequence | Architecture-appropriate |
| 12.12.4 | Failure classes, retry, rollback, and escalation | Known/uncertain failure policy | Architecture-appropriate |
| 12.12.5 | WAL append buffer | Runtime realization | Architecture with role issue |
| 12.13 | WAL writer and durable LSN | Durable-prefix service | Architecture with role issue |
| 12.14 | Group commit and CommitCoordinator | Coalesced durability | Architecture with local LSN error |
| 12.15 | Synchronous commit | v1 commit durability | Architecture-appropriate |
| 12.16 | BufferPool recLSN | Dirty-page-table recovery floor | Architecture-appropriate |
| 12.17 | WAL-before-data and temporary no-flush state | Writeback gate | Architecture-appropriate |
| 12.18 | WAL and commit invariants | Canonical summary | Architecture-appropriate, inherits F12-1 |

## Section-by-section review

Legend: `OK` = clear/sufficient; `—` = not owned locally; `F1–F4` = finding.

| Section | Role | Time | Owner | Depth | Terms | Format | LSN | Append | Durability | Commit/abort | MTR | PAGE_INIT | WBD | Failure | Exhaustion | Recovery | Xref | Semantics | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 12.1 | Scope | OK | OK | OK | OK | — | — | — | OK | OK | — | — | OK | OK | — | OK | OK | OK | CLEAN |
| 12.2 | Stream/segments | OK | OK | OK | OK | OK | OK | — | — | — | — | — | — | — | OK | OK | OK | OK | CLEAN |
| 12.2.1 | Namespace | OK | OK | OK | OK | OK | OK | OK | OK | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.3 | Framing | OK | OK | OK | OK | OK | OK | — | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.4 | Header | OK | OK | OK | OK | OK | OK | — | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.5 | PageId codec | OK | OK | OK | OK | OK | — | — | — | — | — | OK | — | OK | — | OK | OK | OK | CLEAN |
| 12.6 | Txn chain | OK | OK | OK | OK | OK | OK | OK | — | OK | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.7 | Registry | OK | OK | OK | OK | OK | — | — | — | OK | OK | OK | — | OK | — | OK | OK | OK | CLEAN |
| 12.7.1 | Padding | OK | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.7.2 | Terminals | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.7.3 | Ownership | OK | OK | OK | OK | OK | — | OK | — | OK | OK | OK | — | OK | — | OK | OK | OK | CLEAN |
| 12.8 | PAGE_DELTA | OK | OK | OK | OK | F1 | OK | OK | — | — | — | — | OK | F1 | OK | OK | OK | F1 | FINDING |
| 12.9 | Images | OK | OK | OK | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10 | FPI | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.1 | recLSN | OK | OK | OK | OK | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.2 | BTREE_MTR | OK | OK | OK | OK | F1 | OK | OK | — | — | F1 | OK | OK | F1 | OK | F1 | OK | F1 | FINDING |
| 12.10.3 | No-flush | OK | OK | OK | OK | OK | OK | OK | — | — | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.4 | User abort/MTR | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | — | OK | — | OK | OK | OK | CLEAN |
| 12.10.5 | Status protocol | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.5.1 | Mutation order | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.5.2 | Frame/durability | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.5.3 | Retention | OK | OK | OK | OK | OK | OK | — | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.10.5.4 | Crash outcomes | OK | OK | OK | OK | OK | OK | — | OK | OK | — | OK | OK | OK | — | OK | OK | OK | CLEAN |
| 12.11 | Redo order | OK | OK | OK | OK | — | OK | — | — | — | OK | — | — | OK | — | OK | OK | OK | CLEAN |
| 12.12 | Append/publication | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.12.1 | Reservation | OK | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.12.2 | Valid append | OK | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.12.3 | Publication | OK | OK | OK | OK | OK | OK | OK | — | — | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.12.4 | Failure | OK | OK | OK | OK | OK | OK | OK | — | — | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.12.5 | Buffer | F2 | F2 | OK | OK | OK | — | OK | — | — | — | — | — | OK | — | — | OK | OK | FINDING |
| 12.13 | Durable LSN | F3 | F3 | OK | OK | — | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | OK | FINDING |
| 12.14 | Group commit | OK | OK | OK | OK | — | F4 | OK | OK | OK | — | — | — | OK | — | — | OK | F4 | FINDING |
| 12.15 | Sync commit | OK | OK | OK | OK | — | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 12.16 | recLSN | OK | OK | OK | OK | — | OK | — | OK | — | — | — | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.17 | WBD | OK | OK | OK | OK | — | OK | OK | OK | — | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 12.18 | Invariants | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | Note F1 | CLEAN WITH NOTE |

## Ownership and subsystem boundaries

| Mechanism | Canonical owner | Protects/defines | Lifetime | Persistent? | May block? |
|---|---|---|---|---:|---:|
| WAL namespace and raw segment I/O | WalManager, Chapter 12 | WAL byte stream and durable prefix | Database open lifetime | Yes | Yes |
| BufferPool page latch/no-flush | Chapter 7 | Resident physical bytes and writeback exclusion | Short/provisional operation | No | Latches may block; no logical wait while forbidden |
| WAL append reservation | Chapter 12 | One candidate nonoverlapping logical position | Until append/cancel | No | May serialize |
| Valid append end | Chapter 12 | Contiguous recovery-visible logical record stream | Process lifetime/reconstructed on open | WAL bytes may persist | No holes |
| `durable_lsn` | Chapter 12 | Proven durable WAL prefix | Runtime, reconstructed | Derived from persistent WAL | Flush waiters block |
| SQL transaction state | Chapter 9 | ACTIVE/COMMITTING/terminal lifecycle | Transaction | Terminal evidence persists | Yes |
| Transaction locks | Chapter 11 | Logical write/key conflict ordering | Through C4/A2 then C5/A3 release | No | Yes |
| Recovery analysis/redo | Chapter 13 | Reconstructs state from valid WAL prefix | Open/recovery | Consumes persistent WAL | Ordinary traffic blocked |
| Checkpoint retention/recycling | Chapters 13–14 | Recovery floor and removable WAL | Database lifetime | Yes | Maintenance may block |

The raw-WAL exception is precise: WAL segments bypass BufferPool and ordinary logical page-file APIs, while ordinary database pages remain BufferPool-owned.

## WAL persistent format

### Segment/file format

| Property | Contract |
|---|---|
| Location | `database_root/wal/` |
| Name | Exactly 16 lowercase hexadecimal segment-index digits plus `.wal` |
| Segment size | 67,108,864 bytes (`2^26`, 64 MiB) |
| Segment header | None |
| Segment 0 prefix | Bytes `0..7` zero; first record LSN is 8 |
| Fresh contents | All zero |
| Record crossing | Forbidden |
| Creation | Exact next contiguous final name; exclusive/no-follow creation; `ftruncate`; segment `fdatasync`; `fsync(wal/)` |
| Recycling | Chapter 13 retention/recycling owner |
| BufferPool use | Forbidden; specialized raw positional I/O |

“The initial segment size” is persistent-format/v1 language, not current-project chronology.

### Record header

| Offset | Width | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | `total_length` | `48 + payload_length` |
| 4 | 2 | `header_length` | Exactly 48 |
| 6 | 2 | `record_type` | Registry code |
| 8 | 4 | `flags` | Zero |
| 12 | 4 | `reserved` | Zero |
| 16 | 8 | `lsn` | Record-start LSN |
| 24 | 8 | `txn_id` | Normal TxnId or zero system owner |
| 32 | 8 | `prev_txn_lsn` | Prior transaction record or zero |
| 40 | 4 | `payload_length` | Exact payload bytes |
| 44 | 4 | `crc32c` | Header with CRC field zeroed plus payload |

All fields are little-endian. External alignment padding is zero and excluded from `total_length` and CRC.

### LSN/domain table

| Quantity | Width/domain | Sentinel | First valid | Meaning | Maximum | Exhaustion |
|---|---|---|---:|---|---:|---|
| Record-start LSN | Mathematical uint64 byte address | 0 invalid | 8 | Start of ordinary WAL record | `2^64-48` for a minimum record | `WAL_POSITION_EXHAUSTED` |
| Exclusive end | Mathematical `[8,2^64]` | N/A | 8 | One-past complete physical span | `2^64` | Never narrowed to uint64 at terminal end |
| Segment index | Derived integer | None | 0 | `LSN / 2^26` | `2^38-1` | No wrap/reuse |
| Segment offset | `0..2^26-1` | None | 0/8 | `LSN % 2^26` | `2^26-1` | Record must fit segment |
| `page_lsn` | uint64 record start | 0 invalid | 8 | Authorizing page record/MTR/terminal record start | Last legal applicable record | Same WAL exhaustion |
| `rec_lsn` | uint64 record start | 0 invalid | 8 | Recovery-base full-image start | Same | Same |
| `durable_lsn` | uint64 record start | 0/initial invalid state | First durable record | Latest record whose complete bytes and prior prefix are durable | Last legal record start | Same |

There is no persisted end-LSN field. Completion is known from record length and physical span.

### Record registry

| Code | Record | Purpose | Txn-associated? | Page-associated? | Atomic scope | `page_lsn` effect |
|---:|---|---|---:|---:|---|---|
| 0 | WAL_PAD | Consume segment remainder | No | No | One padding record | None |
| 1 | PAGE_INIT | Initialize new page | User or system | Yes | One complete page image | Record LSN |
| 2 | PAGE_DELTA | Existing-page after-image patches | User or system | Yes | One page delta | Record LSN |
| 3 | PAGE_IMAGE | Existing-page complete image | User or system | Yes | One complete page image | Record LSN |
| 4 | BTREE_MTR | Atomic B+ structural change | System; diagnostic owner payload | Multiple pages | Complete MTR | Common MTR LSN |
| 5 | TXN_COMMIT | Terminal committed evidence | Yes | Status semantically | Transaction terminal record | Status page advances to terminal LSN |
| 6 | TXN_ABORT | Terminal aborted evidence | Yes/recovery-generated | Status semantically | Transaction terminal record | Status page advances to terminal LSN |
| 7 | CHECKPOINT_BEGIN | Checkpoint start | No | No | Chapter 13 checkpoint | None |
| 8 | CHECKPOINT_DATA | Checkpoint payload | No | No | Chapter 13 checkpoint | None |
| 9 | CHECKPOINT_END | Checkpoint completion | No | No | Chapter 13 checkpoint | None |

Unknown complete CRC-valid record type is `UNSUPPORTED_WAL_FORMAT`; malformed required records are corruption; incomplete final records are torn tail according to Chapter 13.

## Append, publication, and durability

### Append-state table

| State | Bytes known? | Valid record? | Authorized? | Durable? | Rollback allowed? | Recovery may replay? | Failure consequence |
|---|---:|---:|---:|---:|---:|---:|---|
| Before reservation | No | No | No | No | Yes | No | Ordinary failure |
| Reserved candidate | Private construction only | No | No | No | Yes | No | Cancel without consuming LSN |
| Complete private record | Yes | Not in valid stream | No | No | Yes | No | Exact rollback |
| Valid append end advanced | Yes, retained | Yes | Yes | No | No ordinary rollback | If bytes survive crash | Complete publication or NONCONTINUABLE |
| Physical short write | Exact memory copy retained | Logical record remains valid | Yes | No | No | Only complete persisted prefix | Retry physical write/sync |
| `fdatasync` failure | Exact append retained | Yes | Yes | Not proven | No | Depends on surviving prefix | Retain state; retry/escalate |
| Durable prefix covers record | Yes | Yes | Yes | Yes | No | Yes | Authoritative |
| Append-end/assigned-byte uncertainty | Not safely knowable | Unknown | Unknown | Unknown | No guessing | Recovery decides | DATABASE_NONCONTINUABLE |

Reservation does not consume an LSN, alter page metadata, or advance transaction `last_wal_lsn`. The publication-authorizing append is the boundary after which rollback-and-continue is forbidden.

### Durable prefix and flush

| Operation | Required result |
|---|---|
| Request durability for record LSN `X` | Return only when `durable_lsn >= X` or exact failure |
| Meaning of `durable_lsn >= X` | Complete record at X and every required preceding WAL byte are durable |
| Newly created segment | Directory entry must already be synchronized |
| Cross-segment request | Every segment containing previously unsynchronized prefix bytes must be synchronized |
| Concurrent requests | May coalesce; each caller’s own target remains the completion condition |
| Append/flush race | Flush target can cover only a complete valid append |
| Fatal WAL service failure | Waiters must be released with failure; no indefinite sleep |
| Data-page writeback | Allowed only after durable WAL covers stable image `page_lsn` |

### Durability distinctions

| Object/event | WAL required? | Data-page force required? | Namespace sync required? | Client-visible consequence |
|---|---:|---:|---:|---|
| Volatile page publication | Valid authorizing append | No | Segment may already exist | Not durable |
| Data-page durability | Yes, through `page_lsn` | Yes | Owning data-file rules apply | Physical page durable |
| COMMIT C3 | Yes, through `commit_lsn` | No | Yes if commit lies in new segment | Transaction durably committed |
| Ordinary ABORT A2 | Append/status publication as required | No synchronous WAL flush | Eventually before status page writeback | Runtime ABORTED |
| PAGE_INIT | Append before volatile publication; durable before page write | Yes for durable page | File/publication namespace rules | New page becomes recoverable |
| BTREE_MTR | Complete MTR append | No immediate page force | As applicable | Atomic volatile publication |
| Checkpoint installation | Chapter 13 checkpoint WAL/control protocol | Required checkpoint page set | Control/namespace owner | Establishes recovery floor |
| WAL segment creation | N/A for creation itself | Segment `fdatasync` | `fsync(wal/)` | Records may then satisfy durability |

WAL durability, transaction durability, data-page durability, and namespace durability are correctly separated.

## COMMIT and ABORT

| Step | WAL state | Runtime state | Status page | Locks | Client |
|---|---|---|---|---|---|
| C0–C1 | No terminal authority yet | ACTIVE → COMMITTING | Pre-terminal | Held | No success |
| C2 | Required image and `TXN_COMMIT` validly appended; status update installed | COMMITTING | `page_lsn=commit_lsn`, dirty/no-force | Held | No success |
| C3 | `durable_lsn >= commit_lsn` | COMMITTING | Need not be forced | Held | No success yet |
| C4 | Durable outcome published runtime | COMMITTED | Cache/registry terminal | Still protected through publication | No success yet |
| C5 | Resource/cache cleanup | COMMITTED | Unchanged | Released | Ready for acknowledgment |
| C6 | No new durability event | COMMITTED | Unchanged | Released | Success delivered or transport uncertainty |
| A0–A1 | Abort record/status operation where required | ABORTING | ABORTED installed | Held | No success |
| A2 | Runtime ABORTED publication | ABORTED | Need not be synchronously forced | Held through publication | No final response yet |
| A3 | Cleanup | ABORTED | Unchanged | Released | Ready |
| A4 | No new durability requirement | ABORTED | Unchanged | Released | Abort result delivered |

A read-only transaction may omit terminal WAL/status mutation. V1 does not perform ordinary user-DML physical undo or CLRs.

C3 is unambiguous and consistent with Chapter 9: a durable `TXN_COMMIT` is irreversible. Data-page or status-page force is not a prerequisite. WAL durability alone is not client acknowledgment.

## MTR and PAGE_INIT

### MTR table

| Family | Membership | WAL representation | Shared LSN? | Authorization | Redo atomicity | Pre-authorization failure | Post-authorization failure |
|---|---|---|---:|---|---|---|---|
| Ordinary PAGE_DELTA | One page | One record | N/A | Valid append | One page delta | Exact rollback | Publish/complete or noncontinuable |
| PAGE_INIT/PAGE_IMAGE | One page image | One record | N/A | Valid append | Complete image | Exact rollback/private-tail cleanup | Publish/complete or noncontinuable |
| BTREE_MTR | All affected B+ pages and metadata/free pages | One system record | Yes | Complete valid MTR append | Old or complete new state | Restore every page/metadata item | Complete publication or noncontinuable |
| TXN_STATUS terminal protocol | Optional system image plus terminal record | Ordered multi-record protocol | Distinct `F < T` | Terminal record is final authorization | Image base plus semantic terminal redo | Exact old status/frame state; inert image may remain | Terminal publication must complete |
| Checkpoint | BEGIN/DATA/END | Multiple records plus Chapter 13 installation | Owner-specific | Chapter 13 | Installed checkpoint or ignored incomplete attempt | Leaves prior checkpoint authoritative | Chapter 13 failure rules |

MTR membership, common LSN assignment, no-flush barrier, and old-or-new atomic recovery are clear.

### PAGE_INIT phases

| Phase | Physical file | WAL | Resident/public state | Published bound | Fetch allowed? | Crash result |
|---|---|---|---|---|---:|---|
| Private extension | Tail may exist | None | Private/uninitialized | Old bound | No | Tail removed/reconciled |
| Provisional initialization | Canonical bytes prepared | Reserved/private | No-flush, private | Old bound | No | Old state |
| Valid PAGE_INIT append | Tail exists | Authorized, possibly undurable | Publication must complete | Updated only by owning publication | Not until publication | Redo may reconstruct if record survives |
| Runtime publication | Page/frame canonical | Valid append | Public dirty page with record LSN | New bound | Yes | WAL determines recovery |
| Durable page write | Page image on disk | WAL already durable | Stable image | Published | Yes | Page or WAL reconstruction |

No ghost/uninitialized public page path was found.

## WAL-before-data

Exact rule:

```text
before any dirty stable image with page_lsn = L is written:
    durable_lsn >= L
```

Because `durable_lsn >= L` means the complete record at start L and all preceding bytes are durable, this also covers record payload, CRC, preparatory image records, and earlier segment bytes.

| Stage | Requirement |
|---|---|
| Capture writeback image | Stable copied generation and trusted resident metadata |
| Read `page_lsn=L` | Resident state already validated; on-disk LSN is never trusted before checksum |
| WAL gate | `durable_lsn >= L` |
| Data `pwrite` | Only after WAL gate |
| Page `fdatasync` | Required before durable clean publication |
| Reconcile | Clear dirty only if no newer generation raced |
| Newer generation | Remains dirty with its own later LSN |
| Flush failure | Page remains dirty; committed transaction is not reversed |

The no-flush barrier is stronger than WAL-before-data: provisional bytes cannot enter writeback at all.

## Failure and uncertainty matrix

| Failure class | WAL fact | Rollback? | Transaction/database result | Recovery responsibility |
|---|---|---:|---|---|
| Known pre-authorization failure | No authorizing record | Yes, exact restoration | Lower-layer error; §39 classifies FA/MA | None for failed primitive |
| Known logical append failure | Valid end unchanged | Yes | Structured failure | None |
| Valid append, undurable | Exact bytes retained | No | Published dirty state; durability may retry | Replay only if record survives |
| WAL `pwrite` failure | Valid append retained | No | Durable prefix unchanged | Persisted complete prefix only |
| WAL `fdatasync` failure | Valid append retained; durability unproven | No | COMMITTING/operation retained; retry/escalate | Surviving prefix decides |
| Append-end/assigned-byte uncertainty | Unknown authority | No guessing | DATABASE_NONCONTINUABLE | Establish valid prefix |
| Post-authorizing publication failure | Authorizing record exists | No old-state continuation | Protected completion or NONCONTINUABLE | Redo if record survives |
| Post-durable later failure | Durable record authoritative | No | COMMITTED cannot reverse | Preserve authoritative outcome |
| Data-page flush failure | WAL may be durable | Keep dirty state | I/O error; transaction outcome unchanged | WAL/page state on reopen |
| Segment creation/data sync failure | Segment not admitted to durable prefix | No record may rely on it | Exact I/O/durability failure | Inventory/reconcile |
| Directory-sync uncertainty | Namespace prerequisite not proven | No durability acknowledgment | Failure/noncontinuable according to ownership certainty | Reconcile segment namespace |

## Exhaustion and arithmetic

| Domain | Maximum | Last legal operation | Next operation | No-wrap property |
|---|---:|---|---|---|
| Record-start LSN | `2^64-48` for 48-byte record | Record may end mathematically at `2^64` | Rejected before reservation | Start/end arithmetic is widened and checked |
| Segment index | `2^38-1` | Final segment may end at `2^64` | Index `2^38` rejected | Never formatted through truncation |
| Record `total_length` | 67,108,864 | One segment-sized record at offset 0 of a nonzero segment | Larger rejected | `WAL_RECORD_TOO_LARGE` |
| Payload length | 67,108,816 | Header + payload = one segment | Larger rejected | Checked before uint32 narrowing |
| Physical span | One segment | `align_up(total,8)` fits remainder | Padding/new segment or reject | No cross-segment record |
| Terminal credit | 33,128 bytes | Two 16,520-byte status images plus two 48-byte terminal records | Ordinary work rejected first | Reserved terminal closure cannot be consumed |
| MTR size | Same WAL-record bound | Complete one-record MTR | Not split | Fails before publication |
| Flush target | Legal record-start LSN | Last legal complete record | No terminal one-past encoding | Monotonic record-start domain |

Numeric exhaustion and disk/resource exhaustion are correctly distinguished.

## Concurrency and ordering

| Scenario | Required ordering | Legal result | Forbidden result |
|---|---|---|---|
| Two appends | Total nonoverlapping valid-end order | Either serialized order | Overlap/hole |
| Append vs flush | Flush covers only complete valid append prefix | Earlier prefix or appended record | Treat private/incomplete record as durable |
| Two flush requests | Monotonic coalescing | One sync satisfies both if target covered | Return before own target |
| COMMIT vs flush | COMMIT waits for its record | Group durability | Acknowledge undurable COMMIT |
| Page writeback vs WAL flush | WAL prerequisite first | Delayed page write | Data durable before WAL |
| Shutdown vs append | Admission closes; authorized work drains | Ordered completion/failure | WAL teardown with active producer |
| Recovery vs append | Recovery establishes valid end before READY | Fresh append after READY | Ordinary append racing scan/redo |
| MTR publication vs writeback | No-flush through complete publication | Old or complete new state | Mixed page subset |

## Cross-chapter consistency

| Owner | Result | Assessment |
|---|---|---|
| Chapter 3 lifecycle | CONSISTENT | WAL remains alive through drain/page flush/checkpoint; uncertainty gates ordinary work |
| Chapter 4 framing/exhaustion | FINDING | Rules agree, but §12.14’s illustrative LSNs violate 8-byte alignment |
| Chapter 5 heap | CONSISTENT | Heap PAGE_INIT/delta/full-image behavior aligns |
| Chapter 6 FSM | CONSISTENT BUT SPECIALIZED | Advisory updates remain independently WAL-backed |
| Chapter 7 BufferPool | CONSISTENT | Raw-WAL exception and WAL-before-data match |
| Chapter 8 B+ tree | CONSISTENT | One structural BTREE_MTR, common LSN, no partial publication |
| Chapter 9 transactions | CONSISTENT | C3 durability and C4/C5 publication/release ordering agree |
| Chapter 10 visibility | CONSISTENT | Recovered terminal WAL/status authority agrees |
| Chapter 11 locks | CONSISTENT | Locks release after runtime terminal publication, not at WAL durability |
| Chapter 13 recovery | FINDING | Recovery handoff is coherent, but zero-count grammar lacks an exact accept/reject oracle |
| Chapter 14 retention | CONSISTENT | Retention/status retirement references are precise |
| Chapter 15 DML | CONSISTENT | Publication and pre/post-write failure boundaries agree |
| §39 failures | CONSISTENT | Known, uncertain, MA/FA, and noncontinuable classes align |
| §41 verification | CONSISTENT BUT SPECIALIZED | High-level obligations exist; several byte/durable-prefix fixtures remain implicit |

## Explicit cross-references

| Source | Targets | Purpose | Status |
|---|---|---|---|
| 12.1 | §§4.7, 12.2.1 | Namespace durability | Precise |
| 12.2 | §4.3.2.4 | LSN exhaustion | Precise |
| 12.2.1 | §§12.3, 4.7.7, 13.10 | Tail bytes and recycling | Precise |
| 12.3 | §§4.3.2.4, 4.14 | Size failure and reserved fields | Precise |
| 12.7 | §13.11 | Malformed/unknown recovery classification | Precise |
| 12.7.2 | §12.10.5 | Status full-image base | Precise |
| 12.7.3 | §12.10.5 | System image ownership | Precise |
| 12.10.2 | §4.14 | Nested registry/reserved fields | Precise |
| 12.10.3 | §§12.12, 12.12.2, 4.11.1.1 | Provisional MTR and rollback | Precise |
| 12.10.4 | §§12.12.4, 4.11.3 | Post-append failure and unpublished tail | Precise |
| 12.10.5 | §12.10 | Status pages and FPI | Precise |
| 12.10.5.1 | §§12.12, 4.3.2.4, 39.1.5–39.1.6 | Append and terminal failure | Precise |
| 12.10.5.2 | §§4.12.2, 7.10, 9.13, 9.14.1, 12.17 | Dirty state, terminal publication, WBD | Precise |
| 12.10.5.3 | §§13.10, 13.12–14.14 | Retention and retirement | Precise |
| 12.12 | §§12.10.5, 13.13.2 | Multi-record publication example | Precise |
| 12.12.1 | §§12.3, 12.2.1, 4.3.2.4, 13.11, 4.7 | Reservation and empty segment | Precise |
| 12.12.2 | §13.11 | Persisted-prefix scan | Precise |
| 12.12.3 | §12.10.5 | Preparatory-record model | Precise |
| 12.12.4 | §§4.11.1.1, 3.3.5, 39.1, 7.4.3 | Restoration, lifecycle, retry | Precise |
| 12.12.5 | §§12.12.1–12.12.4 | Required semantic preservation | Precise |
| 12.13 | §12.2.1 | Namespace proof in durability | Precise |
| 12.15 | §§12.2.1, 15.5, 39.1.5 | Synchronous commit and acknowledgment | Precise |
| 12.16 | §§12.10, 12.10.5 | recLSN/FPI specialization | Precise |
| 12.17 | §§12.12, 4.12.2, 7.10–7.11 | Publication and copied writeback | Precise |

No vague “later recovery chapter” references were found.

## Terminology

| Term | Canonical meaning | Assessment |
|---|---|---|
| Reservation | Private candidate LSN/span; consumes no stream position | Precise |
| Valid append | Complete framed/CRC record installed and valid end atomically advanced | Precise |
| Authorization/publication-authorizing record | Append after which old-state rollback-and-continue is forbidden | Precise |
| Runtime page publication | Resident mutation becomes legal dirty state | Precise |
| Durable | Covered by proven `durable_lsn` and namespace prerequisites | Precise |
| `durable_lsn` | Latest complete record-start LSN whose required prefix is durable | Precise |
| LSN | Record-start byte position | Precise |
| End position | Exclusive mathematical end; not persisted as LSN | Precise |
| `page_lsn` | Latest authorizing record/MTR/terminal LSN represented by page | Precise |
| `rec_lsn` | Full-image recovery base for dirty interval | Precise |
| MTR | One atomic physical B+ redo action, not SQL transaction | Precise |
| PAGE_INIT | Canonical complete initialization of a new page | Precise |
| PAGE_IMAGE | Canonical complete image of an existing page | Precise |
| Torn tail | Incomplete final WAL record outside valid complete prefix | Precise |
| Uncertain append | Runtime cannot establish valid-end/assigned-byte truth | Precise |
| Commit | Durable transaction outcome only at C3; runtime/client stages follow | Precise |
| Flush | Context-specific WAL or page synchronization; local objects are stated | Precise |

No material append/written/published/durable overloading survived the audit.

## Normative-language assessment

| Contract | Strength | Result |
|---|---|---|
| No cross-segment ordinary record | MUST/format invariant | Sufficient |
| Reserved/flag bytes zero | MUST/RESERVED_ZERO | Sufficient |
| No reservation-visible page metadata | MUST NOT | Sufficient |
| Exact rollback before authorization | MUST | Sufficient |
| No rollback-and-continue after authorization | MUST NOT | Sufficient |
| Post-authorization completion/noncontinuable | MUST | Sufficient |
| WAL-before-data | MUST | Sufficient |
| Durable COMMIT before terminal publication/success | MUST | Sufficient |
| PAGE_INIT private-before-publication | MUST | Sufficient |
| MTR common LSN/atomic publication | MUST | Sufficient |
| Numeric no-wrap | MUST NOT | Sufficient |
| Zero count domains | Missing | F12-1 |
| Implementation container/mutex roadmap | MAY wording | F12-2 |
| Background flush interval | Descriptive tuning default | F12-3 |

## Temporality and document ownership

### Temporal-language classification

| Occurrence family | Category | Assessment |
|---|---|---|
| before append, after append, later failure/writeback, first post-checkpoint modification | A — runtime ordering | Valid |
| previous transaction LSN, earlier preparatory record, later terminal record | B — WAL/transaction history | Valid |
| “initial segment size” in v1 persistent-format context | C/D — format/v1 scope | Valid |
| future record/encoding format rejected as unsupported | C — format evolution | Valid |
| asynchronous commit deferred from v1 | D — durable v1 scope | Valid |
| “initial configurable target capacity” | F — implementation chronology/tuning | F12-2 |
| “initial implementation … one mutex; later measured alternatives” | F — roadmap | F12-2 |
| “initial background flush interval” | F — implementation tuning | F12-3 |
| Exact section references | E — navigation | Valid |

No other project-chronology occurrence was found.

### Document ownership

| Material | Correct owner | Chapter 12 result |
|---|---|---|
| WAL format, LSN, append, durability, MTR, PAGE_INIT, WBD | ARCHITECTURE | Correct |
| Concrete initial mutex and append-buffer realization | DEVELOPMENT | Leakage: F12-2 |
| Approximate initial background flush interval | DEVELOPMENT/configuration guidance | Leakage: F12-3 |
| Crash schedules/fault injection | VERIFICATION | No leakage |
| Current WAL implementation availability | PROJECT_STATE | No leakage |
| Milestones/history/test results | devlog | No leakage |
| Source layout | DEVELOPMENT | No source-layout leakage |

## Analytical depth

| Boundary | Assessment |
|---|---|
| LSN semantics/exhaustion | Analytically sufficient |
| Append reservation vs authorization | Analytically sufficient |
| Append vs durability | Analytically sufficient |
| Durable prefix | Analytically sufficient |
| COMMIT durability | Analytically sufficient |
| WAL-before-data | Analytically sufficient |
| MTR atomicity/no-flush | Analytically sufficient |
| PAGE_INIT/publication | Analytically sufficient |
| Known vs uncertain failure | Analytically sufficient |
| Status image vs terminal evidence | Analytically sufficient |
| Recovery boundary | Analytically sufficient |
| Numeric exhaustion | Analytically sufficient |
| Zero-count payload grammar | Analytical/semantic completeness finding |
| Append-buffer mechanisms | Correctness rationale retained, but implementation roadmap leaks |
| Background flush interval | Nonarchitectural tuning detail |

## High-priority technical matrix

| # | Item | Result |
|---:|---|---|
| 1 | WAL owner | CONSISTENT |
| 2 | Raw I/O boundary | CONSISTENT |
| 3 | Segment naming | CONSISTENT |
| 4 | Segment layout | CONSISTENT |
| 5 | Segment size | CONSISTENT |
| 6 | LSN width | CONSISTENT |
| 7 | Invalid LSN | CONSISTENT |
| 8 | First LSN | CONSISTENT |
| 9 | LSN meaning | CONSISTENT |
| 10 | End-LSN meaning | CONSISTENT BUT SPECIALIZED |
| 11 | `page_lsn` meaning | CONSISTENT |
| 12 | LSN monotonicity | CONSISTENT |
| 13 | LSN nonreuse | CONSISTENT |
| 14 | LSN exhaustion | CONSISTENT |
| 15 | Segment-index exhaustion | CONSISTENT |
| 16 | Record header | CONSISTENT |
| 17 | Record-length arithmetic | CONSISTENT |
| 18 | Payload-length arithmetic | CONSISTENT |
| 19 | Checksum coverage | CONSISTENT |
| 20 | Alignment/padding | CONSISTENT; §12.14 example FINDING |
| 21 | Cross-segment record | CONSISTENT |
| 22 | Unknown type | CONSISTENT |
| 23 | Unsupported WAL format | CONSISTENT |
| 24 | Malformed record | FINDING: zero-count classification undefined |
| 25 | Transaction `prev_lsn` | CONSISTENT |
| 26 | Append owner | CONSISTENT |
| 27 | Reservation | CONSISTENT |
| 28 | Append construction | CONSISTENT |
| 29 | Authorization/publication | CONSISTENT |
| 30 | Durable-prefix definition | CONSISTENT |
| 31 | Flush target | CONSISTENT |
| 32 | Concurrent append ordering | CONSISTENT |
| 33 | Concurrent flush behavior | CONSISTENT |
| 34 | Append/flush race | CONSISTENT |
| 35 | Pre-authorization failure | CONSISTENT |
| 36 | Post-authorization failure | CONSISTENT |
| 37 | Uncertain append | CONSISTENT |
| 38 | Uncertain durability | CONSISTENT |
| 39 | Partial write | CONSISTENT |
| 40 | Durable COMMIT C3 | CONSISTENT |
| 41 | Commit vs data-page force | CONSISTENT |
| 42 | Commit vs status-page force | CONSISTENT |
| 43 | Commit acknowledgment | CONSISTENT |
| 44 | ABORT record | CONSISTENT |
| 45 | Read-only terminal-WAL exception | CONSISTENT |
| 46 | No user-DML physical undo | CONSISTENT |
| 47 | MTR definition | CONSISTENT |
| 48 | MTR identity | CONSISTENT |
| 49 | MTR LSN | CONSISTENT |
| 50 | MTR atomicity | CONSISTENT |
| 51 | MTR size limit | CONSISTENT |
| 52 | BTREE_MTR | CONSISTENT except zero count |
| 53 | HEAP mutation | CONSISTENT |
| 54 | FSM mutation | CONSISTENT BUT SPECIALIZED |
| 55 | TXN_STATUS mutation | CONSISTENT BUT SPECIALIZED |
| 56 | Catalog/system mutation | CONSISTENT |
| 57 | PAGE_INIT contents | CONSISTENT |
| 58 | PAGE_INIT bound publication | CONSISTENT |
| 59 | PAGE_INIT pre-auth failure | CONSISTENT |
| 60 | PAGE_INIT post-auth failure | CONSISTENT |
| 61 | PAGE_INIT uncertainty | CONSISTENT |
| 62 | WAL-before-data comparison | CONSISTENT |
| 63 | Copied-writeback gate | CONSISTENT |
| 64 | Data-page flush failure | CONSISTENT |
| 65 | Newer dirty generation | CONSISTENT |
| 66 | Segment creation durability | CONSISTENT |
| 67 | Namespace fsync | CONSISTENT |
| 68 | Checkpoint record | CONSISTENT BUT CHAPTER 13-OWNED |
| 69 | Checkpoint durability | CONSISTENT BUT CHAPTER 13-OWNED |
| 70 | Checkpoint not required for COMMIT | CONSISTENT |
| 71 | WAL retention/truncation | CONSISTENT BUT CHAPTER 13-OWNED |
| 72 | Shutdown ordering | CONSISTENT |
| 73 | Open/recovery append gate | CONSISTENT |
| 74 | Torn tail | CONSISTENT |
| 75 | Mid-log corruption | CONSISTENT |
| 76 | WAL inventory gaps | CONSISTENT |
| 77 | Database ownership | CONSISTENT |
| 78 | Target-owner validation | CONSISTENT |
| 79 | Redo idempotence | CONSISTENT |
| 80 | `page_lsn` comparison | CONSISTENT |
| 81 | Checksum before `page_lsn` | CONSISTENT |
| 82 | Torn-page reconstruction | CONSISTENT |
| 83 | Full-page-image policy | CONSISTENT |
| 84 | Delta-record policy | FINDING only for zero patch count |
| 85 | Pre-first-write WAL failure | CONSISTENT |
| 86 | Post-write WAL failure | CONSISTENT |
| 87 | Resource exhaustion | CONSISTENT |
| 88 | Failure/noncontinuable taxonomy | CONSISTENT |
| 89 | WalManager lifetime | CONSISTENT |
| 90 | Implementer invention | FINDING: count-domain policy required |

## Documentation-model matrix

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | Valid WAL/runtime temporal language | CONSISTENT |
| 3 | No current implementation status | CONSISTENT |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No DEVELOPMENT sequencing | FINDING |
| 6 | No VERIFICATION leakage | CONSISTENT |
| 7 | No PROJECT_STATE leakage | CONSISTENT |
| 8 | No devlog/history | CONSISTENT |
| 9 | No source-layout/implementation coupling | FINDING |
| 10 | Append/written/published/durable terminology | CONSISTENT |
| 11 | LSN/end-LSN precision | FINDING in illustrative values only |
| 12 | `page_lsn` terminology | CONSISTENT |
| 13 | Commit/durable distinction | CONSISTENT |
| 14 | WAL/data durability distinction | CONSISTENT |
| 15 | Authorization/durability distinction | CONSISTENT |
| 16 | Failure uncertainty explained | CONSISTENT |
| 17 | MTR rationale | CONSISTENT |
| 18 | PAGE_INIT rationale | CONSISTENT |
| 19 | WAL-before-data rationale | CONSISTENT |
| 20 | Independent of implementation status | CONSISTENT |

## Complete findings

### BLOCKING

None.

### MAJOR

#### F12-1 — Zero-count WAL grammar is undefined

- Section: §§12.8 and 12.10.2.
- Evidence:
  - [§12.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9111): `patch_count` is encoded and patches “MUST be nonempty,” but the count itself is not required to be nonzero.
  - [§12.10.2](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9228): `page_count` has no stated minimum.
  - Nested `PATCH_SET.patch_count` likewise has no stated minimum.
- Severity: MAJOR.
- Type: RECORD FORMAT.
- Scope: Cross-section/persistent recovery format.
- Arithmetic:
  - Zero-patch PAGE_DELTA: payload 24, total 72, span 72—fully frameable and CRC-valid.
  - Zero-page BTREE_MTR: payload 16, total 64—fully frameable and CRC-valid.
  - One-page zero-patch PATCH_SET: `data_length=8`, complete MTR remains frameable.
- Explanation: One conforming decoder may accept these as inert records that advance `page_lsn` or do nothing; another may classify them as malformed WAL.
- Canonical comparison: Chapter 12 claims an exact complete v1 grammar, while Chapter 13 must deterministically distinguish accepted records from corruption.
- Consequence: A WAL stream emitted or accepted by one implementation may prevent another implementation from reaching READY.
- Correct owner: `docs/ARCHITECTURE.md`.
- Future action: **T. FROZEN SEMANTIC ARCHITECTURE DECISION REQUIRED.**
- Required decision: specify whether each count must be at least one, or explicitly authorize its zero-count semantics.

### MINOR

#### F12-2 — §12.12.5 contains implementation sequencing and mechanism roadmap

- Evidence: [§12.12.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9662):
  - “initial configurable target capacity”
  - “The initial implementation MAY … under one mutex.”
  - “Later measured alternatives …”
- Severity: MINOR.
- Type: IMPLEMENTATION COUPLING.
- Scope: Local/document ownership.
- Explanation: The 8 MiB target, one-mutex realization, and later ring/per-thread alternatives describe implementation sequencing and tuning rather than a v1 semantic requirement.
- Canonical comparison: Architecture should preserve only contiguous append, no-hole, publication, concurrency, and lifetime semantics.
- Consequence: Chapter 12 becomes tied to a provisional implementation plan.
- Correct owner: `docs/DEVELOPMENT.md` if durable advice is desired.
- Future action: **D. DEVELOPMENT-OWNERSHIP FIX.**

#### F12-3 — §12.13 contains a nonarchitectural “initial” flush-interval default

- Evidence: [§12.13](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9687): “The initial background flush interval is approximately 10 ms … as a tuning default.”
- Severity: MINOR.
- Type: IMPLEMENTATION COUPLING.
- Scope: Local/document ownership.
- Explanation: The interval is expressly a mutable tuning default, not persisted or correctness-relevant.
- Canonical comparison: Architecture owns monotonic durable-prefix and waiter semantics, not an approximate scheduler interval.
- Consequence: Time-independent architecture carries a mutable runtime tuning choice.
- Correct owner: `docs/DEVELOPMENT.md`, configuration documentation, or omission.
- Future action: **D. DEVELOPMENT-OWNERSHIP FIX.**

#### F12-4 — Group-commit example uses impossible ordinary record LSNs

- Evidence: [§12.14](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:9703):
  - `T1 -> 1000`
  - `T2 -> 1100`
  - `T3 -> 1250`
- Severity: MINOR.
- Type: LSN.
- Scope: Local, with §12.3 comparison.
- Arithmetic:
  - `1000 mod 8 = 0`
  - `1100 mod 8 = 4`
  - `1250 mod 8 = 2`
- Explanation: §12.3 requires every ordinary record start to be 8-byte aligned.
- Canonical comparison: The conceptual values should all be legal record-start LSNs.
- Consequence: The example contradicts the canonical LSN domain and can mislead implementations/tests.
- Correct owner: `docs/ARCHITECTURE.md`.
- Future action: **A. LOCAL WORDING FIX.**

### EDITORIAL

None.

## Frozen architecture semantic questions

One:

> Are `PAGE_DELTA.patch_count`, `BTREE_MTR.page_count`, and nested `PATCH_SET.patch_count` required to be at least one, or are zero-count forms valid v1 records? If valid, what exact redo and `page_lsn` effects do they have?

No other frozen semantic question was found.

## Follow-up verification gaps

The existing methodology is strong for numeric exhaustion, provisional rollback, PAGE_INIT failure, BTREE_MTR old-or-new recovery, COMMIT/ABORT boundaries, WAL-before-data, and lifecycle shutdown. The following remain insufficiently explicit:

1. Exact WAL segment/header/record codec fixtures:
   - names, segment-zero prefix, no segment header;
   - 48-byte header fields and endianness;
   - CRC coverage;
   - zero alignment bytes;
   - WAL_PAD versus short zero tail;
   - no-cross-segment records.

2. Record-family payload codec matrices:
   - WAL PageId;
   - PAGE_DELTA patch grammar;
   - PAGE_INIT/PAGE_IMAGE exact image;
   - BTREE_MTR entry encoding and count domains;
   - unknown type versus malformed nested encoding.

3. Durable-prefix/group-commit procedures:
   - cross-segment durability requests;
   - coalesced waiters with distinct targets;
   - namespace synchronization before `durable_lsn`;
   - waiter wake/failure behavior.

4. Exact WAL-tail and segment-inventory recovery fixtures:
   - incomplete final header/payload/CRC;
   - CRC failure in authoritative mid-log;
   - all-zero next segment;
   - missing interior segment;
   - complete unknown record type.

5. TXN_STATUS two-record crash-prefix matrix:
   - pre-terminal image only;
   - terminal appended but undurable;
   - terminal durable but status page unflushed;
   - `rec_lsn=F` while `page_lsn=T`;
   - retained-image and recycling boundary.

These are `FOLLOW-UP VERIFICATION GAP`s, not architecture defects.

## Direct high-priority answers

| Question | Answer |
|---|---|
| WAL-owner ambiguity? | No |
| LSN semantic ambiguity? | No; one erroneous example |
| `page_lsn` ambiguity? | No |
| Record-length/format ambiguity? | Yes: zero-count domains |
| Append-authorization ambiguity? | No |
| Durable-prefix ambiguity? | No |
| C3 durable-COMMIT contradiction? | No |
| COMMITTED-to-ABORTED reversal path? | No |
| MTR atomicity ambiguity? | No, apart from zero-page grammar |
| PAGE_INIT publication ambiguity? | No |
| WAL-before-data ambiguity? | No |
| Recovery handoff ambiguity? | No |
| Torn-tail/corruption ambiguity? | No for ordinary records; zero-count validity remains unresolved |
| WAL exhaustion/wrap ambiguity? | No |
| Post-authorization rollback-and-continue path? | No |
| Uncertain-WAL-outcome continuation ambiguity? | No |
| Correctness-relevant invention required? | Yes, count-domain acceptance policy |
| Project-time/current-state wording? | Yes, §§12.12.5 and 12.13 |
| DEVELOPMENT-owned material? | Yes |
| VERIFICATION-owned procedure leakage? | No |
| PROJECT_STATE-owned material? | No |
| Devlog/history material? | No |
| Ambiguous terminology? | No material terminology ambiguity |
| Underexplained boundary? | Count-domain legality only |
| Timeless canonical v1 contract? | Not fully until F12-1–F12-4 are resolved |

## Previous-chapter regression

- Chapter 3: consistent; durable COMMIT is irreversible and uncertainty gates ordinary work.
- Chapter 4: format and exhaustion agree; §12.14’s example alone violates alignment.
- Chapter 5: consistent PAGE_INIT/heap WAL ownership.
- Chapter 6: consistent FSM WAL behavior.
- Chapter 7: consistent WAL-before-data and stable writeback.
- Chapter 8: consistent BTREE_MTR and new-page publication.
- Chapter 9: consistent C3/C4/C5 ordering.
- Chapter 10: consistent recovered terminal authority.
- Chapter 11: consistent lock retention through C4/A2 and release at C5/A3.

Chapter 11 compatibility result: **CONSISTENT**.

## Global documentation-model assessment

- Analytical rather than chronological? Mostly; two implementation-roadmap/tuning defects.
- Current-state narration? None.
- DEVELOPMENT sequencing leakage? Yes.
- VERIFICATION procedure leakage? None.
- PROJECT_STATE leakage? None.
- Devlog/history leakage? None.
- WAL terminology ambiguous? No material overload.
- Rationale sufficient? Yes, except count-domain acceptance is unspecified.
- Readable without knowing whether WAL exists in source? Yes.
- Timeless canonical v1 contract? Not fully until the findings are resolved.

Source-layout search found no `.cpp`, `.h`, source-directory, TODO, milestone, or implementation-status coupling. The one-mutex/ring-buffer and 10 ms text is algorithm/tuning coupling, not source-layout coupling.

## Recommended next action

**Frozen semantic review required.**

Recommended order:

1. Decide the zero-count record grammar.
2. Make targeted Chapter 12 documentation fixes for F12-1–F12-4.
3. Synchronize the five narrow verification gaps.
4. Only then treat Chapter 12 as closed.

## Recommended Chapter 13 review scope

Based on the actual boundary, Chapter 13 review should focus on:

- control/checkpoint persistent formats and installation authority;
- WAL segment inventory and valid-tail reconstruction;
- unknown/malformed/torn-tail classification;
- analysis transaction table and dirty-page table reconstruction;
- page-owner validation and deferred FileId resolution;
- `page_lsn` comparison after checksum validation;
- full-image torn-page reconstruction;
- PAGE_DELTA and BTREE_MTR idempotence/atomic replay;
- TXN_STATUS image plus terminal-record reconstruction;
- loser transaction finalization without user-DML undo/CLRs;
- checkpoint retention floor and WAL recycling;
- recovery-created terminal records and WAL headroom;
- READY admission, recovery failure, and namespace durability;
- repeated recovery idempotence and durable-COMMIT preservation.

Known out-of-scope observations in §14.17, §§15.7.2–15.7.3, §31.7, and Appendix C were not modified or reclassified.

## Repository state and read-only guarantee

Initial state:

- `git status --short`: clean
- staged files: none
- HEAD: `3701e9ea201712963ca35a19d5d2b337ff0410c6`

Final state:

- `git status --short`: clean
- staged files: none
- HEAD: `3701e9ea201712963ca35a19d5d2b337ff0410c6`
- `git diff --check`: passed with no output

Files modified by audit: **NONE**.

No audit-created repository change occurred. No pre-existing material was changed, reverted, or staged. No build, test, benchmark, implementation, scaffolding, devlog, review artifact, milestone, commit, or Chapter 13 review was performed.

Implementation Phase 2 remains **NOT STARTED / NOT AUTHORIZED**.