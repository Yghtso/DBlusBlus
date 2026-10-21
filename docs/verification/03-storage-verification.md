# Storage Verification

At minimum:

## Slotted-page tests

- insert until full,
- basic heap-page mutation treats DEAD slots as non-reusable,
- NORMAL -> DEAD transitions and compaction preserve SlotIds,
- DEAD -> UNUSED transition and reusable-slot tests exercise the Chapter 14
  vacuum/reclamation protocol,
- ordinary owner validation enforces page identity, version, geometry, required-zero
  fields, and schema-directed tuple validity for every retained NORMAL and DEAD tuple,
- ordinary owner validation rejects overlapping NORMAL/NORMAL, NORMAL/DEAD, and DEAD/DEAD
  retained tuple ranges,
- retained DEAD slots receive complete tuple validation and use canonical retained or
  reclaimed coordinates with `aux=0`,
- `REDIRECT_RESERVED` is rejected by ordinary owner validation,
- compaction,
- invalid slot access,
- tuple bytes survive compaction.

## Tuple codec tests

- every scalar type,
- nulls,
- empty VARCHAR,
- long VARCHAR within inline limit,
- mixed fixed/varlen schemas,
- encode/decode round trip,
- unaligned field positions.

## Disk tests

- page zero/superblock,
- generic superblock encoding writes zero flags and decoding rejects every nonzero flag
  or required-zero reserved field,
- extend file,
- random page read/write,
- reopen persistence,
- short/error I/O handling where injectable.

## Buffer management verification

This section is the detailed procedural owner for the I/O and BufferPool contract in
[`ARCHITECTURE.md`](../ARCHITECTURE.md) Chapter 7. It specializes the generic checked-arithmetic,
page-format, owner-validation, PAGE_INIT, WAL/MTR, recovery, reclamation, and lifecycle
procedures rather than redefining them. The principal architecture owners are §§4.3.2,
4.7–4.14, 7.2–7.13, 12.10, 12.12, 12.16–12.17, 13.13–13.14, 14.5–14.12, 14.17, 39.1,
and 41.1.

Use pools substantially smaller than the working set, including a three-frame pool, while
retaining direct construction of synthetic runtime states for checked counter and failure
boundaries. Every fixture is canonical except for the single dimension being tested: page
size, format version, FileId, PageNo, FileKind, PageType, object owner, reserved fields,
checksum, and published bound are valid unless that field is the injected fault.

### Deterministic harness and observability

Concurrency verification uses hooks, barriers, injected outcomes, and coordinated threads or
processes. It MUST NOT use sleeps, repeated racing until a failure appears, or elapsed time as
the ordering oracle. The harness exposes abstract semantic events rather than requiring
production function names:

```text
load intent installed
frame bound LOADING
victim candidate observed
victim reservation attempted/completed
pin increment attempted/completed
latch acquisition attempted/completed/canceled
guard returned/released
stable image copied with PageId/generation/page_lsn
WAL durability requested/completed/failed
page pwrite started/completed/short/failed
page-file fdatasync started/completed/failed
dirty reconciliation started/completed
mapping marked non-pinnable/removed
frame reset/new PageId bound
PAGE_INIT publication-authorizing WAL appended
owning published bound advanced
frame published RESIDENT
file gate changed ACTIVE/RETIRING/CLOSED
retirement drain completed
```

At each point the harness can inspect the logical frame state, bound PageId, page-table load
intent and ordinary mapping, pin count, latch ownership/waiters, dirty flag and generation,
I/O/no-flush/victim/drain reservations, replacement reference state, `page_lsn`, `rec_lsn`,
DPT membership, WAL durable LSN, registered-file state, and persistent page-file completion.
Test-only poisoning or generation assertions may detect stale borrowed references; production
APIs need not expose one particular instrumentation mechanism.

### Frame lifecycle and publication

Exercise every row below from a controlled pre-state and pause at its publication point.
Assert the stated observable condition, race one forbidden operation at that point, and inject
each applicable failure before and after publication. This tests the matrix rather than using
it as a second architecture definition.

| State/transition case | Bound/mapped and caller-visible state | Deterministic operation and oracle |
|---|---|---|
| `FREE + NONE` | no PageId, no mapping, no pin, clean, not visible | bind only through a sole existing/new-page intent; stale metadata is absent |
| `FREE -> LOADING + READ_IN_PROGRESS` | PageId and in-progress entry, no public pin/guard | pause before read; another same-page fetch joins and no ordinary caller sees bytes |
| `FREE -> LOADING + NONE` for new page | private unpublished PageId, creator claim only | no ordinary fetch may join merely from the intended PageNo |
| existing-page `LOADING -> RESIDENT` | same mapping becomes pinnable only after full validation | publication assigns one pin per uncancelled claim and wakes waiters |
| new-page `LOADING -> RESIDENT` | owning bound and frame usability publish together | PAGE_INIT/MTR, canonical bytes, owner validation, bound, and creator pin are complete |
| failed `LOADING -> FREE` | failed intent removed; no ordinary mapping/pin | frame reset, all joiners receive one captured error, clean retry may install a new intent |
| `RESIDENT + NONE` | validated PageId, pinnable; dirty allowed | hit pin and persistent mutation follow their separate atomic publication points |
| `RESIDENT -> RESIDENT + WRITEBACK_IN_PROGRESS` | mapping remains usable; writeback copy is private | one writeback reservation; pins/readers/writers follow copied-writeback rules |
| resident writeback completion | same identity; dirty clears only on stable matching generation | newer generation or any failure leaves the frame dirty and coherent |
| `RESIDENT -> EVICTING` | old mapping present but non-pinnable; pin count zero | final eligibility recheck/reservation is the linearization point |
| clean `EVICTING -> FREE/LOADING` | old mapping removed before reset/rebind | no write; complete reset precedes any new PageId |
| dirty `EVICTING -> WRITEBACK_IN_PROGRESS` | old identity retained; no ordinary access | WAL-gated stable writeback precedes removal/rebind |
| dirty eviction success | old mapping removed; reset then FREE/new LOADING | exact write and file synchronization completed first |
| dirty eviction failure `-> RESIDENT` | old mapping pinnable again, old identity and dirty metadata retained | requesting load/joiners receive the same failure; frame is not rebound |
| retirement drain | RETIRING gate blocks new claims; existing ownership drains | mapping/reset/close/unlink cannot pass the drain or semantic discard authority |
| shutdown drain | quiescing rejects new acquisitions | existing guards/I/O drain and required dirty flush completes before teardown |

The corresponding state-condition matrix is exercised directly. For every row, inspect each
column before allowing its listed outgoing transition; a mismatch is a failed invariant, not
an alternate transition.

| State/condition | PageId bound? | Page-table entry | Caller pinnable? | Dirty permitted? | I/O state | Eviction eligible? | Ordinary visibility | Legal outgoing transitions |
|---|---:|---|---:|---:|---|---:|---|---|
| `FREE` | no | none | no | no | `NONE` | allocation candidate, not eviction victim | none | existing/new-page `LOADING` |
| existing-page `LOADING` | yes | in progress | no | no | `READ_IN_PROGRESS` | no | join coordination only | validated `RESIDENT`; failed `FREE` |
| private new-page `LOADING` | yes | private in progress | no | only protected unpublished initialization state | `NONE` | no | creator coordination only | co-published `RESIDENT`; failed `FREE`/noncontinuable disposition |
| `RESIDENT + NONE` | yes | usable | yes if file active and count representable | clean or dirty | `NONE` | only if every §7.9 predicate holds | guarded callers | hit/mutation; resident writeback; `EVICTING`; retirement drain |
| `RESIDENT + WRITEBACK_IN_PROGRESS` | yes | usable | yes | dirty until matching stable reconciliation | `WRITEBACK_IN_PROGRESS` | no | guarded callers; private copy hidden | `RESIDENT + NONE` clean, newer-dirty, or failed-dirty |
| clean `EVICTING + NONE` | yes | non-pinnable old mapping | no | no | `NONE` | already reserved | none | `FREE`; reset then new `LOADING` |
| dirty `EVICTING + NONE` | yes | non-pinnable old mapping | no | yes | `NONE` | already reserved | none | eviction `WRITEBACK_IN_PROGRESS` |
| `EVICTING + WRITEBACK_IN_PROGRESS` | yes | non-pinnable old mapping | no | yes until success | `WRITEBACK_IN_PROGRESS` | already reserved | none | `FREE`/new `LOADING` after success; restored `RESIDENT` after failure |
| file-gated `RETIRING` frame | yes until drain/reset | existing non-new-pinnable mapping | no new pin | according to required persistence/discard authority | `NONE` or draining I/O | no | pre-gate guards only | drained/reset, then file `CLOSED`; failure remains RETIRING |
| file `CLOSED` | no | none | no | no | none | no | none | no ordinary frame transition for the old FileId |

Negative transition tests reject direct `FREE ->` caller-visible `RESIDENT`, ordinary guard
return from `LOADING`, pinned or otherwise ineligible `RESIDENT -> EVICTING`, dirty eviction
rebind before stable writeback, and old-to-new PageId binding without mapping removal and
complete reset. `FREE` has only `NONE`; `LOADING`, `RESIDENT`, and `EVICTING` accept only the
I/O combinations in §7.6.1. No test-only state setter may make an illegal combination look
like a successful public operation.

Distinguish persistent-page publication from frame publication. An existing PageNo may be
persistently published while its frame remains private `LOADING`; a private new-page frame
may exist while its PageNo is unpublished. A load intent is visible to joiners but is not an
ordinary pinnable mapping. Tests observe these dimensions separately.

### Same-page fetch, victim races, and failure cleanup

Coordinate at least two fetches for one valid nonresident PageId. Pause after the sole
`LOAD_INTENT` installation and again after the frame becomes `LOADING`. Assert exactly one
logical loader/read, one eventual ordinary resident identity, all joiners attached to that
load, and exactly one pin for every successfully returned guard. Duplicate mutable ordinary
copies are forbidden regardless of page-table container.

Inject raw read failure, short transfer, bad checksum, wrong owner, and unsupported format
while joiners wait. The loader removes/closes the in-progress mapping, resets the frame,
publishes no guard/pin, and wakes every registered joiner with the corresponding captured
error. Correct the underlying fixture and assert that a later independent fetch installs a
new intent and succeeds; failed pages are not poison-cached.

If a permitted bounded/cancellable waiting interface exists, cancel one joiner before
publication and another immediately after claim-to-pin assignment. The first withdraws only
its pending claim; the second releases only its assigned pin. Neither cancels the loader,
removes a successful mapping, changes another caller's pin, nor leaks waiter state. This case
is conditional and does not require such an optional interface.

For fetch versus eviction, prepare an eligible zero-pin resident frame and pause before the
final pin/victim reservation. If fetch wins, its pin makes victim reservation fail or retry.
If eviction wins, the mapping becomes non-pinnable and fetch cannot acquire the old frame; it
waits/retries according to §7.6.3. A pin acquired after reassignment begins is forbidden.

### Pin, latch, guard, and borrowed-reference lifecycle

Use synthetic checked-counter states immediately below and at the pin maximum. The legal
increment succeeds, an increment beyond the maximum fails without changing the count or
PageId, and no wrap to zero occurs. Pair this with one-release-per-owner cases: normal
destruction, explicit early release followed by destruction, transferred ownership,
canceled latch acquisition after pinning, and attempted double release. Assert no underflow,
no duplicate unpin, and no leaked pin.

For canceled latch acquisition, pause after the pin and fail/cancel before latch ownership.
Exactly that pin is released, no latch or guard survives, and the mapped frame remains valid.
For guard transfer, the destination owns the one pin/latch claim and the source is inert;
destination release performs latch release before checked unpin exactly once. During early
release, pause between those events and prove eviction cannot win while the guard still
depends on protected bytes.

Exercise shared read guards, exclusive write guards, and transaction-lock separation.
Concurrent readers may proceed; writers exclude byte readers/writers; a transaction-level
wait retains no page latch. Pinning alone does not permit unsynchronized byte access and a
latch alone does not preserve frame identity.

To verify borrowed-reference lifetime, obtain a raw/typed view of page A, release its guard,
evict/reassign the frame to B, and attempt stale use through a debug poison, frame token, or
equivalent test facility. The stale view must not be accepted as a valid reference to B.
This is a contract-violation detector, not a required production handle design.

### Copied stable flush, WAL, and dirty reconciliation

For a dirty resident frame, reserve one writeback, copy a stable 8192-byte image under the
read latch, and record copied PageId, `modification_generation`, and `page_lsn`. Release the
latch, finalize the private checksum, satisfy WAL-before-data, perform an exact complete page
write and required `fdatasync`, then pause at reconciliation. If identity and generation are
unchanged, dirty/`rec_lsn`/DPT state may publish clean atomically; flush preserves the resident
PageId and mapping.

After copying generation G, pause before its physical write and again before
reconciliation. Start a WAL-backed mutation and observe its attempted no-flush
acquisition: it must wait without changing bytes or published metadata. Release
successful writeback reconciliation, then permit the mutation to publish G+1;
assert its dirty flag and recovery metadata describe that new dirty interval.
If writeback fails, the waiting mutation returns unchanged, as required by
§12.12.3. The reverse reservation order is covered by the provisional-mutation
procedure below. A writer that was never scheduled to acquire ownership does not
activate this test.

Separately exercise the defensive completion predicate with an injected stale G
token against the coherent G+1 frame. Compare exact bytes, dirty flag, generation,
`rec_lsn`, DPT membership and active reservation identity before and after:
stale completion must not clear or alter the newer state. Inject page A's token
against a later identity B as the identity-negative case. These are deliberate
stale-token faults, not permitted production schedules for bypassing no-flush
ownership. A matching current token after stable write/sync is the positive
control; only it may publish clean. A later ordinary flush can persist G+1.

Keep a page pinned while flushing it. Assert no eviction, a coherent private image,
WAL-before-data, preserved resident identity, and dirty clearing only for a matching
generation. Pinning is not an immutability claim.

When explicit flush requests overlap, accept join-and-recheck, serialized repeat, or an
equivalent mechanism. The current-contents request returns success only when the then-current
generation is stably clean. If optional background writeback exists, it uses the same copied
generation/WAL rules and may skip or requeue busy/no-flush frames; no background worker is a
required feature and it cannot make a pinned frame evictable.

Specialize the generic Disk tests with zero-byte failure, short write, and exact 8192-byte
write outcomes. Only the complete transfer may proceed toward stable completion. After a
successful full `pwrite`, inject `fdatasync` failure: dirty and all recovery metadata remain,
success/eviction is not published, and the frame retains the operation-appropriate identity.
Where writes are batched, no covered frame becomes clean or evictable before the covering
`fdatasync` succeeds.

For WAL-backed pages, inject `flush_through(copied_page_lsn)` failure and prove data-page
`pwrite` never begins. Dirty remains and the caller receives `WAL_DURABILITY_FAILURE`.
Corrupt the resident page after checksum validation only through an explicit test fault; the
write path must checksum the stable private image, not changing resident bytes. The existing
§12.10/§12.12 tests own no-flush and DPT/checkpoint publication races; add a BufferPool
observer proving checkpoint sees either the complete dirty transition or a safely durable
clean page, never an intermediate state.

### CLOCK, eviction, failure restoration, and reset

Table-test every victim-eligibility predicate: `RESIDENT`, zero pins, I/O `NONE`, no latch
owner/waiter, no no-flush barrier, no competing victim/drain reservation, and a successful
final reservation. For CLOCK reference behavior, a successful hit/load sets the use bit; an
eligible referenced frame receives the defined second chance; an eligible unreferenced frame
may be reserved. `FREE`, `LOADING`, `EVICTING`, pinned, and reserved frames do not become
victims through the reference-bit rule.

Construct separate pools in which every frame is excluded by pins, I/O, latch ownership or
waiters, no-flush state, and victim/drain reservation. Count logical CLOCK visits and
eligibility decisions rather than time. One complete unsuccessful ordinary pass returns
`NO_REPLACEABLE_FRAME`, changes no pins/states, steals no frame, and does not wait
indefinitely. A conditional bounded/cancellable convenience path cannot weaken these
predicates.

For clean eviction, assert non-pinnable reservation, old-mapping removal, complete reset,
then optional new binding, with no data write. For dirty eviction, assert WAL durability,
exact write, `fdatasync`, mapping removal/reset, then new binding. Independently inject WAL,
write, short-write, and sync failure: restore the old pinnable `RESIDENT` mapping, preserve
dirty/`rec_lsn`/generation/DPT state, release the reservation, fail the requesting load and
its joiners, and permit a later retry. No alternative victim hides that failure.

After successful eviction or failed loading cleanup, inspect all architecture-significant
state: PageId, pins, dirty flag, cached/trusted `page_lsn` state where separately retained,
`rec_lsn`/DPT/FPI metadata, I/O and no-flush state, latch/waiter state, replacement bit,
victim/drain claims, and generation/identity tokens. Nothing owned by the old PageId may
influence the new binding. Dirty/writeback generation follows §7.6.1; inject
an old asynchronous completion and prove it cannot match a later residency. Apply the
runtime-generation terminal/quiesce procedure under “Encoded, structural, generation, and
epoch specializations” rather than brute-force wrapping.

Pair `NO_REPLACEABLE_FRAME` against disk `RESOURCE_FULL`, `PAGE_NUMBER_EXHAUSTED`,
`WAL_POSITION_EXHAUSTED`, identifier exhaustion, heap `NO_SPACE`, corruption, retirement,
and `BUFFERPOOL_QUIESCING`. Each fixture reaches only its owning domain and no result is
reported as generic “buffer full.”

### Validation before residency

Parameterize ordinary load validation over the managed families applicable to the
page registry:

| Family/context | Owning specialization |
|---|---|
| `HEAP_DATA`, including catalog-relation heaps | §§4.13.3, 5.3–5.13, and immutable relation descriptor |
| `FSM_DATA` | §§4.13.6 and Chapter 6 FSM verification |
| `BTREE_INTERNAL`, `BTREE_LEAF`, `BTREE_FREE` | §§4.13.4–4.13.5 and B+ verification |
| `TXN_STATUS` | §§9.12, 12.10.5, and recovery/status ownership |
| `CATALOG_DATA` bootstrap page | §16.9 specialized bootstrap/open path; ordinary publication only where that owner registers it |
| `SUPERBLOCK`/page zero | specialized open/identity path below, not assumed to be an ordinary data-page fetch |

For each applicable ordinary family, start with canonical bytes and owner context. Observe
registered-owner lookup, published-bound check, exact read, family/version dispatch,
checksum, common PageId identity, FileKind/PageType compatibility, nonfetching L1/L2 owner
validation, `LOADING -> RESIDENT`, then guard return. Fault each earlier step separately and
assert no later ordinary publication, no guard/pin, complete load cleanup, one error for all
joiners, and successful retry after correcting the fixture.

Cases include bad checksum, wrong PageNo, wrong registered FileId/owner, wrong FileKind,
wrong PageType, and family-local structural corruption. A plausible `page_lsn` behind a bad
checksum must not be trusted for WAL/recovery decisions. For every applicable family pair a
malformed recognized-v1 fixture with a recognizable future version: the first yields its
canonical corruption result; the second yields `UNSUPPORTED_PAGE_FORMAT` or
`UNSUPPORTED_FILE_FORMAT`. Dispatch must not parse future bytes as v1.

### BufferPool new-page publication

Pause after append intent reservation and private `LOADING` frame binding. The selected
PageNo remains outside the owning published bound, has no ordinary mapping or guard, and an
ordinary concurrent fetch cannot attach merely by guessing it. The private frame is not
evidence of persistent publication.

Inject every known pre-publication-authorizing-WAL failure through the generic PAGE_INIT
procedure. Assert private mapping/frame cleanup, unchanged published bound, no escaping
guard/reference, serialized tail restoration, and deterministic PageNo reuse where §4.11.1.1
permits it. After the publication-authorizing record validly appends, inject failure before
complete bound/frame publication and require retained completion/retry or
`STORAGE_NONCONTINUABLE`; do not roll back and reuse an authorized PageNo.

On success, observe one publication boundary that has canonical initialized bytes,
PAGE_INIT/MTR and frame recovery metadata, owner validation, owning `published_page_count`,
ordinary `RESIDENT` mapping, and exactly one creator pin. Before it, neither scans nor fetches
can use the page; after it, the bound includes the PageNo and the mapping is usable. Repeat
with a concurrent ordinary fetch paused on each side.

Specialize PAGE_INIT crash tests by asserting all frame-table, pin, latch, waiter, and CLOCK
state disappears on process death. Recovery uses only WAL/file publication state and admits
the page ordinarily only after reconstruction and canonical validation.

### File retirement and shutdown specialization

Expose `ACTIVE -> RETIRING -> CLOSED` at the registered-file/BufferPool boundary. Race a new
load/pin with the RETIRING gate: a claim linearized before the gate drains as existing
ownership; a gate winner rejects the new operation with `FILE_RETIRED_OR_CLOSING`. No claim
is admitted after the gate.

Hold read/write guards across the transition. They remain valid until release, while no new
guard is admitted and close/unlink waits. Race copied writeback or victim I/O with retirement;
the I/O completes or fails before handle close, and all reservations drain before mapping
removal. A descriptor or fd must never be closed while in-flight I/O still owns it.

Test dirty discard in two matched fixtures. A proven semantic drop/retirement owner may
authorize discard after drain; RETIRING alone and generic eviction do not. Without that
authority, required dirty state is preserved/failed rather than discarded. Inject drain or
writeback failure and assert the file remains RETIRING/nonordinary, no unlink or false CLOSED
publication occurs, and higher-level failure handling receives the error. Successful CLOSED
state has no mapping, frame, pin, guard, or I/O for the FileId; nonreuse prevents an old
PageId from binding a new file.

The existing “Shutdown, draining, and failure injection” procedure owns the complete
database lifecycle. Its Chapter-7 specialization asserts BufferPool quiescing rejects new
fetch/new-page work, existing guards/I/O drain, required dirty pages use WAL-before-data,
required flush failure prevents clean shutdown, and BufferPool helpers/ownership end before
the WAL service.

### Bounded raw-I/O exceptions

Inventory every permitted BufferPool bypass and instrument capability use so an unlisted
ordinary page operation cannot issue raw managed-page I/O:

| Exception owner | Permitted object and reason | Validation/termination oracle |
|---|---|---|
| WAL manager | WAL segments use the WAL record/durability protocol | never interpreted as BufferPool pages; exception ends at WalManager boundary |
| database lifecycle/recovery owner | `database.control` dual-slot lifecycle state | control validation precedes use; no PageGuard/CLOCK semantics are applied |
| registered-file/open owner | initial FileSuperblock/page-zero identity establishment | bounded raw read is validated before registration; it is not a permanent page-zero escape |
| bootstrap/create owner | private initial object/page construction before ordinary publication | canonical validation/publication completes before ordinary BufferPool use |
| recovery owner | torn-page private reconstruction | untrusted bytes remain private; reconstructed page validates before READY/residency |
| namespace owner/DiskManager | create/rename/unlink and directory synchronization | no page interpretation; exception ends at durable §4.7 namespace boundary |

Record every direct DiskManager page read/write in ordinary HEAP/FSM/BTREE/catalog/status
operations and require it to originate from BufferPool or one named private owner. The
existence of WAL/control/bootstrap/recovery/namespace paths is not a generic escape hatch.
Verify page-zero access according to its actual phase: identity-establishing access may be
specialized before registration, while registered ordinary data-page access returns to the
canonical BufferPool boundary.

For recovery, start from a torn page requiring a complete image. Ordinary load rejects it;
the private recovery owner may reconstruct without ordinary publication, but the resulting
page passes normal checksum, identity, format, and owner validation before READY or guard
return. Bootstrap follows the same private-construction-to-canonical-publication shape and
must never be described or tested as an implementation-stage absence of BufferPool.

### Chapter 7 failure-classification matrix

| Fixture/outcome | Required result and BufferPool oracle |
|---|---|
| invalid or unpublished PageId | `FILE_OR_PAGE_NOT_FOUND`; no data-page publication |
| RETIRING/CLOSED owner | `FILE_RETIRED_OR_CLOSING`; no new claim |
| failed/short transfer | `RAW_IO_FAILURE`; partial bytes never become resident/stably clean |
| malformed recognized v1 / wrong owner | applicable corruption result, including `CORRUPT_PAGE`; no guard |
| recognizable unsupported page/file version | `UNSUPPORTED_PAGE_FORMAT` / `UNSUPPORTED_FILE_FORMAT` |
| WAL durability failure | `WAL_DURABILITY_FAILURE`; no dependent data-page write |
| page write or `fdatasync` failure | `RAW_IO_FAILURE`; dirty and identity retained |
| complete CLOCK pass without victim | `NO_REPLACEABLE_FRAME`; no waiting or stolen frame |
| quiescing BufferPool | `BUFFERPOOL_QUIESCING`; existing claims only drain |
| uncertain append/restoration/publication | `STORAGE_NONCONTINUABLE`; no ordinary retry/publication |

The higher statement/transaction/lifecycle consequence remains owned by §39.1 and the
existing failure procedures. Tests preserve distinctions rather than inventing BufferPool
aliases.

### Chapter 7 concurrency matrix

| Race | Deterministic barrier | Legal survivor(s) | Forbidden outcome | Architecture owner |
|---|---|---|---|---|
| same-page fetch/fetch | sole load intent | one loader and one resident identity; joiners share outcome | duplicate usable frames | §§7.5, 7.6.3, 7.8 |
| fetch/victim | final pin/reservation | pin wins or mapping becomes non-pinnable | pin after reassignment begins | §§7.6.3, 7.12.1 |
| flush/new mutation | attempted no-flush acquisition during G writeback | mutation waits for reconciliation; subsequent G+1 is dirty | mutation publishes during active writeback | §§7.10.1–7.10.3, 12.12.3 |
| guard release/eviction | latch release before unpin | eviction only after complete release | eviction while guard uses bytes | §§7.7–7.9 |
| dirty eviction/WAL | WAL durability request | WAL durable then page write, or preserved dirty failure | data write before WAL | §§7.10–7.12.1 |
| retirement/fetch | RETIRING gate | pre-gate claim drains or post-gate rejection | new claim after gate | §7.12.5 |
| retirement/guard | existing guard held | guard finishes; close waits | invalidated live view/use-after-close | §7.12.5 |
| retirement/writeback | I/O reservation | I/O completes/fails before close | close/unlink during I/O | §7.12.5 |
| shutdown/fetch | quiescing publication | old work drains; new work rejected | post-quiesce acquisition | §§7.12.6, 3.3.6 |
| reassignment/stale view | complete frame reset | new identity only after old ownership ends | stale view accepted as new page | §§7.7.2, 7.12.2 |
| checkpoint/clean-to-dirty | DPT transition gate | complete old or complete new DPT state | missing/intermediate rec_lsn/FPI state | §§7.10.1, 7.10.5 |

### Buffer management domain/case matrix

| Family | Deterministic fixture | Barrier/fault | Expected oracle | Architecture reference |
|---|---|---|---|---|
| Frame transitions | one frame per legal/illegal edge | transition publication | exact legal survivor; illegal edge absent | §§7.6.1–7.6.2 |
| Same-page miss | 2+ fetchers, nonresident page | load intent/read/validation | one loader/copy; shared result | §§7.6.3, 7.8 |
| Fetch-victim | eligible zero-pin resident | final recheck | pin or victim, never both | §§7.6.3, 7.12.1 |
| Pin arithmetic | synthetic max/zero states | increment/release | checked failure; no wrap/underflow | §§7.7, 7.9 |
| Guard lifecycle | read/write/transferred guards | acquire/cancel/release | one claim; latch before pin release | §§7.7–7.7.2 |
| Stable flush | dirty resident G | copied image/reconciliation | stable matching G may clean | §§7.10.2–7.10.3 |
| Flush-mutation | attempt mutation during G I/O | no-flush acquisition/reconciliation | writer waits; success permits subsequent G+1; failure leaves mutation unchanged | §§7.10.1–7.10.3, 12.12.3 |
| Stale completion | coherent G+1 plus injected G token | completion predicate | no newer bytes/metadata/reservation altered | §7.10.3 |
| Stable completion | injected write/sync/WAL outcomes | pwrite/fdatasync/WAL | no premature clean/write | §§7.10.3, 7.11 |
| Eviction/reset | clean/dirty victim | reservation/write/reset | no old-state leak; failure restores | §§7.12.1–7.12.2 |
| CLOCK exhaustion | each ineligibility predicate | one logical pass | exact `NO_REPLACEABLE_FRAME` | §§7.9, 7.12–7.12.3 |
| Family validation | each managed page family | each validation stage | no invalid RESIDENT/guard | §§7.6.4, 4.13–4.14 |
| New-page publication | private appended page | WAL/bound/frame publication | no partial ordinary visibility | §7.12.4 |
| Retirement | active file with loads/pins/I/O | RETIRING/drain/CLOSED | no post-gate claim or premature unlink | §7.12.5 |
| Shutdown | active BufferPool | quiesce/drain/flush | no false clean close | §§7.12.6, 3.3.6 |
| Raw-I/O exceptions | one fixture per named owner | bypass capability | only bounded owner uses bypass | §§7.3–7.5, 13.14 |

### Chapter 7 architecture-obligation coverage map

Domains are: A layer/ownership, B frame state, C identity/page table, D pin, E latch,
F guard, G dirty state, H flush, I WAL/stable completion, J fetch, K PAGE_INIT/new page,
L replacement/eviction, M validation/format, N retirement, O shutdown, P raw-I/O exception,
Q concurrency, R failure, and S other architecture-defined storage behavior. Each row has
one primary procedure owner; cross-referenced generic procedures remain part of that owner.

| # | Domain | Atomic obligation and architecture owner | Verification owner and methodology |
|---:|:---:|---|---|
| 1 | A | DiskManager owns managed raw positional I/O/handles — §7.3 | Bounded raw-I/O exceptions; capability trace |
| 2 | A | Raw layer does not parse formats or publish database objects — §§7.3–7.3.1 | Bounded exceptions; negative interface/property case |
| 3 | A | FileId is logical identity; fd is private and registration-lifetime-bound — §7.3.1 | Retirement/CLOSED and exception fixtures |
| 4 | A | Higher storage owner initializes/validates superblock and object identity — §7.3.1 | Page-zero/open exception plus generic format tests |
| 5 | A | Ordinary managed page views use BufferPool lifetime and perform no I/O — §7.5 | Capability trace across managed families |
| 6 | A | BufferPool remains format agnostic and invokes owner validation — §§7.5, 7.6.4 | Cross-family parameterized load |
| 7 | A | WAL durability owner is distinct; BufferPool enforces dependency — §§7.3, 7.11 | WAL failure barrier and call trace |
| 8 | S | Positional I/O does not use shared offsets — §7.4.1 | Existing Disk tests; concurrent offset fixture |
| 9 | S | Reads require an exact page and expose no partial result — §7.4.2 | Disk short-read injection plus LOADING oracle |
| 10 | S | Writes require complete-page transfer — §7.4.2 | Stable-completion short/exact-write matrix |
| 11 | S | EINTR retry and non-retried-close semantics — §7.4.3 | Existing Disk injectable syscall procedure |
| 12 | S | No implicit extension/sparse write; aligned checked file bounds — §§7.4.4–7.4.6 | Disk/PageNo exhaustion procedures |
| 13 | S | I/O errors retain file/page/operation/errno context — §7.4.7 | Injected I/O result inspection |
| 14 | I | `fdatasync` owns file-byte stability; namespace sync remains separate — §7.4.8 | Stable completion and namespace lifecycle cross-reference |
| 15 | B | Every frame maps to one legal ownership/I/O combination — §7.6.1 | Frame lifecycle matrix enumeration |
| 16 | B | `FREE -> LOADING+READ` binds sole existing-page intent — §7.6.2 | Existing-page transition barrier |
| 17 | B | `FREE -> LOADING+NONE` binds sole private new-page intent — §7.6.2 | New-page private-frame barrier |
| 18 | B | Existing-page `LOADING -> RESIDENT` only after validation — §7.6.2 | Transition/validation publication fixture |
| 19 | B | New-page `LOADING -> RESIDENT` coordinates bound and PAGE_INIT — §7.6.2 | New-page co-publication fixture |
| 20 | B | Failed `LOADING -> FREE` removes intent and resets frame — §7.6.2 | Loader-failure matrix |
| 21 | B | Resident hit atomically adds one checked pin — §7.6.2 | Hit/pin linearization fixture |
| 22 | B | Persistent mutation publishes generation/dirty/recovery metadata atomically — §§7.6.2, 7.10.1 | Existing WAL/MTR publication observer plus BufferPool state capture |
| 23 | B | Resident copied writeback reserves one orthogonal I/O state — §7.6.2 | Stable-flush base fixture |
| 24 | B | Matching-generation stable completion may publish clean — §7.6.2 | Dirty reconciliation barrier |
| 25 | B | Newer generation survives old writeback completion dirty — §7.6.2 | Injected stale G token against coherent G+1 |
| 26 | B | Resident writeback failure returns coherent dirty RESIDENT — §7.6.2 | WAL/write/sync fault matrix |
| 27 | B | Final victim reservation publishes `RESIDENT -> EVICTING` — §7.6.2 | Fetch/victim barrier |
| 28 | B | Clean eviction may end in FREE after mapping removal/reset — §7.6.2 | Clean-eviction fixture |
| 29 | B | Clean eviction may transfer reservation to new LOADING only after reset — §7.6.2 | Clean-rebind fixture |
| 30 | B | Dirty victim enters eviction writeback without ordinary access — §7.6.2 | Dirty-eviction fixture |
| 31 | B | Successful dirty eviction may end FREE after stable completion — §7.6.2 | Dirty-success/no-request fixture |
| 32 | B | Successful dirty eviction may bind new LOADING only after reset — §7.6.2 | Dirty-success/request fixture |
| 33 | B | Failed dirty eviction restores old pinnable RESIDENT — §§7.6.2, 7.10.4 | Eviction failure-restoration matrix |
| 34 | B | Retirement drain removes mapping only after claims/I/O/latches drain — §§7.6.2, 7.12.5 | Retirement state harness |
| 35 | B | Illegal direct visibility, pinned eviction, and pre-reset rebind transitions are absent — §§7.6.1–7.6.2 | Negative transition table |
| 36 | C | BufferPool identity is PageId, not frame/fd/path/pointer — §§7.5–7.6 | Cross-frame/reopen identity fixtures |
| 37 | C | At most one active load and usable frame exists per PageId — §§7.5, 7.8 | Same-page multi-fetch barrier |
| 38 | C | `LOAD_INTENT` is installed before victim selection/I/O — §7.8 | Intent observability barrier |
| 39 | C | Joiners register claims without public pins/latches — §7.8 | Joiner-state inspection |
| 40 | C | Successful load atomically publishes RESIDENT and claim pins — §7.8 | Publication/wakeup barrier |
| 41 | C | Canceled joiner removes/releases only its own claim — §7.8 | Conditional cancellation cases |
| 42 | C | Failed load closes mapping, shares one error, and wakes joiners — §7.8 | Loader failure with 2+ joiners |
| 43 | C | Corrected later fetch may retry; no poison cache — §7.8 | Failure-then-repair sequence |
| 44 | C | Mapping never names A while frame bytes/identity are B — §§7.8, 7.12.2 | Reset/rebind observer |
| 45 | J | Normal fetch rejects invalid/unpublished PageId before ordinary load — §7.6.3 | Published-bound fixture |
| 46 | J | Resident-hit pin is the fetch linearization point — §7.6.3 | Hit/victim two-survivor race |
| 47 | J | First miss/waiter linearize at one validated publication — §7.6.3 | Same-page success fixture |
| 48 | Q | Fetch versus victim reservation has only pin-wins or eviction-wins survivor — §7.6.3 | Final recheck barrier |
| 49 | D | Pin preserves frame residency/identity, not byte exclusion — §§7.7, 7.7.1 | Pin-without-latch negative case |
| 50 | E | Read/shared and write/exclusive latches protect page bytes — §7.7.1 | Concurrent guard matrix |
| 51 | E | Transaction lock waits retain no page latch — §7.7.1 | Lock/latch barrier cross-reference |
| 52 | F | One guard owns exactly one pin and appropriate latch — §7.7 | Guard lifecycle counter trace |
| 53 | F | Guard releases latch before checked unpin — §7.7 | Paused early-release observer |
| 54 | F | Canceled/failed latch acquisition releases one pin and no guard — §7.7 | Post-pin cancellation fault |
| 55 | D | Pin increment is checked and cannot wrap — §7.7 | Synthetic max boundary |
| 56 | D | Release cannot underflow or double-decrement — §7.7 | Double/destructor-after-release cases |
| 57 | F | Guard transfer leaves exactly one owner and inert source — §7.7.2 | Transfer/destruction trace |
| 58 | F | Early release equals destruction and is ownership-idempotent — §7.7.2 | Early release plus destructor |
| 59 | F | Borrowed page/view lifetime ends at guard release — §7.7.2 | Poison/token stale-use test |
| 60 | C | Stale view cannot observe a reassigned frame as valid B — §§7.7.2, 7.12.2 | A-release/evict/B-bind test |
| 61 | D | Pinned frame may flush but cannot evict — §§7.9, 7.10.2 | Pinned-flush and victim-negative pair |
| 62 | L | Zero pins are necessary but all §7.9 predicates are required — §7.9 | Eligibility table |
| 63 | G | Dirty means current published generation not known stable — §7.10 | State/result inspection |
| 64 | G | Dirty does not imply commit/WAL durability/visibility; COMMIT is NO-FORCE — §7.10 | Commit-with-dirty-resident integration case |
| 65 | G | Persistent mutation publication obeys WAL/no-flush/metadata order — §7.10.1 | Existing §12.12 procedure plus frame observer |
| 66 | G | Published mutation advances generation exactly once; provisional rollback does not — §§7.6.1, 7.10.1 | Clean/dirty rollback state comparison |
| 67 | S | Generation cannot repeat while stale completion exists; terminal handling quiesces — §§7.6.1, 4.3.2.5 | Runtime generation specialization |
| 68 | H | Copied writeback captures stable bytes/PageId/generation/page_lsn under read latch — §7.10.2 | Stable-copy barrier |
| 69 | H | Durable checksum is finalized on private stable image — §§7.10.2–7.10.3 | Stable-copy checksum with explicit resident fault |
| 70 | I | WAL durable LSN reaches copied page_lsn before page write — §7.11 | WAL/data ordering trace |
| 71 | I | WAL durability failure prevents data-page pwrite — §7.11 | `flush_through` fault |
| 72 | I | Stable transfer requires exact 8192-byte pwrite — §7.10.3 | Short/exact-write fault matrix |
| 73 | I | Stable completion additionally requires owning-file `fdatasync` — §7.10.3 | Full-write/sync-failure case |
| 74 | I | Batched sync cannot clean/evict a covered frame before sync — §7.10.3 | Multi-frame batch barrier |
| 75 | G | Matching PageId/generation reconciliation atomically clears dirty/DPT/rec_lsn — §§7.10.3, 7.10.5 | Reconciliation publication observer |
| 76 | G | Newer mutation keeps dirty and recovery metadata after old completion — §7.10.3 | Injected stale G token against coherent G+1 |
| 77 | C | Old-page completion cannot mutate later frame identity — §§7.10.3, 7.12.2 | Injected stale completion token |
| 78 | H | Explicit overlapping/current-content flush joins/repeats to current clean generation — §7.10.2 | Two explicit callers |
| 79 | H | Optional background writeback skips/requeues safely and remains optional — §§7.9, 7.10.2 | Conditional background case |
| 80 | H | Pinned flush preserves mapping/identity and coherent copy — §§7.9, 7.10.2 | Held-pin stable flush |
| 81 | R | Flush failure preserves mapping, dirty/recovery metadata, and retryability — §7.10.4 | WAL/write/sync failure table |
| 82 | Q | DPT/checkpoint capture observes complete dirty or safely clean state — §7.10.5 | Transition-gate observer cross-reference |
| 83 | L | CLOCK sets/tests reference state only for defined resident accesses — §7.12 | Reference-bit table |
| 84 | L | Referenced eligible frame receives second chance — §7.12 | Ordered small-pool traversal |
| 85 | L | Final victim reservation atomically rechecks all eligibility — §7.12.1 | Candidate-observed/final-recheck race |
| 86 | L | One complete unsuccessful ordinary pass returns `NO_REPLACEABLE_FRAME` — §7.12.3 | Logical visit-count fixture |
| 87 | L | Ordinary no-victim operation neither waits indefinitely nor steals ineligible frame — §7.12.3 | Barrier/progress-state oracle |
| 88 | L | Optional bounded/cancellable wait path cannot weaken eligibility — §7.12.3 | Conditional interface contract test |
| 89 | L | Clean eviction removes mapping and resets before reuse — §§7.12.1–7.12.2 | Clean-eviction ordering trace |
| 90 | L | Dirty eviction completes WAL/write/sync before removal/reuse — §§7.11, 7.12.1 | Dirty-success trace |
| 91 | R | Dirty-eviction failure restores old mapping/dirty state — §§7.10.4, 7.12.1 | Four-fault restoration matrix |
| 92 | R | Failed dirty eviction fails requesting load and its joiners — §7.12.1 | Captured-error waiter case |
| 93 | C | Complete reset removes every old identity/metadata influence — §7.12.2 | Post-reset field inspection |
| 94 | S | Buffer exhaustion remains distinct from disk/numeric/page-space outcomes — §§7.12.3, 7.12.7 | Paired classification fixtures |
| 95 | M | Registered owner and published bound validate before load — §§7.6.3–7.6.4 | Validation-order barrier |
| 96 | M | Exact transfer completes before byte trust — §§7.4.2, 7.6.4 | Short-read failure |
| 97 | M | Family/version dispatch precedes family-specific v1 parsing — §§7.6.4, 4.14 | Current/future paired fixtures |
| 98 | M | Checksum/common validation precedes page_lsn trust — §§7.6.4, 4.12–4.13 | Bad-checksum/plausible-LSN fixture |
| 99 | M | PageId/FileKind/PageType identity validates before publication — §7.6.4 | One-fault-per-field matrix |
| 100 | M | Complete nonfetching L1/applicable L2 owner validation precedes publication — §7.6.4 | Cross-family owner fixtures |
| 101 | M | Owner validator cannot recursively fetch or weaken missing context — §7.6.4 | Validator dependency/capability observer |
| 102 | M | Any validation failure cleans LOADING and publishes no guard — §7.6.4 | Failure/joiner/retry fixture |
| 103 | M | Malformed v1 and unsupported future format remain distinct — §§7.12.7, 4.14 | Paired corruption/unsupported table |
| 104 | M | HEAP/FSM/BTREE/catalog/status families use their registered validators — §§4.13, 7.6.4 | Parameterized family harness |
| 105 | K | New page begins private LOADING and cannot be ordinarily fetched — §7.12.4 | Private-frame/concurrent-fetch barrier |
| 106 | K | Pre-WAL new-page failure resets frame/bound and restores tail — §7.12.4 | PAGE_INIT failure specialization |
| 107 | K | Post-authorizing-WAL failure finishes publication or becomes noncontinuable — §7.12.4 | Post-append fault boundary |
| 108 | K | PAGE_INIT bytes, owner bound, RESIDENT mapping, and creator pin co-publish — §7.12.4 | One publication observer |
| 109 | Q | Concurrent fetch cannot access private unpublished new page — §7.12.4 | Before/after publication race |
| 110 | K | Crash discards runtime frame state; recovery uses persistent WAL/file state — §§7.6.5, 7.12.4 | PAGE_INIT crash specialization |
| 111 | N | ACTIVE/RETIRING gate linearizes new-fetch admission — §7.12.5 | Retirement/fetch race |
| 112 | N | Existing guards remain valid and drain before close/unlink — §7.12.5 | Held-guard retirement fixture |
| 113 | N | Existing writeback/I/O drains before fd close/unlink — §7.12.5 | Retirement/writeback barrier |
| 114 | N | Dirty discard requires proven higher semantic retirement authority — §7.12.5 | Authorized/unauthorized paired fixtures |
| 115 | R | Retirement failure preserves RETIRING state and prevents unlink/CLOSED — §7.12.5 | Drain/writeback fault injection |
| 116 | N | CLOSED has no frame/mapping/pin/I/O; old FileId cannot rebind — §7.12.5 | Post-close inventory/nonreuse check |
| 117 | O | Quiescing rejects new work, drains claims/I/O, flushes required dirty pages, and tears down before WAL — §7.12.6 | Existing shutdown procedure plus BufferPool observer |
| 118 | P | WAL segments remain under specialized WalManager I/O/durability — §§7.3, 7.5 | WAL exception capability trace |
| 119 | P | `database.control` remains under specialized lifecycle/recovery I/O — §§7.3, 7.5 | Control exception fixture |
| 120 | P | Initial FileSuperblock/page-zero access is bounded to identity establishment — §§7.3.1, 7.5 | Open/registration boundary fixture |
| 121 | P | Bootstrap/create private access ends at canonical publication — §§7.3.1, 7.5 | Bootstrap private-publication fixture |
| 122 | P | Recovery-private torn-page access ends at validated reconstruction — §§7.6.4, 13.14 | Torn-page recovery fixture |
| 123 | P | Namespace create/rename/unlink bypasses page interpretation only — §§7.3–7.4.8 | Namespace capability/durability trace |
| 124 | P | Ordinary managed pages have no generic raw-I/O escape hatch — §7.5 | Direct-I/O capability trace |
| 125 | C | Persistent page publication and frame publication are independent dimensions — §§7.6.3, 7.12.4 | Existing-page/new-page paired observer |
| 126 | R | BufferPool errors preserve exact storage/format/exhaustion/lifecycle distinctions — §7.12.7 | Failure-classification matrix |
| 127 | S | Frame/page-table/pin/latch/CLOCK state is process-local and not recovered — §7.6.5 | Crash specialization and post-reopen inventory |

## Heap tests

- thousands of tuples,
- many pages,
- scan after reopen,
- deletion-marker behavior under MVCC visibility,
- FSM stale-entry repair.

## Free-space map verification

This verification family owns the deterministic procedures for the FSM contract in
`ARCHITECTURE.md` Chapter 6. It specializes the generic storage-format, owner-validation,
PAGE_INIT, WAL/MTR, recovery, and reclamation procedures rather than duplicating them. The
relevant generic owners are Architecture §§4.10–4.13, 7.3–7.12, 12.9, 12.12, 12.17,
13.11–13.19, 14.5–14.12, 14.16, 15.2–15.4, 39.1, and 41.1.

### Deterministic fixtures and observability

Build FSM fixtures with explicit little-endian codecs. Unless a case intentionally targets
structural or owner corruption, each fixture has the correct FileSuperblock, FileKind,
FileId, TableId, common header, `FSM_DATA` PageType, PageNo, 48-byte header, reserved-zero
fields, initialized prefix, zero suffix, paired published bounds, and checksum. This keeps a
category or mapping case from accidentally becoming a format-corruption case.

The FSM-specific codec fixture checks exact offsets and widths independently:

| Byte range | Verification oracle |
|---|---|
| `0..31` | canonical common page header |
| `32..39` | little-endian `first_heap_page_no` |
| `40..41` | little-endian `entry_count` |
| `42..43` | zero FSM `reserved16` |
| `44..47` | zero FSM `reserved32` |
| `48..8191` | exactly 8144 one-byte category entries |

Assert a 16-byte FSM-specific extension, a 48-byte total header, an 8192-byte complete page,
and no gap or overlap. The generic FileSuperblock procedure supplies the separate 72-byte
`FileKind::FSM` superblock checks.

The test oracle computes category and mapping results independently of the production helper
under test. A static expected vector or a test-side checked-arithmetic implementation is
acceptable; calling the production helper to calculate its own expected result is not.

Deterministic hooks or barriers expose these abstract events without prescribing production
function or class names:

```text
heap mutation published
heap page published
guarded heap free_bytes captured
FSM mapping computed
candidate category read
heap owner validation accepted
heap insertion recheck begins
FSM mutation begins
FSM WAL publication completes
FSM page writeback begins
FSM PAGE_INIT publication completes
prefix advancement begins and completes
local repair begins and completes
rebuild page publication completes
candidate publication occurs
```

Concurrency and crash cases use barriers, injected outcomes, or coordinated child processes;
they MUST NOT depend on sleeps or probabilistic timing. Category bytes cannot be observed as
trusted candidate metadata until transfer, format/version dispatch, checksum, page identity,
registered FSM owner, paired heap owner, FSM header/ranges, prefix, and suffix validation have
completed. A plausible `page_lsn` on a bad-checksum page must not become trusted state.

### Category arithmetic and heap geometry

Exhaustively enumerate every legal `free_bytes` value in `0..8144`. For each value, calculate
the expected category with independent checked arithmetic:

```text
b        = min(free_bytes, 8144)
u        = min(max(b - 8, 0), 8135)
expected = floor(u * 255 / 8135)
```

Compare the expected value with the category codec/update result, assert that it is in
`0..255`, and assert monotonic nondecrease from the preceding input. This is an exhaustive
8145-case domain check, not a sampled property test. Where the category converter accepts a
wider input domain, also pass 8145 and the converter's maximum representable input and assert
that checked clamping produces the same result as 8144; these are converter-saturation cases,
not valid heap-geometry fixtures.

Independently enumerate every category `c` in `0..255` and calculate: `minimum_usable(c) = (c * 8135 + 254) / 255`.

Assert exact equality with the inverse helper, monotonic nondecrease, checked intermediate
arithmetic, `minimum_usable(0) = 0`, `minimum_usable(255) = 8135`, and no result above 8135.
Keep explicit readable vectors for categories 1, 127, and 254 with expected lower bounds 32,
4052, and 8104 respectively.
For every forward-domain input and every threshold category, assert that a forward result at
least that threshold represents at least the corresponding `minimum_usable` byte count. This
joint exhaustive check proves that the inverse never promises more complete encoded tuple
bytes than the forward bucket guarantees.

Named boundary vectors keep failures readable:

| `free_bytes` | Usable bytes after the slot reserve | Expected category |
|---:|---:|---:|
| 0 | 0 | 0 |
| 1 | 0 | 0 |
| 8 | 0 | 0 |
| 9 | 1 | 0 |
| 39 | 31 | 0 |
| 40 | 32 | 1 |
| 71 | 63 | 1 |
| 72 | 64 | 2 |
| 4075 | 4067 | 127 |
| 8142 | 8134 | 254 |
| 8143 | 8135 | 255 |
| 8144 | 8135 | 255 |

Create otherwise valid `FSM_DATA` fixtures containing every byte value `0..255` inside the
initialized prefix and assert that all are accepted. Verify category zero in two contexts:
inside `[0, entry_count)` it is an initialized least-capacity estimate; at or beyond
`entry_count` the same zero byte is uninitialized storage and cannot be accessed or returned
as a candidate.

Exercise the eight-byte new-slot reserve with paired heap/FSM fixtures. For a requested
complete encoded tuple length `n`, compare a gap of `n + 8` with `n + 7` using the exact
inverse/bucket threshold: the first may advertise the request under the new-slot convention,
while the second must not guarantee it. Separately construct an architecture-authorized
reusable `UNUSED` slot and a gap that fits `n` only without a new slot. Heap insertion may
succeed, while FSM selection may conservatively skip the page; the oracle classifies this as
safe packing inefficiency and retains the guarded heap recheck.

For the whole-tuple limit, assert that complete encoded length 8135 is accepted by the heap
limit, a fresh page with `free_bytes = 8144` maps to category 255, free gaps 8143 and 8144 map
to 255, and `minimum_usable(255) = 8135`. A complete encoded tuple length of 8136 must still
be rejected by the Chapter 5 heap oracle; no category interpretation makes it legal.

Construct a fragmented heap page whose current contiguous `[lower,upper)` gap is smaller than
its total reclaimable space. Assert that pre-compaction categorization uses the current gap,
legal compaction may increase the recomputed category, and the old lower category remains a
safe stale value. FSM metadata must not itself authorize compaction or claim post-compaction
capacity before heap logic establishes it.

### Mapping and maximum coverage

For every mapping case, the independent checked-arithmetic oracle computes:

```text
ordinal        = heap_page_no - 1
fsm_page_no    = 1 + ordinal / 8144
entry_index    = ordinal % 8144
reverse_page   = 1 + (fsm_page_no - 1) * 8144 + entry_index
```

Reject heap page zero, `INVALID_PAGE_NO`, underflow, overflow, an FSM data PageNo of zero, and
any entry index at least 8144 before narrowing or indexing. Verify these fixed vectors:

| Heap PageNo | Heap ordinal | FSM PageNo | Entry | Coverage boundary |
|---:|---:|---:|---:|---|
| 1 | 0 | 1 | 0 | first entry of FSM page 1 |
| 8144 | 8143 | 1 | 8143 | last entry of FSM page 1 |
| 8145 | 8144 | 2 | 0 | first entry of FSM page 2 |
| 16288 | 16287 | 2 | 8143 | last entry of FSM page 2 |
| 16289 | 16288 | 3 | 0 | first entry of FSM page 3 |
| 1,125,899,906,842,622 | 1,125,899,906,842,621 | 138,249,006,243 | 7773 | maximum physical heap PageNo |

For representative FSM page `P`, verify that entries `0..8143` reverse to the contiguous
range `1 + (P-1)*8144` through `P*8144`, subject to the physical PageNo and initialized-prefix
bounds. Forward then reverse and reverse then forward must each produce one identity, with no
gap or overlap.

At the maximum physical heap PageNo, verify checked subtraction, division, remainder,
addition, multiplication, and file-offset formation before narrowing. The paired file needs
`138,249,006,243` FSM data pages, `138,249,006,244` total pages including its superblock, and
`1,132,535,859,150,848` bytes. Its last page begins with heap PageNo
`1,125,899,906,834,849` and needs initialized prefix length 7774. Assert the final FSM PageNo
and signed positional-I/O byte range remain legal, so FSM capacity does not exhaust before
the heap PageNo domain.

### Initialized prefix, format, and ownership

Create valid pages with `entry_count` 0, 1, 8143, and 8144 when all represented pages lie
within the paired heap's published bound. Create raw malformed input with `entry_count = 8145`
and reject it before category access or ordinary publication.

Publish a heap extent longer than its initialized FSM prefix and assert that the FSM page is
structurally valid, only initialized entries are searchable, and omitted suffix coverage is
not corruption. Then extend the prefix without skipping positions: heap publication must
precede entry availability, the new entry must map to the exact heap PageNo, and every byte
after the new prefix must remain zero.

For each selected prefix length, verify all suffix bytes are zero. Inject one nonzero byte at
the first suffix position, a middle suffix position, and the final category byte where those
positions exist; each fixture is malformed v1 FSM data and must be rejected. Direct access,
update, or candidate publication at `index == entry_count` must also be rejected rather than
interpreting the suffix zero as category zero.

Pair each FSM page with the authoritative heap published bound. A prefix ending exactly at
that bound is accepted; a prefix extending one entry beyond it is rejected before ordinary
candidate use. An initialized entry for a retired, unpublished, or out-of-bound heap PageNo
must never become a candidate.

Use two fully valid relations A and B with distinct TableIds, heap FileIds, and FSM FileIds.
Ordinary FSM use for A must agree across the expected FSM descriptor, `FileKind::FSM`, FSM
FileId, superblock object_id/TableId, paired heap TableId/identity, requested FSM PageNo and
`FSM_DATA` type, and both files' published bounds. Attempting FSM(A) with heap(B) is rejected
as wrong-owner state before candidate publication, not treated as stale metadata.

Run a table-driven owner/format matrix containing:

```text
correct FSM FileId, wrong TableId
wrong FSM FileId, correct TableId
correct superblock, wrong registered descriptor
valid page bytes copied from another FSM file
correct TableId, wrong paired HEAP identity
FSM PageNo 0 used as FSM_DATA
INVALID_PAGE_NO
stored PageNo mismatch
wrong PageType or FileKind
unpublished FSM PageNo
bad header_size or checksum
nonzero common or FSM reserved field
wrong deterministic first_heap_page_no
recognized future format version
```

Malformed v1 and wrong-owner cases are rejected before category access. A recognized future
format/version produces the canonical unsupported-format result before future bytes are
interpreted. The bad-checksum case embeds a plausible high `page_lsn` and proves checksum
acceptance precedes trust in that LSN. Generic FileSuperblock and common-page checks reuse the
Chapter 4 storage procedures; this family adds the FSM pairing, deterministic mapping,
prefix/suffix, and candidate-publication oracles.

### Advisory candidates and physical-space effects

For stale high, persist a structurally valid suitable category while the paired heap page has
insufficient actual insertion space. Candidate selection may suggest the page, but it must
then fetch and owner-validate the heap page, acquire the required protection, recompute actual
heap insertion feasibility, and reject mutation. Assert no extent overlap or out-of-bounds
write, and permit downward repair plus continued search/fallback. A test hook immediately
before heap mutation must fail the test if no guarded geometry recheck occurred.

For stale low, persist a category below the request threshold while the heap page can accept
the tuple. Candidate search may skip the page and choose another page or extension. The oracle
records only efficiency/packing loss; no exact packing or eventual-discovery guarantee is
imposed. Repair or rebuild may improve the category.

Construct a structurally and owner-valid category that differs from current heap geometry.
Structural validation must accept it, and ordinary use treats it as stale advisory metadata,
not corruption. Parameterize the stale-high and stale-low cases so that the stale source can
also be the process-local candidate accelerator while persisted FSM bytes remain exact.
Invalidating or rebuilding that accelerator must not affect persistent correctness, and no
container, bucket layout, or tie-breaker is prescribed.

Exercise physical-space changes rather than logical row counts:

- logical DELETE retains its tuple bytes and must not advertise reclaimed space;
- UPDATE consumes destination capacity while the old version retains its space until
  reclamation;
- an aborted but physically retained inserted version still consumes space;
- `NORMAL -> DEAD` and retained DEAD payload do not by themselves create reusable bytes;
- payload discard, `DEAD -> UNUSED`, or compaction may permit a refreshed category computed
  from the final validated heap geometry.

An FSM value that resembles an empty or maximally available page cannot authorize SlotId,
RID, or whole-page reuse. The Vacuum and Reclamation Tests remain the oracle for the
Chapter 14 grace and identity predicates.

### Mutation, PAGE_INIT, and crash boundaries

Create a deterministic boundary where a heap mutation publishes successfully and the FSM
hint update is omitted or fails before publication. Assert that the heap mutation remains
valid, its statement/transaction outcome is not rolled back solely because the hint is
absent, the old category may remain stale, and later candidate use remains safe through heap
recheck. This directly permits separate heap and FSM MTRs without requiring them.

For an attempted FSM update after heap truth changes, inject:

| Boundary | FSM survivor | Required oracle |
|---|---|---|
| before FSM mutation publication | old complete legal category | heap result remains valid; stale hint is safe |
| complete FSM WAL publication, page not flushed | redoable mutation | generic recovery may install the complete new category |
| WAL/publication outcome is unsafe or indeterminate | canonical failure state | apply §39.1; do not invent an FSM-specific result |
| after complete FSM page publication | new complete legal category | `page_lsn`, checksum, and candidate visibility agree |

Specialize the generic PAGE_INIT and MTR Rollback Tests for new `FSM_DATA` pages. Pause after
private image construction, after PAGE_INIT WAL assignment, before published-bound advance,
and after ordinary publication. Assert correct PageNo/type/owner/header/prefix/suffix,
WAL-before-data, final checksum, and page_lsn. No private or unpublished page may enter either
the persisted or runtime candidate search.

Crash before PAGE_INIT publication, after PAGE_INIT WAL publication but before data flush,
and after page publication. Reuse the generic append/recovery oracle, adding that candidate
search never sees a private/unpublished FSM page. Failure before publication leaves no
ordinary searchable page; a published page must recover as a complete valid page.

Interrupt prefix advancement near both an ordinary boundary and 8144. Recovery must expose
either the old complete prefix or the new complete prefix, never a torn count, nonzero bytes
outside the recovered prefix, a skipped initialized position, or a prefix past the heap
published bound.

Use barriers for both growth races:

- while one worker publishes a new heap page, FSM search/update may lag but cannot lead the
  authoritative heap bound;
- when mapping first requires a new FSM data page, concurrent search ignores it until
  PAGE_INIT, publication, owner validation, prefix validity, and candidate publication all
  complete.

### Required derived state, repair, rebuild, and recovery

For a committed table, ordinary open establishes the catalog-owned FSM descriptor, managed
identity, FileId, FileKind, TableId, and final file name. Remove the required FSM file and
assert that READY cannot silently treat it as optional or substitute another file. Where the
canonical recovery/repair owner permits reconstruction, the required object must be restored
at its exact identity before normal use.

Separate structurally valid staleness from structural invalidity. READY may admit a valid but
inaccurate FSM according to the recovery contract. Bad checksum, bad header size, nonzero
reserved data, wrong deterministic range, excessive prefix, nonzero suffix, or owner mismatch
must not enter ordinary use. An explicitly owned rebuild reads validated paired heap geometry;
it never treats invalid old category values as source truth.

This family proves the complete required/rebuildable/derived/persistent classification:

- required: the expected catalog-owned FSM identity is not ignored;
- rebuildable: the owned repair/recovery path can reconstruct invalid category state;
- derived: validated heap geometry, not old FSM bytes, row counts, or planner statistics, is
  the rebuild oracle;
- persistent: rebuilt pages use ordinary PAGE_INIT, WAL, page_lsn, checksum, and publication
  procedures.

For local repair, begin with one stale-high and one stale-low entry, capture a guarded and
owner-validated heap snapshot, independently calculate its category, and publish only the
mapped entry/prefix change permitted by the architecture. Assert owner and mapping stability.
The result may become stale again after a later heap mutation. Crash during repair and accept
only the old complete category or new complete category according to generic WAL/page
recovery; torn page state is forbidden.

For whole-file or page-range rebuild, scan validated paired heap-page headers, derive each
current contiguous gap, apply the same independent forward-category oracle used by incremental
updates, map each heap PageNo exactly once, construct the legal prefix, and zero the suffix.
Compare incremental update and rebuild across the exhaustive `free_bytes` domain to prove
formula identity.

Crash after any published rebuild page/prefix. Every published survivor must be independently
valid; short or missing coverage remains non-searchable rather than fabricated category-zero
coverage; unpublished work is ignored or reconciled; and rebuild can restart or resume under
the recovery owner. If rebuild scans concurrently with heap changes, accept structurally legal
stale categories and require heap recheck. A quiescent rebuild strategy is also conforming;
the test suite must not require one scheduling choice.

The crash/recovery specialization is:

| Injected boundary | Heap authority | Permitted FSM survivor | Recovery/candidate oracle |
|---|---|---|---|
| heap mutation durable, hint absent | new heap state | old legal category | READY may retain staleness; heap recheck is final |
| FSM WAL mutation complete, page unflushed | corresponding heap state | old disk image plus redoable WAL | redo installs a complete legal FSM mutation |
| FSM PAGE_INIT private or unpublished | published heap bound only | no ordinary FSM page | page cannot be searched; generic append recovery applies |
| FSM PAGE_INIT published | published heap bound | complete initialized FSM page | validate owner/header/prefix/suffix before search |
| prefix advancement interrupted | unchanged published heap bound | old or new complete prefix | no torn count, skipped entry, nonzero suffix, or lead beyond heap |
| local repair interrupted | authoritative heap page | old or new complete category | either survivor is advisory-safe; no torn page |
| rebuild partially published | validated heap pages | independently valid published prefixes/pages | short coverage is safe; restart/resume rebuild |
| page reinit or retirement before FSM refresh | new authoritative bound/incarnation | stale old category | bound and Chapter 14 gates prevent candidate or RID misuse |

### Reclamation, retirement, and identity safety

Specialize the Vacuum and Reclamation Tests with an FSM estimate that suggests maximal
capacity while a former RID or page identity is still grace-protected. Before the complete
Chapter 14 predicate, SlotId reuse and whole-page reinitialization remain forbidden. After
grace and canonical reinitialization, the heap geometry resets and FSM state may be refreshed
for the new page incarnation; an old RID must not resolve to a new tuple prematurely.

Persist stale FSM metadata for a retired or out-of-bound heap PageNo and for a physically
present unpublished heap tail. Assert that candidate publication rejects each case before
heap access based on FSM alone. Repair/rebuild may remove or zero such metadata, but the stale
bytes cannot expand the authoritative heap bound.

### Error and classification matrix

| Condition | Procedure | Expected classification or result |
|---|---|---|
| category differs from guarded heap geometry | stale-high/low fixture | legal advisory staleness; heap recheck decides |
| malformed v1 FSM page | raw format/prefix/suffix fixture | corruption; reject ordinary use |
| valid bytes owned by another FSM/heap pair | two-relation owner fixture | wrong-owner corruption; reject before candidate publication |
| required FSM file absent | committed-table open fixture | missing-required-object/open-repair outcome; never optional substitution |
| recognizable future page/file format | version-dispatch fixture | unsupported-format result before layout interpretation |
| exact or short ordinary I/O | generic disk fault specialization | canonical I/O failure |
| unsafe WAL/publication failure | FSM mutation injection | canonical §39.1 durability/instance consequence |
| candidate fails guarded heap-space recheck | stale-high insertion fixture | candidate `NO_SPACE`/retry path, not FSM corruption |

### FSM verification domain/case matrix

| Domain | Deterministic fixture | Fault/race boundary | Expected oracle | Architecture reference |
|---|---|---|---|---|
| Forward categories | all `free_bytes` 0..8144 | N/A | exact independent formula and monotonicity | §§6.3, 6.3.1 |
| Inverse categories | all categories 0..255 | N/A | exact lower bound and monotonicity | §6.4 |
| Slot reserve / tuple limit | paired gaps, reusable slot, 8135/8136 | heap recheck | conservative eight-byte rule; heap decides | §§5.4–5.6, 6.3–6.4 |
| Mapping boundaries | fixed vectors and reverse map | arithmetic limits | no gap, overlap, wrap, or early FSM exhaustion | §6.6; §4.3.2.3 |
| Prefix/suffix | counts 0, 1, 8143, 8144, malformed 8145 | prefix publication | short prefix accepted; suffix canonical | §§6.7–6.8 |
| Owner pairing | two valid relations and cross-cases | before candidate publication | exact descriptor/FileId/TableId/heap pairing | §§4.10, 4.13.6, 6.13 |
| Stale high | overstated legal category | before heap mutation | guarded heap recheck prevents unsafe insert | §§6.1, 6.10 |
| Stale low | understated legal category | candidate enumeration | efficiency loss only | §6.10 |
| PAGE_INIT | canonical private FSM page | WAL and publication boundaries | unpublished page never searchable | §§6.8, 12.9, 12.12 |
| Prefix crash | prefix near ordinary/full boundary | count/category publication | old or new complete prefix only | §§6.7–6.8, 12.12 |
| Local repair | stale-high and stale-low entries | repair WAL publication | old or new legal category | §6.10; §12.12 |
| Rebuild | validated heap scan | partial page/range publication | restartable valid short coverage; formula identity | §6.10; §§13.11–13.19 |
| Required file | committed catalog owner | missing/corrupt file at open | owned repair/rebuild or no READY | §§4.13.6, 6.10, 13.18 |
| Reclamation/reuse | grace-protected RID/page | before and after grace | FSM never authorizes identity reuse | §§6.12–6.13, 14.5–14.12 |

### Chapter 6 architecture-obligation coverage map

The atomic inventory below is the procedural owner map for Chapter 6. “Complete” means that
the obligation has a deterministic fixture, operation or injected boundary, and explicit
oracle in this section or in the named generic procedure it specializes.

| # | Architecture obligation | Architecture owner | Verification owner |
|---:|---|---|---|
| 1 | Forward formula exactness | §6.3 | Category arithmetic — exhaustive forward domain |
| 2 | Inverse formula exactness | §6.4 | Category arithmetic — exhaustive inverse domain |
| 3 | Forward monotonicity | §6.3 | Category arithmetic — adjacent full-domain comparison |
| 4 | Saturation at physical/tuple maxima | §6.3 | Category arithmetic — 8143/8144 and upper-limit cases |
| 5 | Category-zero boundary and meaning | §§6.2–6.4 | Category arithmetic — named boundaries and context test |
| 6 | Every uint8 category is valid in-prefix | §§6.2, 6.5 | Category arithmetic — 0..255 encoded fixtures |
| 7 | Eight-byte new-slot reserve | §6.3 | Category arithmetic — `n+8`/`n+7` paired cases |
| 8 | Reusable-slot conservative understatement | §6.3 | Category arithmetic — reusable UNUSED fixture |
| 9 | 8135 maximum complete encoded tuple interaction | §§5.6, 6.3–6.4 | Category arithmetic — whole-tuple boundary |
| 10 | 8136 remains an invalid complete encoded tuple | §5.6 | Category arithmetic — heap final-authority boundary |
| 11 | First heap-page mapping | §6.6 | Mapping — heap 1 vector |
| 12 | Last entry of FSM page 1 | §6.6 | Mapping — heap 8144 vector |
| 13 | First entry of FSM page 2 | §6.6 | Mapping — heap 8145 vector |
| 14 | Exact 8144-entry boundaries | §6.6 | Mapping — 8144/8145 and 16288/16289 vectors |
| 15 | Reverse mapping and coverage | §6.6 | Mapping — bidirectional range enumeration |
| 16 | Checked mapping arithmetic | §6.6 | Mapping — checked operation/failure cases |
| 17 | Maximum heap PageNo mapping | §§4.3.2.3, 6.6 | Mapping — terminal vector |
| 18 | Maximum paired FSM coverage | §§4.3.2.3, 6.6 | Mapping — page-count/file-length proof |
| 19 | `entry_count` lower bound | §6.7 | Prefix — zero-count valid fixture |
| 20 | `entry_count` upper bound | §6.7 | Prefix — 8144 valid and 8145 invalid fixtures |
| 21 | Short initialized prefix is valid | §6.7 | Prefix — heap extent longer than prefix |
| 22 | Initialized category zero is valid | §§6.7–6.8 | Prefix — in-prefix zero fixture |
| 23 | Suffix bytes are canonical zero | §6.7 | Prefix — valid zero suffix |
| 24 | Nonzero suffix is malformed | §6.7 | Prefix — first/middle/final corruption fixtures |
| 25 | Entry outside prefix is inaccessible | §6.7 | Prefix — `index == entry_count` operations |
| 26 | Prefix cannot exceed paired heap bound | §6.7 | Prefix — exact-bound/one-past pair |
| 27 | Correct HEAP/FSM TableId pairing | §§4.10, 4.13.6 | Ownership — valid relation fixture |
| 28 | HEAP and FSM use distinct FileIds | §§4.7, 6.1–6.2 | Ownership — valid relation identity checks |
| 29 | Wrong FSM/heap attachment is rejected | §§4.13.6, 6.13 | Ownership — two-relation cross-pair fixture |
| 30 | Wrong FileKind is rejected | §§4.10, 6.2 | Ownership — malformed-owner table |
| 31 | Wrong PageType is rejected | §§4.9, 6.5 | Ownership — malformed-owner table |
| 32 | Wrong stored PageNo is rejected | §§4.9, 6.6 | Ownership — malformed-owner table |
| 33 | Unpublished FSM page is rejected | §§4.11, 6.8 | Ownership/PAGE_INIT — publication barrier |
| 34 | Entry beyond heap bound is rejected | §§6.7, 6.13 | Prefix/ownership — paired-bound fixture |
| 35 | Stale-high category is safe | §6.10 | Advisory — overstated candidate fixture |
| 36 | Stale-low category is safe | §6.10 | Advisory — understated candidate fixture |
| 37 | Heap recheck precedes mutation | §§6.1, 6.4, 6.10 | Advisory — mandatory ordering hook |
| 38 | Runtime candidate index is not authoritative | §6.9 | Advisory — invalidation/stale runtime parameter |
| 39 | FSM cannot prove insertion success | §§6.1, 6.10 | Advisory — stale-high rejection |
| 40 | FSM cannot authorize RID reuse | §§6.12–6.13 | Reclamation — grace-protected capacity fixture |
| 41 | Heap mutation may precede FSM hint update | §6.10; §14.16 | Mutation — omitted-hint boundary |
| 42 | Missed FSM update does not roll back heap truth | §6.10; §39.1 | Mutation — omitted/failed hint oracle |
| 43 | Separate FSM MTR is permitted | §§6.8, 6.10; §12.12 | Mutation — separate publication fixture |
| 44 | WAL-before-data applies to FSM | §§6.8, 12.17 | PAGE_INIT/WAL generic specialization |
| 45 | `page_lsn` and checksum follow final FSM mutation | §§4.12, 6.8 | PAGE_INIT/WAL specialization |
| 46 | FSM PAGE_INIT publication is atomic | §§6.8, 12.9, 12.12 | PAGE_INIT boundary matrix |
| 47 | Private/unpublished FSM pages are not searchable | §6.8 | PAGE_INIT candidate-publication hook |
| 48 | Complete FSM mutation is redoable | §§12.12, 13.13 | Mutation/recovery — WAL-published case |
| 49 | READY may use structurally valid stale FSM | §6.10; §13.18 | Recovery — stale valid open fixture |
| 50 | Structurally invalid FSM is rejected | §§4.13, 6.5–6.8 | Format/ownership corruption matrix |
| 51 | Local stale-entry repair | §6.10 | Repair — guarded heap snapshot fixture |
| 52 | Interrupted repair is atomic/recoverable | §§6.10, 12.12 | Repair — old/new survivor crash case |
| 53 | Rebuild derives from heap geometry | §6.10 | Rebuild — validated heap scan |
| 54 | Partial rebuild crash remains safe | §6.10; §§13.11–13.19 | Rebuild — published-prefix crash matrix |
| 55 | Interrupted prefix advancement remains complete | §§6.7–6.8, 12.12 | Prefix — old/new survivor crash case |
| 56 | Rebuild can restart/resume safely | §6.10; §13.18 | Rebuild — partial restart case |
| 57 | FSM file is required for its catalog owner | §§4.7, 4.13.6 | Required-state — missing-file open fixture |
| 58 | Missing FSM follows owned repair/open behavior | §§4.13.6, 6.10, 13.18 | Required-state — no optional substitution |
| 59 | FSM is rebuildable derived persistent state | §§6.8, 6.10 | Required-state four-part classification fixture |
| 60 | Logical DELETE does not free physical bytes | §§5.16, 15.4 | Physical effects — retained tuple fixture |
| 61 | UPDATE old page retains physical consumption | §§5.15, 15.3 | Physical effects — old/new version fixture |
| 62 | Vacuum/compaction may refresh category | §§6.11–6.12, 14.16 | Physical effects — state/compaction matrix |
| 63 | `DEAD -> UNUSED` remains grace-gated | §§6.12, 14.5–14.12 | Reclamation — before/after grace fixture |
| 64 | Whole-page reinit remains RID/page-reuse gated | §§6.12, 14.5–14.12 | Reclamation — page-incarnation fixture |
| 65 | Retired/unpublished heap pages cannot be candidates | §§4.11, 6.7–6.8 | Retirement — stale out-of-bound fixtures |
| 66 | Exact FSM page layout and little-endian fields | §§6.5–6.5.1 | Deterministic canonical fixture and codec checks |
| 67 | Common/FSM reserved fields are zero | §§6.5.1, 6.13 | Format — table-driven nonzero fixtures |
| 68 | Future format and malformed v1 dispatch differ | §§4.13–4.14, 6.5 | Format — version/classification matrix |
| 69 | Category input is current contiguous heap gap | §§5.4, 6.3, 6.11 | Category/compaction fixture |
| 70 | Runtime accelerator is derived and nonpersistent | §6.9 | Advisory — invalidate/rebuild runtime index |
| 71 | FSM validation ordering protects LSN/category use | §§4.13, 6.13 | Observability — staged acceptance hooks |
| 72 | Stale, corrupt, missing, unsupported, I/O, WAL, and `NO_SPACE` outcomes remain distinct | §§4.13–4.14, 6.10, 39.1 | Error and classification matrix |
