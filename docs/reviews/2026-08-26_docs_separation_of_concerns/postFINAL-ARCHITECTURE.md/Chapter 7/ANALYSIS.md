# Chapter 7 review verdict

**CHAPTER 7 — TARGETED DOCUMENT FIXES RECOMMENDED**

Chapter 7 is technically coherent and sufficiently complete to implement the canonical v1 I/O and buffer-management contract. No blocking or major semantic defect was found.

The chapter has three localized project-time wording defects, one incomplete error-category summary, and one imprecise cross-reference:

- BLOCKING: **0**
- MAJOR: **0**
- MINOR: **4**
- EDITORIAL: **1**

No frozen architecture semantic review is required.

## Review scope and repository state

Primary scope read:

- [Chapter 7 — I/O and Buffer Management](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:4370), through the line before Chapter 8.
- [AGENTS.md](/home/yghtso/Projects/DBlusBlus/AGENTS.md)
- Architecture front matter.

Context-only architecture consulted:

- Chapters 2–6.
- Chapter 8 consumer boundaries.
- Chapters 9, 11–16 where locks, WAL, recovery, reclamation, DML, descriptors, bootstrap, and retirement interact with Chapter 7.
- §§39.1 and 41.1.
- Appendix B.

Other live documents consulted:

- [PROJECT_STATE.md](/home/yghtso/Projects/DBlusBlus/docs/PROJECT_STATE.md)
- [DEVELOPMENT.md](/home/yghtso/Projects/DBlusBlus/docs/DEVELOPMENT.md)
- [VERIFICATION.md](/home/yghtso/Projects/DBlusBlus/docs/VERIFICATION.md)

Initial Git state:

| Check | Result |
|---|---|
| `git status --short` | Clean |
| `git diff --cached --name-only` | Empty |
| `git rev-parse HEAD` | `7f16c96d767b0c6a997ee64f2474ebb3ec6f93c6` |

No pre-existing repository changes were present.

# Actual Chapter 7 structure

Chapter title: **7. I/O and Buffer Management**

| Section | Exact heading | Canonical responsibility | Documentation role |
|---|---|---|---|
| 7.1 | Scope | Raw I/O versus resident-page boundary | Architecture with cross-reference issue |
| 7.2 | Explicit I/O model | Explicit positional I/O; no mmap dependency | Architecture-appropriate |
| 7.3 | DiskManager responsibility boundary | Raw file/descriptor/I/O ownership | Architecture-appropriate |
| 7.3.1 | File lifecycle boundary | Registered-file and higher-level publication boundary | Architecture-appropriate |
| 7.4 | Page-file I/O semantics | Managed page-transfer contract | Architecture-appropriate |
| 7.4.1 | Positional access | Offset-stable `pread`/`pwrite` | Architecture-appropriate |
| 7.4.2 | Complete page transfers | Exact 8192-byte transfer requirement | Architecture-appropriate |
| 7.4.3 | EINTR behavior | Retry and close semantics | Architecture-appropriate |
| 7.4.4 | Allocation boundary | No implicit sparse allocation | Architecture-appropriate |
| 7.4.5 | File-size validity | Aligned physical size | Architecture-appropriate |
| 7.4.6 | Checked physical offsets | Checked signed-offset arithmetic | Architecture-appropriate |
| 7.4.7 | Error context | Structured raw-I/O context | Architecture-appropriate |
| 7.4.8 | Page-file durability primitive | File-data durability versus namespace durability | Architecture-appropriate |
| 7.5 | BufferPool ownership | Ordinary page-lifetime and access ownership | Architecture with temporality issue |
| 7.6 | Canonical resident-frame state machine | Runtime frame lifecycle | Architecture-appropriate |
| 7.6.1 | Frame metadata and state dimensions | Identity, pins, dirtiness, I/O, recovery metadata | Architecture-appropriate |
| 7.6.2 | State transition table | Canonical frame transitions | Architecture-appropriate |
| 7.6.3 | Fetch linearization | Hit/miss/same-page load coordination | Architecture-appropriate |
| 7.6.4 | Validation before resident publication | Validation and owner trust boundary | Architecture-appropriate |
| 7.6.5 | Crash versus runtime state | Process-local versus durable state | Architecture-appropriate |
| 7.7 | RAII page guards | Scoped pin/latch ownership | Architecture-appropriate |
| 7.7.1 | Pin and latch are different contracts | Residency versus page-byte exclusion | Architecture-appropriate |
| 7.7.2 | Guard and reference lifetime safety | Release order and borrowed-reference validity | Architecture-appropriate |
| 7.8 | Resident-page table | PageId-to-load/frame mapping | Architecture with temporality issue |
| 7.9 | Pinning and eviction eligibility | Exact replaceability predicates | Architecture-appropriate |
| 7.10 | Dirty pages and stable writeback | Dirty meaning and writeback protocol | Architecture-appropriate |
| 7.10.1 | Persistent mutation publication | WAL-coupled mutation publication | Architecture-appropriate |
| 7.10.2 | Copied stable writeback | Snapshot/write/reconciliation protocol | Architecture-appropriate |
| 7.10.3 | Stable completion and dirty-generation reconciliation | Concurrent flush/mutation safety | Architecture-appropriate |
| 7.10.4 | Flush failure | Failure-preserving frame state | Architecture-appropriate |
| 7.10.5 | DPT and checkpoint synchronization | Atomic dirty/DPT/FPI visibility | Architecture-appropriate |
| 7.11 | WAL-before-data enforcement | Centralized WAL durability gate | Architecture-appropriate |
| 7.12 | CLOCK replacement | Canonical v1 replacement policy | Architecture-appropriate |
| 7.12.1 | Victim reservation and eviction | Reservation and safe eviction | Architecture-appropriate |
| 7.12.2 | Mapping removal and frame reassignment | Identity reset and stale-handle prevention | Architecture-appropriate |
| 7.12.3 | No eligible frame | Buffer exhaustion result | Architecture with temporality issue |
| 7.12.4 | Newly allocated pages | PAGE_INIT and frame/file publication | Architecture-appropriate |
| 7.12.5 | File retirement and drain | ACTIVE/RETIRING/CLOSED frame drain | Architecture-appropriate |
| 7.12.6 | Controlled BufferPool shutdown | Quiescing, drain, and required flush | Architecture-appropriate |
| 7.12.7 | BufferPool error categories | Structured BufferPool outcomes | Architecture with completeness issue |
| 7.13 | I/O and buffer invariants | Consolidated normative invariants | Architecture-appropriate |

# Section-by-section review

Codes: `OK` = clear/consistent; `—` = not locally applicable; `Thin` = clear but rationale concise; `F` = finding; `N` = note.

| Sec. | Role | Time | Owner | Depth | Terms | Layer | Life | Pin/latch | Dirty/flush | Evict | WAL | Validation | Concurrency | Failure | X-ref | Semantics | Status |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 7.1 | Boundary | OK | OK | OK | OK | OK | — | — | — | — | OK | OK | — | OK | F | OK | FINDING |
| 7.2 | I/O model | N | OK | OK | OK | OK | — | — | — | — | — | — | OK | OK | OK | OK | CLEAN WITH NOTE |
| 7.3 | DiskManager | OK | OK | OK | OK | OK | OK | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 7.3.1 | Lifecycle | OK | OK | OK | OK | OK | OK | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 7.4 | Page I/O | OK | OK | OK | OK | OK | OK | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 7.4.1 | Positioning | OK | OK | OK | OK | OK | — | — | — | — | — | — | OK | OK | OK | OK | CLEAN |
| 7.4.2 | Transfer | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 7.4.3 | EINTR | OK | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | CLEAN |
| 7.4.4 | Allocation | OK | OK | OK | OK | OK | — | — | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 7.4.5 | Size | OK | OK | OK | OK | OK | — | — | — | — | — | OK | — | OK | OK | OK | CLEAN |
| 7.4.6 | Offset | OK | OK | OK | OK | OK | — | — | — | — | — | OK | — | OK | OK | OK | CLEAN |
| 7.4.7 | Errors | OK | OK | OK | OK | OK | — | — | — | — | — | — | — | OK | OK | OK | CLEAN |
| 7.4.8 | Durability | OK | OK | OK | OK | OK | OK | — | OK | — | OK | — | OK | OK | OK | OK | CLEAN |
| 7.5 | BP owner | F | F | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | FINDING |
| 7.6 | Frame states | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.6.1 | Metadata | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.6.2 | Transitions | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.6.3 | Fetch | OK | OK | OK | OK | OK | OK | OK | — | OK | — | OK | OK | OK | OK | OK | CLEAN |
| 7.6.4 | Validation | OK | OK | OK | OK | OK | OK | OK | — | — | — | OK | OK | OK | OK | OK | CLEAN |
| 7.6.5 | Crash/runtime | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.7 | Guards | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | CLEAN |
| 7.7.1 | Pin/latch | OK | OK | OK | OK | OK | OK | OK | — | OK | — | — | OK | OK | OK | OK | CLEAN |
| 7.7.2 | References | OK | OK | OK | OK | OK | OK | OK | — | OK | — | — | OK | OK | OK | OK | CLEAN |
| 7.8 | Page table | F | F | OK | OK | OK | OK | OK | — | OK | — | OK | OK | OK | OK | OK | FINDING |
| 7.9 | Eligibility | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | CLEAN |
| 7.10 | Dirty/writeback | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.10.1 | Mutation | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.10.2 | Stable copy | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.10.3 | Reconcile | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.10.4 | Flush fail | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.10.5 | DPT | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.11 | WAL gate | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.12 | CLOCK | N | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN WITH NOTE |
| 7.12.1 | Victim | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.12.2 | Reassign | OK | OK | OK | OK | OK | OK | OK | OK | OK | — | OK | OK | OK | OK | OK | CLEAN |
| 7.12.3 | Exhaustion | F | F | OK | OK | OK | OK | OK | — | OK | — | — | OK | OK | OK | OK | FINDING |
| 7.12.4 | New page | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.12.5 | Retirement | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.12.6 | Shutdown | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |
| 7.12.7 | Errors | OK | OK | OK | OK | OK | — | — | — | — | OK | F | — | F | OK | F | FINDING |
| 7.13 | Invariants | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | OK | CLEAN |

# Technical architecture assessment

## Ownership and I/O layers

| Layer | Owns | Consumes | Must not own | Relationship |
|---|---|---|---|---|
| DiskManager | Root-relative namespace operations, registered handles, positional I/O, exact transfers, file size/extension, `fdatasync` | OS file APIs and managed-file registration | Page layouts, page identity interpretation, BufferPool policy | Lowest managed storage provider |
| Registered storage/PageFile owner | FileId/FileKind/object identity, published bound, PageNo validity, page-family validator | DiskManager | Frame replacement, pins, latches | Typed file owner used by BufferPool |
| BufferPool | Resident frames, PageId mapping, pins, guards, dirty state, writeback, replacement | Registered file owner, DiskManager I/O, WAL durability | Logical tuple/index semantics or transaction isolation | Runtime resident-page owner |
| Frame | One current PageId binding plus page bytes and runtime metadata | BufferPool state machine | Persistent identity independent of PageId | Internal BufferPool storage |
| PageGuard | Scoped pin and latch claim; borrowed-view lifetime | BufferPool/frame | Transaction locking, persistence policy | Caller-facing lifetime capability |
| Typed page views | Decode/validate/mutate guarded bytes | Guarded resident bytes | I/O, pinning, eviction, flushing | Consumers above BufferPool |

### DiskManager contract

| Concern | Chapter 7 result |
|---|---|
| Descriptor ownership | Managed by DiskManager/registered-file lifecycle |
| Access method | Exact positional `pread`/`pwrite`; no shared file-position dependence |
| mmap | Not part of the canonical page path |
| Short transfer | Explicit error; never successful page I/O |
| EINTR | Retried where safe; `close` is not blindly retried |
| File extension | Explicit append-first allocation owner |
| Sparse writes | Prohibited as implicit allocation |
| Size validity | Exact page multiple and checked bounds |
| Durability | `fdatasync` for file bytes/length; directory `fsync` separately owns namespace durability |
| Thread safety | Positional access and registry ownership permit concurrent operations |
| Identity/path | Managed descriptor identity, not caller-supplied path reopening |
| Logical page interpretation | Explicitly excluded |

### PageFile contract and lifetime

| Question | Result |
|---|---|
| Owns DiskManager? | Concrete C++ ownership is not architecture-pinned |
| Borrows DiskManager? | Permitted only under database/registered-owner lifetime |
| Owns fd directly? | Managed-file registration/DiskManager owns the active handle abstraction |
| Immutable descriptor metadata | Owned by the registered storage owner |
| Lifetime precondition | Registration and descriptor context outlive loads, resident frames, drains, and validation |
| Shutdown survival | No ordinary access survives BufferPool/database teardown |
| Buffer policy | Not owned by PageFile |
| Current non-owning pointer implementation | Correctly belongs in PROJECT_STATE, not architecture |

No DiskManager/PageFile lifetime ambiguity was found at the architectural level.

## BufferPool identity and frame model

| Property | Contract |
|---|---|
| Canonical key | `PageId = (FileId, PageNo)` |
| Non-keys | Frame index, fd, path, pointer, and object address |
| One-resident-copy rule | At most one usable ordinary resident frame per PageId |
| Concurrent miss | One `LOAD_INTENT`/loader; other fetches join |
| Failure | Failed loader removes the in-progress mapping and wakes joiners with failure |
| Runtime table | PageId → logical loading/resident/evicting entry → frame |
| Validation boundary | Complete required validation before ordinary resident publication |
| Recovery-private copy | May temporarily hold untrusted/torn bytes, but cannot become ordinary resident state before canonical validation |

### Frame-condition table

| Condition | PageId | Page table | Pin | Dirty | Evictable | I/O | Caller-visible |
|---|---|---|---|---|---|---|---|
| `FREE` | No | No | 0 | No | Allocation candidate | No | No |
| `LOADING` | Reserved | In-progress | Private creator/load claim | No or unpublished initialized mutation | No | Read or private construction | No |
| `RESIDENT` clean | Yes | Usable | ≥0 | No | Only if §7.9 predicates hold | Optional copied flush irrelevant | Yes when pinned/guarded |
| `RESIDENT` dirty | Yes | Usable | ≥0 | Yes | Only if §7.9 predicates hold | Copied writeback allowed | Yes |
| Resident writeback | Yes | Usable | May be pinned | Yes until reconciliation | No while reserved | `WRITEBACK_IN_PROGRESS` | Existing/future guarded access permitted |
| `EVICTING` clean | Yes until removal/reset | Non-pinnable | 0 | No | Victim reserved | No | No |
| `EVICTING` dirty | Yes until successful writeback | Non-pinnable | 0 | Yes | Victim reserved | Writeback | No |
| Retiring-file frame | Yes until drain/reset | Blocked for new pins | Draining | May remain dirty | No | Existing I/O drains | Existing guards only until release |

Frame reassignment is complete: the old mapping is made non-pinnable and removed, pins/latches/I/O are drained, dirty writeback succeeds if required, all metadata is reset, and only then may a new `LOADING` identity be bound. Stale bytes or stale identity cannot become visible as the new page.

## Pin, latch, guard, and transaction lock

| Property | Pin | Page latch | PageGuard | Transaction lock |
|---|---|---|---|---|
| Prevents eviction/reassignment | Yes | Not independently | Yes, through owned pin | No |
| Protects page bytes | No | Yes | Through read/write latch mode | No |
| Owns caller lifetime | Counted claim | Held exclusion interval | Scoped single-owner claim | Transaction/protocol lifetime |
| Shared/exclusive modes | No | Yes | Read/write guards correspond | Logical lock modes |
| Logical isolation | No | No | No | Yes |
| Persistent effect | None | None | May publish guarded mutation | Governs logical conflict/visibility |
| Release | Checked decrement | Unlock | Latch first, then pin | Transaction protocol |
| Deadlock domain | Resource/lifetime | Internal latch order | Inherits page-latch rules | Logical lock manager |
| May wait while page latch held | N/A | Must not wait for transaction lock | Must release short latch first | Yes, outside page latch |

Pin counts use checked arithmetic and cannot wrap or underflow. A zero-pin frame may remain resident. Pinning does not imply latch ownership or logical isolation.

### PageGuard lifetime

| Event | Required effect |
|---|---|
| Acquisition | Obtain exactly one pin and appropriate shared/exclusive latch |
| Failed latch acquisition/cancellation | Release the acquired pin exactly once |
| Move/transfer | Transfer the single ownership claim without duplication |
| Borrowed page/view creation | Valid only within the guard’s applicable lifetime and mode |
| Dirty marking | Write-guard acquisition alone is insufficient; successful persistent mutation publication owns dirtying |
| Early release/destruction | Release latch first, then decrement pin |
| Post-release | All borrowed pointers/views are invalid |
| Eviction race | Frame cannot become evictable while guard still depends on its bytes |

Separate read/write guards are architecture-defined conceptual modes; the architecture does not unnecessarily mandate a particular C++ class layout.

## Dirty state, flush, and WAL

Dirty means the current published persistent-byte generation is not known durable in the data file. It does not mean committed, WAL-durable, or transaction-visible.

| Situation | Dirty possible? | `page_lsn` | WAL durable before data write? | Data flush required for transaction durability? | May remain resident? |
|---|---|---|---|---|---|
| Uncommitted physical mutation | Yes | Mutation/MTR LSN | Yes before page write | No | Yes |
| Committed transaction | Yes | Latest page mutation LSN | Yes before page write | No; v1 is NO-FORCE | Yes |
| Aborted but physically retained tuple version | Yes | Applicable mutation LSN | Yes before page write | No | Yes |
| Checkpoint | Enumerated via DPT | Stable copied LSN | Yes | Not equivalent to COMMIT | Yes |
| Explicit flush | Yes before success | Stable copied LSN | Yes | No | Yes |
| Eviction | Yes before success | Stable copied LSN | Yes | No | No after successful eviction |
| Shutdown | Required dirty pages | Stable copied LSN | Yes | Required for clean shutdown, not transaction COMMIT | No after teardown |

### Flush/eviction table

| Operation | Pinned allowed? | WAL-before-data | Writes data | Clears dirty | Removes mapping | Reassigns frame | Failure state |
|---|---:|---:|---:|---:|---:|---:|---|
| Copied explicit flush | Yes | Yes | Yes | Only if identity/generation still match | No | No | Original frame remains mapped and dirty |
| Background flush | Yes, with stable copy | Yes | Yes | Same reconciliation rule | No | No | Skip/requeue/report; no false clean |
| Clean eviction | No | N/A | No | Already clean | Yes | May after reset | Reservation must roll back safely |
| Dirty eviction | No | Yes | Yes + `fdatasync` | On stable successful completion | Yes after success | Yes after reset | Restore resident mapping/dirty state; requesting load fails |
| Shutdown flush | Drains pins first | Yes | Yes | On stable completion | Teardown after drain | No new binding | Clean shutdown cannot be claimed |

Flush and eviction are not conflated: flush preserves frame identity; eviction ends it.

A flush racing with mutation is correctly handled through a copied stable generation. Completion clears dirty only if the frame still represents the same PageId and its generation still matches. A newer mutation therefore cannot be erased by an older flush completion.

Checksums are finalized on the private stable image under §4.12.2; the complete 8192-byte image is written. `pwrite` alone is not stable completion: the required `fdatasync` must also succeed.

`FlushAll` is not defined as a public architectural API. Controlled shutdown owns the corresponding required dirty-page drain without implying an API or COMMIT boundary.

## BufferPool operations

| Operation | Preconditions | Identity/pin/guard effects | Dirty/WAL effects | Failure/publication effects |
|---|---|---|---|---|
| Fetch hit | Active owner, published PageId, resident pinnable mapping | Checked pin increment; acquire requested guard | None | No identity change |
| Fetch miss | Valid published PageId; sole load intent | Reserve/bind LOADING frame; no public guard before validation | Victim may require WAL-gated writeback | Publish RESIDENT mapping only after validation |
| Join miss | Existing same-PageId intent | Wait/join same logical load | None | Receive same success or load failure |
| New-page allocation | Sole private append intent | LOADING private frame; creator claim becomes one pin at publication | PAGE_INIT/MTR establishes page_lsn/dirty/recovery state | File publication and RESIDENT usability are coordinated |
| Explicit flush | Resident stable page identity | Identity and mapping retained | Copy, checksum, WAL gate, write, sync, reconcile | Failure retains dirty state |
| Evict | All §7.9 predicates and victim reservation | Mapping becomes non-pinnable; remove/reset after success | Dirty victim uses stable writeback | Failure restores old association |
| Guard release | Valid owning guard | Latch release, then one unpin | No implicit dirtying | May make frame eligible |
| File retirement | RETIRING gate established | Block new pins/loads; drain/remove/reset | Dirty discard only with semantic retirement authority | No unlink before drain |
| Shutdown | BufferPool quiescing | Reject new acquisitions; drain guards/I/O | Flush persistence-required dirty pages | Failure prevents clean-shutdown claim |

### Fetch cases

| Case | Frame/I/O | Validation | Publication and result | Failure cleanup |
|---|---|---|---|---|
| Hit | Existing resident frame; no read | Prior resident validation remains applicable | Pin and guard returned | Pin acquisition failure leaves count unchanged |
| Miss | One reserved LOADING frame; exact read | Checksum/common identity, owner, bounds, L1/L2 | Atomically RESIDENT and usable | Remove intent, reset frame, notify waiters |
| Concurrent same-page miss | One loader; others join | Performed once for logical load | One resident copy | Same failure delivered to joiners |
| Invalid PageId | No data-page read | Registered owner/published bound rejects first | No mapping | Structured not-found/invalid result |
| Corruption/wrong owner | Read may complete | Required validation rejects | Never ordinarily resident | Reset frame; no poisoned mapping |
| I/O failure/short read | Exact transfer fails | No trust/publication | No pin/guard | Reset and wake waiters |

## Replacement and exhaustion

CLOCK is an intentional canonical v1 replacement decision, not accidental implementation coupling. Its metadata is process-local and has no persistent correctness role. Alternatives are outside the v1 baseline unless architecture is revised.

| Eligibility condition | Required |
|---|---|
| State is `RESIDENT` | Yes |
| `pin_count == 0` | Yes |
| I/O state is `NONE` | Yes |
| No latch owner or waiter | Yes |
| No mutation no-flush barrier | Yes |
| No existing drain/victim reservation | Yes |
| Atomic final recheck before `EVICTING` | Yes |

One complete unsuccessful CLOCK attempt returns `NO_REPLACEABLE_FRAME`; it does not steal pinned frames or wait indefinitely. This is separate from disk capacity, PageNo exhaustion, WAL exhaustion, and file resource-full outcomes.

Pinned-page flush is permitted using the stable-copy protocol. Pinned-page eviction is forbidden.

No mandatory background flusher is introduced; any background writeback must obey the same stable-copy, generation, WAL, and failure rules.

## Validation and publication

### Validation order

| Step | Trusted after step | Must precede | Failure owner/result |
|---|---|---|---|
| Registered owner and published-bound lookup | Requested FileId/PageNo is addressable | Physical data-page read/ordinary join | File/page not found, retired, or closing |
| Exact read | Full 8192-byte transfer obtained | Byte interpretation | Raw I/O failure |
| Family/version dispatch | Expected format family/version selected | v1 layout parsing | Unsupported format or corruption |
| Checksum/common header | Common bytes and `page_lsn` trustworthy | page_lsn/WAL semantics | Corruption |
| PageId/FileKind/PageType identity | Correct physical/logical identity | Typed access | Corruption/wrong owner |
| Registered owner L1/L2 validation | Complete local owner constraints | Resident publication | Corruption/owner failure |
| `LOADING -> RESIDENT` publication | Ordinary caller may observe bytes | Guard return | No partial publication |

### Owner-validation dimensions

| Dimension | Expected source | Before frame publication? | Before typed use? | Failure |
|---|---|---:|---:|---|
| Database/registered file identity | Active registered owner | Yes | Yes | Not found/retired/owner failure |
| FileId | PageId and registered descriptor | Yes | Yes | Wrong owner/corruption |
| PageNo | Requested PageId/common header | Yes | Yes | Corruption/not found |
| Published bound | Registered owner | Yes | Yes | Not found/unpublished |
| FileKind | FileSuperblock/registered owner | Yes | Yes | Corruption/unsupported |
| PageType | Common header and owner dispatch | Yes | Yes | Corruption/unsupported |
| Object/Table/Index owner | Immutable registered descriptor | Yes where locally available | Yes | Wrong owner/corruption |
| Family-local structure | Nonfetching owner validator | Yes | Yes | Corruption |

On-disk page publication and frame publication are correctly distinct:

- A published page may be nonresident.
- A frame may privately load a published page without being visible.
- A new page may occupy a private LOADING frame before either file publication or ordinary frame publication.
- Frame residency never itself publishes a PageNo.

### New-page publication

| Stage | File/WAL/page state | Frame mapping | Caller visibility | Failure outcome |
|---|---|---|---|---|
| Reserve append intent | Serialized unpublished PageNo | Private load intent | None | Release intent |
| Bind private frame | Extent ownership pending | `LOADING` | None | Reset frame |
| Construct image | Canonical PAGE_INIT image | Still private | None | Restore before-image/tail if pre-append |
| WAL/MTR append | Publication-authorizing record exists | Still non-usable | None | Finish publication or noncontinuable after uncertain/valid append |
| Owning bound publication | `published_page_count` advances | Atomically becomes RESIDENT | Creator receives one pin/guard | Cannot reuse published PageNo |
| Later crash | Chapter 4/12/13 reconciliation | Runtime mapping disappears | Recovery reconstructs | No private frame state survives |

## Retirement, shutdown, recovery, and exceptions

### Raw-I/O exceptions

| Context | BufferPool bypass | Why/owner | Validation | End of exception |
|---|---:|---|---|---|
| WAL segments | Yes | WalManager owns log format and durability | Chapter 12 | Not converted to BufferPool pages |
| `database.control` | Yes/specialized | Lifecycle/recovery owner uses dual-slot control protocol | Chapters 3/13 | Before ordinary READY access |
| Initial FileSuperblock/open validation | Yes/specialized | File identity must be established before BufferPool registration | Chapters 3/4/16 | Registration completes |
| Bootstrap/create private publication | Yes where publication owner requires | No ordinary page exists yet | Canonical initialization/owner validation | Page/file is durably published |
| Recovery-private torn-page reconstruction | Yes or special recovery BufferPool mode | Ordinary validation cannot trust torn image | Chapter 13 reconstruction then canonical validation | Before ordinary resident publication/READY |
| Namespace create/rename/unlink | Yes | DiskManager/storage namespace owner | §4.7 synchronization | Durable namespace transition |
| Ordinary HEAP/FSM/BTREE/catalog/status page use | No | BufferPool owns runtime page lifetime | Full owner validation | Until retirement/shutdown |

There is no vague general-purpose raw-I/O escape hatch.

### Retirement

| Event | New fetches | Existing pins/I/O | Dirty pages | Mapping/frame | Descriptor/reclamation owner |
|---|---|---|---|---|---|
| ACTIVE | Allowed | Normal | Normal flush rules | Resident mappings allowed | Registered file owner |
| ACTIVE→RETIRING | Rejected at gate | Drain | Flush or discard only as semantic owner authorizes | Mapping becomes non-pinnable | Drop/retirement owner, Chapter 14 |
| Drain complete | Rejected | Zero/drained | No unresolved required dirty data | Remove mapping/reset frame | Descriptor still valid through drain |
| CLOSED/unlink | Rejected | None | None requiring persistence | Frame reusable | Close handle, then durable §4.7.7 unlink |
| Drain failure | Rejected/retiring | Preserve state | Preserve required dirty state | No unlink | Higher-level failure owner |

FileId nonreuse prevents stale PageIds from binding to a newly created file.

Shutdown correctly quiesces new acquisitions, drains guards/I/O, flushes required dirty pages under WAL-before-data, and tears down BufferPool before WAL. A failed required flush cannot produce a successful clean-shutdown result.

Recovery-private access is tightly bounded and cannot leak unvalidated bytes into ordinary resident state. Bootstrap follows the corresponding private-publication boundary.

## Consumer consistency

| Chapter 7 contract | Consumer | Result |
|---|---|---|
| Guarded typed-page bytes | HEAP/HeapPage | CONSISTENT |
| Guarded actual heap geometry and separately managed hints | FSM | CONSISTENT |
| Multi-page guarded mutations and no-flush MTR barrier | B+ tree | CONSISTENT BUT SPECIALIZED |
| Immutable descriptor context for owner validation | Catalog pages | CONSISTENT |
| Physical visibility/status-page write ordering | TXN_STATUS | CONSISTENT BUT SPECIALIZED |
| Private torn-page reconstruction before ordinary publication | Recovery | CONSISTENT BUT SPECIALIZED |
| DPT snapshot/writeback coordination | Checkpoint | CONSISTENT BUT SPECIALIZED |
| File retirement and grace-authorized discard/reuse | Vacuum/drop | CONSISTENT BUT SPECIALIZED |

Chapter 6 is compatible: FSM pages use ordinary BufferPool/PAGE_INIT/WAL rules; stale category values remain advisory, but structural and owner validation precede candidate use.

## Concurrency and failure matrices

### Concurrency

| Race | Required coordination | Legal survivor | Forbidden outcome |
|---|---|---|---|
| Same nonresident page fetched twice | Sole load intent and joiners | One resident frame | Two mutable resident copies |
| Fetch versus eviction | Non-pinnable victim reservation and atomic eligibility recheck | Fetch pin or eviction reservation | Pin acquired after reassignment begins |
| Flush versus mutation | Stable copied generation plus reconciliation | Old generation durable while newer remains dirty | Clearing newer dirty state |
| Guard release versus eviction | Latch release before checked unpin | Eviction after complete release | Eviction while guard uses bytes |
| Retirement versus fetch | ACTIVE→RETIRING gate | Existing operations drain | New pin/load after retirement gate |
| Shutdown versus fetch | Quiescing gate | Existing operation drains | New acquisition after quiesce |
| Dirty eviction versus WAL flush | WAL durability before page write | Durable WAL then page write | Data page reaches storage first |
| Reassignment versus stale guard | Zero pins, mapping removal, complete reset | New LOADING identity | Old pointer observes new page |
| Checkpoint versus clean→dirty | DPT-publication gate | Complete old or new DPT state | Missing rec_lsn/FPI transition |
| Different-page access | Per-frame/page synchronization; partitionable table | Concurrent progress | Unnecessary permanent global serialization |

### Failures

| Failure | Frame/page-table state | Pin/guard and dirty state | Retry/result |
|---|---|---|---|
| Read I/O/short read | Load intent removed; frame reset | No public pin/guard | Structured raw-I/O failure; later retry possible |
| Corruption/wrong owner | Never RESIDENT | No public guard | Corruption/owner result |
| Unsupported format | Must not interpret as v1 | No public guard | Canonical unsupported-format result |
| Victim write failure | Original mapping restored RESIDENT | Dirty retained | Requesting load fails |
| WAL flush failure | Page not written | Dirty retained | WAL durability failure |
| Checksum/write failure | No false stable completion | Dirty retained | Raw/writeback failure |
| No victim | Existing frames unchanged | Existing pins unchanged | `NO_REPLACEABLE_FRAME` |
| Shutdown during acquisition | No new acquisition | Existing claims drain | `BUFFERPOOL_QUIESCING` |
| Retirement race | File gate rejects new operation | Existing claim drains | `FILE_RETIRED_OR_CLOSING` |
| Uncertain WAL/publication state | No ordinary continuation | Protected state retained | `STORAGE_NONCONTINUABLE` |

Global lock/latch ordering is compatible with Chapters 9, 11, 12, and 14. Chapter 7 distinguishes page latches from transaction locks and does not permit waiting for a transaction-level lock while holding a short page latch. Its synchronization model remains partitionable and compatible with the Chapter 1 parallel-ready goal.

# Documentation-model review

## Global answers

| Question | Answer |
|---|---|
| Analytical rather than chronological? | **Yes**, overall |
| Current-project-state narration? | No direct availability claim, but three project-stage phrases remain |
| DEVELOPMENT sequencing leakage? | **Yes**, localized in §§7.8 and 7.12.3 |
| VERIFICATION procedure leakage? | No |
| devlog/history leakage? | No |
| Implementation absence treated as optionality? | The §7.5 phrase can imply this; canonical rules elsewhere prevent an actual semantic gap |
| Ambiguous ownership/lifetime terminology? | No correctness-relevant ambiguity |
| Rationale sufficient? | Yes |
| Readable without knowing whether BufferPool is implemented? | Yes technically, though the three temporal passages should be cleaned |
| Fully timeless canonical v1 contract as written? | Not quite; targeted wording fixes are required |

PROJECT_STATE correctly owns the fact that BufferPool is not implemented. No architecture rule legitimately depends on that present implementation fact.

## Temporal-language classification

Classifications: A runtime ordering; B transaction/MVCC history; C format evolution; D durable v1 scope; E navigation; F project chronology/status.

| Section | Phrase | Class | Safe? | Correct owner/action | Finding |
|---|---|---:|---:|---|---:|
| 7.1 | “Later WAL/recovery chapters refine…” | E | Semantically yes, imprecise | Precise architecture references | E1 |
| 7.2 | “support later prefetch/writeback policy” | D | Yes | No change | No |
| 7.5 | “once the buffer layer exists” | F | No | Rewrite timelessly in ARCHITECTURE | M1 |
| 7.6–7.10 | “current” page/generation; “later retry” | A | Yes | Runtime protocol | No |
| 7.8 | “The initial implementation MAY…” | F | No | Timeless implementation freedom; sequencing belongs in DEVELOPMENT | M2 |
| 7.8 | “allow later partitioning” | F | No | State partitionability as present architecture property | M2 |
| 7.12 | replacement alternatives may be measured later | D | Yes | Durable v1 scope/alternative-policy boundary | No |
| 7.12.1–7.12.2 | current waiters/later residency | A | Yes | Runtime ordering | No |
| 7.12.3 | waiting API “may be added later” | F | No | State optional API timelessly | M3 |
| 7.13 | current waiter/current generation | A | Yes | Runtime invariant | No |

## Document ownership

| Section | Content | Current location | Correct owner | Finding |
|---|---|---|---|---:|
| 7.5 | Ordinary page access conditioned on buffer layer existence | Architecture | Architecture, rewritten as unconditional v1 rule | M1 |
| 7.8 | “Initial implementation” container/locking suggestion | Architecture | Timeless freedom in Architecture; sequencing/examples in Development if retained | M2 |
| 7.12.3 | API may be added later | Architecture | Architecture as an optional bounded API capability | M3 |
| 7.1 | Vague later-owner navigation | Architecture | Architecture with precise section references | E1 |
| 7.12.7 | Error-category summary | Architecture | Architecture; align with Chapter 4 taxonomy | M4 |

No VERIFICATION-, PROJECT_STATE-, devlog-, README-, AGENTS-, or source-layout material otherwise leaked into Chapter 7.

## Analytical depth

| Mechanism | Rule clear? | Rationale present? | Misuse risk | Assessment |
|---|---:|---:|---|---|
| Mandatory BufferPool access | Yes | Yes | Raw I/O bypasses lifetime/validation | Analytically sufficient |
| Raw-I/O exceptions | Yes across architecture | Yes | Unvalidated or incoherent page state | Analytically sufficient |
| Pin | Yes | Yes | Eviction while referenced | Analytically sufficient |
| Latch | Yes | Yes | Concurrent byte corruption | Analytically sufficient |
| PageGuard | Yes | Yes | Pin/latch leak or stale pointer | Analytically sufficient |
| Dirty state | Yes | Yes | Confusing dirty with commit/durability | Analytically sufficient |
| WAL-before-data | Yes | Yes | Unrecoverable data-page state | Analytically sufficient |
| Flush | Yes | Yes | False durability or lost dirty generation | Analytically sufficient |
| Eviction | Yes | Yes | Lost dirty page/stale identity | Analytically sufficient |
| Frame reassignment | Yes | Yes | Wrong-page access | Analytically sufficient |
| Retirement | Yes | Yes | Fetch/unlink/use-after-close race | Analytically sufficient |

## Terminology

| Term | Canonical Chapter 7 meaning | Ambiguity result |
|---|---|---|
| Page/PageId | Persistent logical page identity `(FileId, PageNo)` | Clear |
| Frame | Process-local BufferPool storage slot | Clear |
| Resident | Validated frame ordinarily published for one PageId | Clear |
| Cached | Not used as a substitute for the formal resident state | Clear |
| Pinned | Outstanding caller lifetime claim preventing eviction | Clear |
| `pin_count` | Checked number of outstanding pin claims | Clear |
| Latched | Page bytes protected by shared/exclusive internal latch | Clear |
| Dirty | Published generation not known stable in page file | Clear |
| Flush | Stable writeback without ending identity | Clear |
| Evict | End frame’s PageId residency after required writeback | Clear |
| Victim | Atomically reserved eligible frame | Clear |
| PageGuard | Single-owner scoped pin/latch capability | Clear |
| PageFile/registered owner | Typed file identity and validation owner | Clear |
| BufferPool | Resident-page lifetime/writeback/replacement owner | Clear |
| Published page | Reachable under owning persistent bound/protocol | Clear |
| Raw I/O | DiskManager positional file operation | Clear |
| Recovery-private | Bounded nonordinary reconstruction state | Clear |

Normative strength is generally appropriate:

| Area | Normative result |
|---|---|
| Ordinary BufferPool access | Correct contract exists, weakened stylistically by M1 |
| Pin/eviction | Strong MUST/MUST NOT invariants |
| Guard release | Exact latch-before-pin order |
| Validation | Required before resident publication |
| WAL-before-data | Explicit central mandatory gate |
| Dirty reconciliation | Exact identity/generation condition |
| Retirement | Exact gate/drain/unlink ordering |
| Raw exceptions | Narrowly owned by other canonical protocols |
| Replacement | CLOCK is deliberately fixed for v1 |
| Implementation internals | Container/mutex details remain free, despite M2’s project-stage phrasing |

No source-directory, `.cpp`, `.h`, source TODO, fixed frame count, concrete hash-table type, or mandatory mutex-class coupling was found.

# Explicit cross-reference audit

Grouped repeated references retain every distinct target used by Chapter 7.

| Source | Target(s) | Purpose | Exists/owner/precision | Status |
|---|---|---|---|---|
| 7.1 | “Later WAL/recovery chapters” | Metadata/flush refinement | Owners exist but reference is vague | E1 |
| 7.3–7.3.1 | §4.7 | Managed namespace lifecycle | Exists; correct owner | Good |
| 7.4.4 | §4.11 | Append-first allocation | Exists; correct owner | Good |
| 7.4.6 | §4.3.2.3 | Exact physical bound | Exists; precise | Good |
| 7.4.8 | §§4.7, 4.7.2 | File and namespace durability | Exists; precise | Good |
| 7.6.1 | §7.10.2 | Resident writeback state | Local precise owner | Good |
| 7.6.1 | §12.12 | Provisional mutation rollback/publication | Exists; precise |
| 7.6.1 | §4.3.2.5 | Runtime generation exhaustion | Exists; precise | Good |
| 7.6.1 | Chapters 12–13 | Recovery metadata | Exists; broad but appropriate summary | Good |
| 7.6.2 | §§7.8, 7.10.1, 7.12.4–7.12.5 | Mapping/mutation/new-page/retirement transitions | Local precise owners | Good |
| 7.6.2 | §12.12 | Mutation failure/noncontinuable behavior | Exists; precise | Good |
| 7.6.3 | §§7.6.4, 7.12.4 | Validation and private new-page exception | Local precise owners | Good |
| 7.6.4 | §§4.13, 4.13.1 | L0/L1/L2 validation | Exists; precise | Good |
| 7.6.5 | §§7.10–7.11; Chapters 12–13 | Durable state versus runtime state | Correct owners | Good |
| 7.7 | §§7.6.3, 7.10.1 | Pin linearization and mutation publication | Local precise owners | Good |
| 7.10 | §§12.10, 12.10.3, 12.10.5, 12.12, 12.16 | MTR, status, rollback, rec_lsn/FPI | Exists; precise | Good |
| 7.10.3 | §4.12.2 | Checksum finalization | Exists; precise | Good |
| 7.10.4 | §39.1 | Higher-level failure effect | Exists; precise enough | Good |
| 7.10.5 | §§12.10, 12.10.5, 12.12, 13.5 | DPT/checkpoint synchronization | Exists; correct owners | Good |
| 7.11 | §§12.17, 12.10.2, 7.10.3–7.10.5 | WAL gate/no-flush/reconciliation | Exists; precise | Good |
| 7.12–7.12.1 | §§7.9, 7.10–7.11 | Victim eligibility/writeback | Local precise owners | Good |
| 7.12.4 | §§4.11.1, 4.11.1.1, 4.11.3, 12.12.4 | New-page publication/failure/crash | Exists; precise | Good |
| 7.12.5 | §4.7.7 | File retirement/unlink | Exists; precise | Good |
| 7.12.6 | §3.3 | Database shutdown order | Exists; precise | Good |
| 7.12.7 | §§12.12.4, 39.1.3, 39.1.5–39.1.6 | Noncontinuable and statement/terminal results | Exists; precise | Good |
| 7.13 | §4.7 | Durable namespace invariant | Exists; precise | Good |

# High-priority consistency matrices

## 60 technical items

| # | Item | Result |
|---:|---|---|
| 1 | Chapter 7 ownership boundary | CONSISTENT |
| 2 | DiskManager role | CONSISTENT |
| 3 | PageFile role | CONSISTENT BUT SPECIALIZED |
| 4 | PageFile/DiskManager lifetime | CONSISTENT |
| 5 | Ordinary BufferPool mandate | FINDING — wording only |
| 6 | Raw-I/O exception set | CONSISTENT BUT SPECIALIZED |
| 7 | BufferPool key | CONSISTENT |
| 8 | One-resident-copy invariant | CONSISTENT |
| 9 | Frame identity semantics | CONSISTENT |
| 10 | Frame reassignment | CONSISTENT |
| 11 | Pin definition | CONSISTENT |
| 12 | Pin-count safety | CONSISTENT |
| 13 | Latch definition | CONSISTENT |
| 14 | Pin/latch distinction | CONSISTENT |
| 15 | Transaction-lock distinction | CONSISTENT |
| 16 | PageGuard lifetime | CONSISTENT |
| 17 | Guard release ordering | CONSISTENT |
| 18 | Read/write guard distinction | CONSISTENT |
| 19 | Dirty definition | CONSISTENT |
| 20 | Dirty race | CONSISTENT |
| 21 | Flush definition | CONSISTENT |
| 22 | FlushAll semantics | N/A — no public operation defined |
| 23 | WAL-before-data | CONSISTENT |
| 24 | page_lsn trust | CONSISTENT |
| 25 | Checksum writeback | CONSISTENT |
| 26 | Fetch hit | CONSISTENT |
| 27 | Fetch miss | CONSISTENT |
| 28 | Concurrent same-page miss | CONSISTENT |
| 29 | Fetch failure cleanup | CONSISTENT |
| 30 | New-page role | CONSISTENT |
| 31 | PAGE_INIT relationship | CONSISTENT |
| 32 | Unpublished new-page handling | CONSISTENT |
| 33 | Replacement eligibility | CONSISTENT |
| 34 | Replacement-policy freedom | CONSISTENT — CLOCK fixed for v1, revisions permitted |
| 35 | No-victim behavior | CONSISTENT |
| 36 | Clean eviction | CONSISTENT |
| 37 | Dirty eviction | CONSISTENT |
| 38 | Eviction failure | CONSISTENT |
| 39 | Flush versus eviction | CONSISTENT |
| 40 | Runtime page table | CONSISTENT |
| 41 | Stale-handle safety | CONSISTENT |
| 42 | Frame generation | CONSISTENT BUT SPECIALIZED |
| 43 | File retirement | CONSISTENT |
| 44 | Dirty-page retirement | CONSISTENT |
| 45 | Shutdown behavior | CONSISTENT |
| 46 | Shutdown flush failure | CONSISTENT |
| 47 | Recovery access | CONSISTENT BUT SPECIALIZED |
| 48 | Bootstrap access | CONSISTENT BUT SPECIALIZED |
| 49 | Control/WAL exclusions | CONSISTENT BUT SPECIALIZED |
| 50 | Typed-page consumers | CONSISTENT |
| 51 | Validation before residency | CONSISTENT |
| 52 | Owner validation | CONSISTENT |
| 53 | Writeback validity | CONSISTENT |
| 54 | Latch/I/O consistency | CONSISTENT |
| 55 | Deadlock/order compatibility | CONSISTENT |
| 56 | Flush pinned page | CONSISTENT |
| 57 | Evict pinned page | CONSISTENT |
| 58 | COMMIT versus page flush | CONSISTENT |
| 59 | Buffer exhaustion taxonomy | CONSISTENT |
| 60 | Implementation freedom | CONSISTENT, with M2/M3 wording findings |

## 20 documentation items

| # | Item | Result |
|---:|---|---|
| 1 | Timeless wording | FINDING |
| 2 | Known §7.5 phrase | FINDING |
| 3 | No current-project-state narration | FINDING — implied stage contingency, not direct availability statement |
| 4 | No Phase-2 narration | CONSISTENT |
| 5 | No implementation sequencing | FINDING |
| 6 | No verification-procedure leakage | CONSISTENT |
| 7 | No devlog/history leakage | CONSISTENT |
| 8 | No source-layout coupling | CONSISTENT |
| 9 | No temporary implementation choice promoted | FINDING |
| 10 | Precise BufferPool terminology | CONSISTENT |
| 11 | Precise pin/latch terminology | CONSISTENT |
| 12 | Precise dirty/durable terminology | CONSISTENT |
| 13 | Precise flush/evict terminology | CONSISTENT |
| 14 | Precise ownership cross-references | FINDING — E1 |
| 15 | Rationale for mandatory BufferPool access | CONSISTENT |
| 16 | Rationale for pin/latch split | CONSISTENT |
| 17 | Rationale for WAL-before-data | CONSISTENT |
| 18 | Rationale for guard lifetime | CONSISTENT |
| 19 | Rationale for raw-I/O exceptions | CONSISTENT |
| 20 | Readable without implementation status | CONSISTENT, but temporal blemishes remain |

# Findings

## Blocking findings

None.

## Major findings

None.

## Minor findings

### M1 — §7.5 project-stage BufferPool wording

- Evidence: [§7.5](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:4595): “Normal page-format objects operate over bytes supplied through BufferPool-managed lifetime **once the buffer layer exists**.”
- Severity: MINOR
- Type: TEMPORALITY
- Scope: Local, with Chapter 2 and consumer-chapter confirmation.
- Explanation: The phrase conditions a mandatory v1 architectural boundary on project implementation progress.
- Canonical comparison: §2.5 and Chapter 7’s own invariants establish BufferPool as the ordinary resident-page path.
- Consequence: A reader could incorrectly infer that ordinary direct page-byte ownership remains architecturally acceptable in a “pre-buffer” v1 mode.
- Correct owner: ARCHITECTURE, stated timelessly. Current implementation absence remains PROJECT_STATE.
- Future action: **B. TIMELESSNESS REWRITE**.

Correct concept: normal page-format objects operate over bytes supplied through BufferPool-managed lifetime; typed page objects remain non-owning views.

### M2 — §7.8 implementation-sequencing language

- Evidence: [§7.8](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:4829): “The **initial implementation** MAY realize this logical table with conventional hash maps/condition variables protected by mutexes… The abstraction MUST allow **later partitioning**…”
- Severity: MINOR
- Type: TEMPORALITY
- Scope: Local/cross-document.
- Explanation: The invariant—container freedom plus partitionable semantics—is valid architecture. “Initial implementation” and “later partitioning” describe project sequence.
- Canonical comparison: Chapter 2 already requires a parallel-ready, partitionable design without pinning one container.
- Consequence: Architecture becomes dependent on implementation maturity and promotes one development-stage mechanism.
- Correct owner: Timeless implementation freedom remains in ARCHITECTURE; sequencing examples belong in DEVELOPMENT.
- Future action: **B. TIMELESSNESS REWRITE**.

### M3 — §7.12.3 roadmap wording

- Evidence: [§7.12.3](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:5107): “A separately named cancellable/waiting convenience API **may be added later**…”
- Severity: MINOR
- Type: TEMPORALITY
- Scope: Local.
- Explanation: Whether a bounded waiting API is permitted is an architectural scope statement, not a roadmap event.
- Canonical comparison: The durable rule is that any such API cannot weaken victim eligibility or make the ordinary load wait indefinitely.
- Consequence: Introduces unnecessary project chronology into an otherwise timeless resource-exhaustion contract.
- Correct owner: ARCHITECTURE as a timeless optional capability.
- Future action: **B. TIMELESSNESS REWRITE**.

### M4 — §7.12.7 incomplete format-error summary

- Evidence: [§7.12.7](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:5145) lists `CORRUPT_PAGE` but omits the canonical unsupported-format outcomes.
- Severity: MINOR
- Type: SEMANTIC COMPLETENESS
- Scope: Cross-section with §4.14.
- Explanation: Chapter 7 requires version/family dispatch before v1 parsing, but its “BufferPool error categories” summary does not include `UNSUPPORTED_PAGE_FORMAT` or applicable `UNSUPPORTED_FILE_FORMAT`.
- Canonical comparison: [§4.14](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:2534) distinguishes malformed v1 corruption from recognizable future unsupported formats.
- Consequence: An implementer using the local summary could collapse unsupported format into corruption despite the canonical Chapter 4 taxonomy.
- Correct owner: ARCHITECTURE’s Chapter 7 result summary, by precise cross-reference or inclusion without redefining Chapter 4.
- Future action: **A. LOCAL WORDING FIX**.

This is recoverable from Chapter 4 and therefore is not a frozen semantic question.

## Editorial finding

### E1 — §7.1 vague WAL/recovery reference

- Evidence: [§7.1](/home/yghtso/Projects/DBlusBlus/docs/ARCHITECTURE.md:4392): “Later WAL/recovery chapters refine the buffer metadata and flush protocol…”
- Severity: EDITORIAL
- Type: CROSS-REFERENCE
- Scope: Cross-section.
- Explanation: Stable precise owners exist.
- Canonical comparison: Relevant owners include §§12.10, 12.12, 12.16–12.17 and §§13.13–13.14.
- Consequence: Navigation is less precise, but no semantic ambiguity results.
- Correct owner: ARCHITECTURE navigation.
- Future action: **G. CROSS-REFERENCE FIX**.

# Verification coverage

Classification: **FOLLOW-UP VERIFICATION GAP**

Existing VERIFICATION coverage owns broad BufferPool themes—eviction, pin protection, guard modes, dirty writeback, CLOCK, WAL/PAGE_INIT, shutdown, and consumer specializations—but does not yet provide deterministic procedures for every Chapter 7 concurrency and failure boundary.

Exact follow-up gaps:

1. Complete `FREE`/`LOADING`/`RESIDENT`/`EVICTING` transition coverage.
2. Same-page miss coalescing, joiner cancellation, loader failure wakeup, and clean retry.
3. Fetch-versus-victim-reservation linearization.
4. Pin overflow, canceled latch acquisition, move/early guard release, and stale borrowed-reference rejection.
5. Copied-flush generation races, including pinned flush and explicit join/repeat versus background skip/requeue.
6. Stable completion requiring both exact write and `fdatasync`, including dirty-clear publication.
7. Dirty-eviction failure restoring the old mapping and failing the requesting load/waiters.
8. Complete frame-metadata reset and nonrepeating identity/generation behavior on reassignment.
9. Exact one-pass `NO_REPLACEABLE_FRAME` behavior.
10. Cross-family validation-before-residency and unsupported-format classification for HEAP, FSM, BTREE, catalog, and status pages.
11. BufferPool-specific new-page co-publication and pre-/post-WAL append frame cleanup.
12. ACTIVE→RETIRING→CLOSED races with loads, pins, writeback, and authorized dirty discard.
13. Bounded bootstrap, FileSuperblock/page-zero, WAL/control, and recovery-private raw-I/O exceptions.

Shutdown’s broad lifecycle procedure and the Chapter 6 FSM specialization are already covered; this gap is specifically Chapter 7 synchronization.

# Direct invention and ambiguity answers

| Question | Result |
|---|---|
| Layer-ownership contradiction? | No |
| DiskManager/PageFile lifetime ambiguity? | No |
| Ordinary BufferPool-access ambiguity? | No semantic gap; M1 is a temporal wording defect |
| Raw-I/O exception ambiguity? | No |
| Duplicate-resident-page ambiguity? | No |
| Pin/latch semantic ambiguity? | No |
| PageGuard lifetime ambiguity? | No |
| Stale-frame-handle hazard left open? | No |
| Dirty-state ambiguity? | No |
| Flush/durability ambiguity? | No |
| WAL-before-data gap? | No |
| Flush-versus-mutation race ambiguity? | No |
| Fetch/publication ambiguity? | No |
| Failed-fetch cleanup ambiguity? | No |
| Eviction eligibility ambiguity? | No |
| Dirty-eviction failure ambiguity? | No |
| Validation-before-residency gap? | No |
| Retirement/resident-page ambiguity? | No |
| Shutdown/storage ambiguity? | No |
| Correctness-relevant implementer invention required? | No |
| Project-time/current-state wording? | Yes: M1–M3 |
| DEVELOPMENT-owned material? | Yes, localized sequencing aspect of M2/M3 |
| VERIFICATION-owned procedure in Chapter 7? | No |
| PROJECT_STATE-owned material in Chapter 7? | No direct state claim; M1 implies project contingency |
| Devlog/history material? | No |
| Ambiguous terminology? | No correctness-relevant ambiguity |
| Analytically underexplained boundary? | No |
| Can Chapter 7 stand unchanged as fully timeless? | No; targeted wording fixes are recommended |
| Can its technical contract be implemented without status knowledge? | Yes |

# Regression and next action

Previous-chapter regression result:

- Chapter 1 parallel-ready architecture: compatible.
- Chapter 2 provider/consumer and immutable metadata boundaries: compatible.
- Chapter 3 open/recovery/shutdown lifecycle: compatible.
- Chapter 4 page identity, validation, publication, checksum, and WAL foundations: compatible, subject to M4’s local summary correction.
- Chapter 5 guarded heap-page ownership: compatible.
- Chapter 6 FSM ordinary access, advisory semantics, PAGE_INIT, and WAL ownership: compatible.

Frozen architecture semantic questions: **NONE**.

Recommended next action:

1. **Targeted documentation edit** for M1–M4 and E1.
2. Then **Chapter 7 verification synchronization** for the listed deterministic-methodology gaps.
3. No architecture semantic review is required.

Recommended Chapter 8 review scope:

- Start at `# 8. B+ Tree Indexing` and review every subsection through the line before Chapter 9.
- Discover its live structure first.
- Emphasize its role as a Chapter 7 BufferPool/PageGuard consumer: page identity, typed validation, root publication, MTR/no-flush behavior, split/merge and sibling invariants, lock/latch order, uniqueness, recovery, retirement, and prohibition of private page caches.

# Final repository checks

| Check | Initial | Final |
|---|---|---|
| Working tree | Clean | Clean |
| Index | Clean | Clean |
| HEAD | `7f16c96d767b0c6a997ee64f2474ebb3ec6f93c6` | `7f16c96d767b0c6a997ee64f2474ebb3ec6f93c6` |
| `git diff --check` | N/A at start | Passed with no output |

Files modified by audit: **NONE**.

No repository state changed. No pre-existing material was modified, reverted, or staged. No review artifact or devlog was created. No build, test, benchmark, source audit, implementation, BufferPool/PageGuard/HeapFile work, or subsystem scaffolding occurred.

**Phase 2 remains NOT STARTED / NOT AUTHORIZED.**