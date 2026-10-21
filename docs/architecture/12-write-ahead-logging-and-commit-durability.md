# 12. Write-Ahead Logging and Commit Durability

## 12.1 Scope and durability model

Write-ahead logging is mandatory. The database uses: `STEAL + NO-FORCE`.

Therefore:

- dirty data pages may reach their data files before the creating user transaction commits,
- commit does not force every heap/index/status/catalog page,
- WAL must contain enough durable redo and transaction-outcome information to reconstruct logical state after a crash.

The central ordering rule is:

> A WAL-protected persistent page MUST NOT become durable with a modification whose required recovery WAL is not already durable.

Durability of that WAL and of newly created database objects also includes the filesystem namespace prerequisites in §4.7 and §12.2.1. Regular-file synchronization alone cannot satisfy an acknowledgement whose required filename may disappear after crash.

Ordinary user-DML heap/index changes use redo plus transaction-status visibility; they do not require physical user-DML undo.

## 12.2 Logical WAL stream and physical segments

The database has one logical WAL byte stream.

It is physically stored in fixed-size files under `wal/` with the segment naming shape:

```text
0000000000000000.wal
0000000000000001.wal
...
```

The basename is exactly the unsigned segment index encoded as 16 lowercase hexadecimal digits followed by `.wal`. Shorter, uppercase, signed, decimal, or alternate spellings are not segment names.

The segment size is exactly:

```text
64 MiB
= 67,108,864 bytes
```

An LSN is the byte position of a WAL record in the logical WAL stream.

LSN/end-position exhaustion is exactly §4.3.2.4. Segment `0` bytes `0..7` are
the canonical zero prefix because LSN `0` is invalid; a fresh stream's first
record and initial append end are at LSN `8`.

Consequently, for a logical byte position `L`:

```text
segment_index  = L / 67,108,864
segment_offset = L % 67,108,864
```

WAL segmentation provides bounded file units, recovery scan boundaries, and a natural recycling unit.

### 12.2.1 WAL segment namespace and creation

WAL segments use direct final names under the already-durable `database_root/wal/` directory. They do not use DDL `pending/` names because their publication authority is the WAL writer's logical stream and `durable_lsn`, not catalog visibility.

Segments have no separate segment header. A fresh segment's logical contents begin as all zero bytes, which are valid unwritten/tail bytes under §12.3.

Creation of segment index `S` is serialized by the WAL segment/append owner and uses:

```text
1. require S to be the next contiguous segment index
2. create its exact final basename with exclusive no-follow semantics
3. ftruncate the fresh file to exactly 67,108,864 bytes
4. fdatasync the segment so its zero initial contents and length are durable
5. fsync database_root/wal
6. only after step 5 mark the segment namespace-durable and permit WAL
   bytes in S to participate in any durability request
```

Directory synchronization is therefore immediate for each newly created WAL segment, not deferred until an arbitrary later commit. It occurs once per creation; ordinary later WAL flushes do not resynchronize the directory unless another namespace mutation occurred.

Writing/acknowledging records in that segment then uses:

```text
7. pwrite complete contiguous WAL bytes
8. fdatasync the segment through the requested record/span
9. only after step 8 advance durable_lsn into S and wake durability waiters
```

No record in `S`, including `TXN_COMMIT`, is durable by architecture definition before both the segment's step-5 namespace publication and the step-8 content synchronization covering that record have succeeded.

Segment-creation failure or `fsync(wal_directory)` failure prevents `durable_lsn` from entering the segment and is propagated as WAL I/O failure. A possibly surviving empty/partial segment is not evidence of durable WAL beyond the prior segment.

After recovery, an exact-size next-contiguous all-zero segment may be adopted for future append only after validating its zero state, `fdatasync`ing it, and `fsync`ing `wal/` again. This reestablishes both content and namespace durability without assuming whether its precrash creation sync completed.

Crash outcomes are:

- before create: the prior segment prefix is the complete available WAL namespace,
- after create but before directory sync: the unacknowledged segment may be absent or present after restart; no durable outcome depended on it,
- after directory sync but before WAL write: the durable all-zero segment is an empty trailing segment,
- after WAL write but before segment `fdatasync`: recovery accepts only the complete valid prefix that actually survived; no waiter was acknowledged for the unsynchronized suffix,
- after segment `fdatasync` but before in-memory `durable_lsn` publication: the segment name and synchronized valid WAL prefix are durable and recovery uses them, although no waiter was yet acknowledged,
- after `durable_lsn` publication/acknowledgement: both the segment name and acknowledged WAL bytes survive under the required filesystem contract.

WAL-segment unlink/recycling follows §4.7.7 and §13.10.

## 12.3 Record alignment, physical span, and segment boundary

Every ordinary WAL record starts at an 8-byte-aligned logical LSN.

For every non-tail-padding record:

```text
total_length   = 48 + payload_length
physical_span  = align_up(total_length, 8)
```

`total_length` excludes external alignment bytes.

Bytes in:

```text
[record_lsn + total_length,
 record_lsn + physical_span)
```

MUST be zero. They are not included in the record CRC. No ordinary record may cross a 64 MiB segment boundary.

If the next record does not fit in the remaining segment bytes:

- when at least 48 bytes remain, emit one `WAL_PAD` record whose `total_length` consumes exactly the remaining bytes and whose payload is all zero;
- when fewer than 48 bytes remain, write an all-zero raw tail and begin the next record at the next segment boundary.

Recovery recognizes an all-zero short segment tail as padding, not as a record header. A logical record whose aligned `physical_span` is greater than 64 MiB is rejected as `WAL_RECORD_TOO_LARGE` before LSN reservation. One logical `BTREE_MTR` therefore has the same hard limit.

The exact emitted `total_length`/payload maxima and the no-split MTR exhaustion
outcome are §4.3.2.4. A uint32 length that is representable but whose aligned
span exceeds one segment is still too large.

## 12.4 WAL record header v1

Every ordinary WAL record begins with exactly 48 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `total_length` |
| `4` | 2 | `header_length = 48` |
| `6` | 2 | `record_type` |
| `8` | 4 | `flags = 0` |
| `12` | 4 | `reserved = 0` |
| `16` | 8 | `lsn` |
| `24` | 8 | `txn_id` |
| `32` | 8 | `prev_txn_lsn` |
| `40` | 4 | `payload_length` |
| `44` | 4 | `crc32c` |
|  | **48** | **total header** |

All integer fields are explicit little-endian. The architecture assigns no WAL-header flag bits. Encoders write zero and decoders reject nonzero `flags` or `reserved`. These fields are RESERVED_ZERO under §4.14; record length/framing never makes nonzero unknown semantics ignorable.

A decoder requires:

```text
header_length = 48
total_length  = 48 + payload_length
total_length >= 48
align_up(total_length,8) remains inside the current segment
header.lsn = physical logical record start LSN
```

CRC32C covers:

```text
48-byte header with bytes 44..47 logically zero
+
exact payload_length payload bytes
```

External alignment bytes are excluded and must be zero.

## 12.5 WAL PageId codec

Whenever a WAL payload persists a `PageId`, it uses exactly 16 bytes:

| Relative offset | Size | Field |
|---:|---:|---|
| `0` | 4 | `file_id` |
| `4` | 4 | `reserved32 = 0` |
| `8` | 8 | `page_no` |
|  | **16** | **total** |

The decoder rejects `INVALID_FILE_ID`, `INVALID_PAGE_NO`, or nonzero reserved bytes when the owning record requires a real page. This WAL PageId encoding is independent of C++ object layout.

## 12.6 Per-transaction WAL chain

Each user transaction tracks `last_wal_lsn`.

A user transaction record stores:

```text
txn_id       = owning normal TxnId
prev_txn_lsn = previous WAL record for that user transaction
```

System records store:

```text
txn_id       = INVALID_TXN_ID = 0
prev_txn_lsn = INVALID_LSN    = 0
```

B+ MTRs remain system records even when they carry an optional diagnostic owner TxnId in their payload.

## 12.7 Persisted WAL record-type registry

The 16-bit little-endian `record_type` codes are:

| Code | Record type |
|---:|---|
| `0` | `WAL_PAD` |
| `1` | `PAGE_INIT` |
| `2` | `PAGE_DELTA` |
| `3` | `PAGE_IMAGE` |
| `4` | `BTREE_MTR` |
| `5` | `TXN_COMMIT` |
| `6` | `TXN_ABORT` |
| `7` | `CHECKPOINT_BEGIN` |
| `8` | `CHECKPOINT_DATA` |
| `9` | `CHECKPOINT_END` |

Existing codes MUST NOT be renumbered.

Codes `0..9` are the complete v1 registry. A different code in an otherwise complete, correctly framed and CRC-valid record is `UNSUPPORTED_WAL_FORMAT`: analysis/redo cannot skip it or continue to READY because its recovery/transaction effects are unknown. Malformed framing follows §13.11's required-prefix corruption versus incomplete-tail rule.

### 12.7.1 WAL_PAD

A `WAL_PAD` record is a system record. Its payload consists only of zero bytes and its `total_length` fills the remainder of the current segment. The record has a normal valid CRC. `WAL_PAD` is never passed to page/transaction redo logic.

### 12.7.2 TXN_COMMIT and TXN_ABORT

Both terminal transaction records have:

```text
payload_length = 0
total_length   = 48
```

Their header stores:

```text
txn_id       = the normal transaction being terminated
prev_txn_lsn = that transaction's previous WAL record, or 0
```

`TXN_COMMIT` and ordinary user `TXN_ABORT` therefore become the tail of the per-transaction WAL chain. A recovery-generated TXN_ABORT uses the loser TxnId and the last transaction WAL LSN reconstructed by analysis as `prev_txn_lsn`.

These zero-payload records are the semantic evidence for terminal transaction outcome. They are not physical full-page images. When reflecting either outcome into a `TXN_STATUS` page requires a full image, the separate system `PAGE_IMAGE`/`PAGE_INIT` protocol in §12.10.5 supplies the physical reconstruction base.

### 12.7.3 Page-record transaction ownership

`PAGE_INIT`, `PAGE_DELTA`, and `PAGE_IMAGE` use:

```text
txn_id = owning user TxnId
prev_txn_lsn = prior user WAL LSN
```

when the page change contains user-transaction-owned persistent state whose crash-loser outcome must be tracked.

Pure system/maintenance page changes use:

```text
txn_id = 0
prev_txn_lsn = 0
```

The `PAGE_INIT` or `PAGE_IMAGE` that supplies a `TXN_STATUS` recovery base under §12.10.5 is such a system record. It does not become part of the terminating user's per-transaction WAL chain. The structural existence of a newly initialized page may survive a user abort even when the PAGE_INIT record participates in that user's WAL chain; no physical user-DML undo follows from the header ownership.

`BTREE_MTR`, checkpoint records, and WAL_PAD are system records with header `txn_id=0` / `prev_txn_lsn=0`. A BTREE_MTR's optional owner TxnId remains diagnostic payload only.

## 12.8 PAGE_DELTA

`PAGE_DELTA` payload prefix is exactly 24 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 16 | WAL PageId |
| `16` | 2 | expected `PageType` |
| `18` | 2 | `reserved16 = 0` |
| `20` | 4 | `patch_count` |

`patch_count` MUST be at least `1`. Writers MUST NOT emit a zero-patch
`PAGE_DELTA`. A decoder that encounters `patch_count = 0` in an otherwise
complete recognized-v1 `PAGE_DELTA` MUST reject it as malformed WAL through the
canonical corruption path in §§4.14 and 13.11. It is not a torn tail, an
unsupported future format, or a legal no-op, and it MUST NOT reach redo or
advance `page_lsn`.

Each patch follows as:

| Size | Field |
|---:|---|
| 2 | page-relative `offset` |
| 2 | `length` |
| 4 | `reserved32 = 0` |
| `length` | after-image bytes |

Patches are serialized in strictly ascending offset order and MUST be nonempty and nonoverlapping. For every patch: `offset + length <= 8192` using checked arithmetic.

Delta patches MUST NOT overlap common-header bytes `8..19` (`page_lsn` and checksum). Recovery manages those fields centrally.

The mutation code must ensure the nonempty patch set describes every other persistent byte changed by that logical page mutation. `PAGE_DELTA` represents an actual existing-page mutation; the architecture does not assign mutation meaning to a framing-only record with no patches.

The payload is valid only when parsing exactly `patch_count` entries consumes exactly `payload_length` bytes; trailing payload bytes are forbidden. Redo applies the delta only when the trusted current page has: `page.page_lsn < record.lsn` then sets: `page.page_lsn = record.lsn`.

The resident checksum becomes stale until normal checksum finalization.

## 12.9 PAGE_INIT and PAGE_IMAGE

`PAGE_INIT` and `PAGE_IMAGE` have the same exact payload layout:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 16 | WAL PageId |
| `16` | 2 | expected `PageType` |
| `18` | 2 | `reserved16 = 0` |
| `20` | 4 | `image_length = 8192` |
| `24` | 8192 | complete canonical page after-image |
|  | **8216** | **payload** |

The embedded full page image must itself satisfy:

```text
page_no   = WAL PageId.page_no
page_type = expected PageType
page_lsn  = owning WAL record LSN
checksum  = valid whole-page CRC32C
```

`PAGE_INIT` publishes/reconstructs one newly initialized non-B+ page. `PAGE_IMAGE` replaces the complete state of an already-published page.

## 12.10 Full-page-image invariant and torn-page protection

The architecture requires a complete page image whenever either condition is true:

```text
A. a WAL-protected page transitions clean -> dirty

or

B. this is the first WAL-protected modification of that page
   after the most recently completed checkpoint FPI epoch
```

For an ordinary non-B+ page, that modification uses `PAGE_IMAGE` (or `PAGE_INIT` for initial publication). For a B+ MTR, the MTR uses a full-image affected-page entry for each page satisfying either condition.

The clean-to-dirty rule is the critical WAL-retention invariant:

> Every dirty interval begins at a WAL record that contains a complete recoverable image of that page.

Therefore the frame's `rec_lsn` always identifies a retained recovery image while the frame is dirty. Later changes during the same dirty interval may use deltas unless condition B independently requires another checkpoint-epoch image.

### 12.10.1 BufferPool recLSN consequence

On clean -> dirty at full-image LSN `X`:

```text
dirty   = true
rec_lsn = X
```

Already-dirty later modifications preserve the existing `rec_lsn` even when a later checkpoint-epoch full image is emitted. WAL retention through the minimum DPT `rec_lsn` therefore retains at least one complete recovery image for every dirty page.

### 12.10.2 B+ tree mini-transactions

Every physical B+ mutation executes as a system `BTREE_MTR`, including ordinary leaf insert/erase, split, redistribution, merge, root replacement, and tree-local free-list modification.

The exact payload prefix is 16 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | diagnostic `owner_txn_id` or `INVALID_TXN_ID` |
| `8` | 4 | `page_count` |
| `12` | 4 | `reserved32 = 0` |

`page_count` MUST be at least `1`. Writers MUST NOT emit a zero-page
`BTREE_MTR`. A decoder that encounters `page_count = 0` in an otherwise
complete recognized-v1 `BTREE_MTR` MUST reject it as malformed WAL through the
canonical corruption path in §§4.14 and 13.11. It is not an empty structural
transaction, a legal no-op, a barrier, or an unsupported future format.

Affected-page entries are serialized in ascending `(file_id,page_no)` order and every PageId appears at most once.

Each affected-page entry begins with exactly 24 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 16 | WAL PageId |
| `16` | 2 | expected B+ `PageType` |
| `18` | 1 | encoding |
| `19` | 1 | `reserved8 = 0` |
| `20` | 4 | `data_length` |

Encoding codes are:

```text
1 = FULL_IMAGE
2 = PATCH_SET
```

These are the complete `BTREE_MTR` affected-page encoding registry. Encoding
`0` or any other value in a record using the known `BTREE_MTR` grammar is
WAL corruption; it is not an ignorable nested extension. Likewise the entry
`reserved8`, PATCH_SET `reserved32`, and every unassigned patch/header flag are
RESERVED_ZERO under §4.14. A future affected-page encoding requires a new
owning WAL grammar/record type that a v1 reader can reject before interpreting
its entries.

For `FULL_IMAGE`: `data_length = 8192` followed by one canonical page image whose `page_lsn = mtr.lsn` and checksum is valid.

For `PATCH_SET`, the data begins with:

```text
uint32 patch_count
uint32 reserved32 = 0
```

`patch_count` MUST be at least `1`. Writers MUST NOT emit a zero-patch
`PATCH_SET`; a decoder MUST reject that form as malformed recognized-v1 WAL
through the canonical corruption path. Because the containing recognized-v1
record is complete, this violation is neither a torn tail nor an unsupported
future format. The count is followed by the same patch entry grammar/rules as
PAGE_DELTA. `data_length` must equal exactly
`8 + sum(8 + patch.length)` for those entries.

After exactly `page_count` affected-page entries are parsed, no trailing MTR payload bytes are permitted.

`BTREE_MTR` represents an actual atomic structural mutation with at least one
page or persistent metadata participant, and `PATCH_SET` represents actual byte
patches. The architecture assigns no no-op, heartbeat, position-marker, or barrier semantics
to either forbidden zero-count form.

Persistent allocation/free/root metadata changes are represented by including the affected metadata/free pages in the MTR; there is no second hidden persistent side channel. A newly appended B+ page MUST use `FULL_IMAGE` in the publishing MTR.

Every MTR-modified page receives: `page_lsn = mtr.lsn`.

### 12.10.3 MTR no-flush barrier

While an MTR is being constructed, every affected resident frame is provisional and non-flushable under the canonical §12.12 protocol. The MTR owner captures exact rollback state before the first byte mutation and is the only observer permitted to interpret provisional tree bytes.

The publication sequence is:

```text
1. acquire required tree/root synchronization, append-publication lock(s) for
   any new tail pages, and every page pin/latch in canonical B+ order
2. identify the complete affected page/metadata set; reserve private LOADING
   frames for new pages; wait for active copied writebacks to reconcile; acquire
   every frame/DPT-transition reservation, the one checkpoint-shared
   DPT-publication gate for the complete set, and all fallible rollback/WAL memory
3. capture exact pre-MTR bytes and frame metadata for each existing page,
   pre-MTR root/height/root_generation state, free-list/allocation state, and
   pre-extension physical/published page counts
4. install no-flush ownership on every affected frame before changing bytes
5. install one deterministic valid final tree shape as provisional bytes while
   keeping published page_lsn/generation/dirty/rec_lsn/DPT/FPI metadata and
   ordinary root/reachability publication at their pre-MTR state
6. reserve candidate mtr.lsn, construct every FULL_IMAGE/PATCH_SET entry from
   that final shape, finalize canonical image checksums, and validate the whole
   BTREE_MTR record
7. validly append the complete BTREE_MTR atomically under §12.12.2
8. as one protected MTR publication:
       set every affected resident page_lsn = mtr.lsn
       increment each affected page generation exactly once
       publish dirty/rec_lsn/DPT/FPI metadata from each page's pre-MTR state
       publish new-page counts/reachability and process-local root metadata
       publish all persistent root/sibling/parent/free-list effects together
9. remove no-flush barriers and release transition/page/structural ownership
```

No ordinary observer can see a half-published root, sibling chain, parent link, or free list because the required structural latches and provisional visibility boundary span steps 1–9. Persistent root/free-list metadata pages are ordinary affected MTR pages, while process-local root identity/height/generation publication occurs only in step 8.

If candidate reservation, record construction, or append fails with a known no-append outcome after provisional bytes exist, the MTR owner, while retaining every latch/barrier/reservation:

```text
1. restores every existing affected page's exact pre-MTR 8192 bytes
2. restores its exact PageId/frame identity, page_lsn, dirty flag,
   modification_generation, rec_lsn, DPT membership, and FPI epoch metadata
3. restores process-local root/height/root_generation and allocation/free-list
   metadata exactly
4. removes every failed new page from tree reachability/private LOADING state
5. truncates/verifies every consecutively appended unpublished B+ tail page as
   one §4.11.1.1 rollback unit
6. validates that the complete pre-MTR runtime state is reestablished
7. only then removes no-flush ownership, releases latches/pins, and returns the
   structured construction/WAL/I/O error to the owning operation
```

A page dirty before the MTR therefore remains dirty with exactly its old contents, generation, `page_lsn`, `rec_lsn`, DPT membership, and FPI state after rollback. A clean page returns clean. Rollback neither advances nor resets a generation. Reused free-list pages return to their old free state; newly appended PageNos become reusable only after successful serialized tail truncation. Parent/root/sibling/free-list restoration prevents a page from being simultaneously reachable and free or from acquiring two free-list owners.

After step 7 append success, rollback-and-continue is forbidden. Step 8 must complete or the database storage owner becomes noncontinuable under §12.12.4, retaining barriers rather than exposing a runtime tree inconsistent with a possibly recovery-visible MTR. MTR append does not itself require `fdatasync`; after step 8 the pages are legal ordinary dirty state, and a later WAL-flush failure preserves them while preventing page writeback.

The exact crash/non-crash outcomes are:

| Point | Non-crash failure outcome | Process crash / recovery outcome |
|---|---|---|
| before no-flush acquisition | unchanged tree; release ordinary ownership | unchanged durable tree |
| after no-flush, before mutation | release barrier with unchanged state | volatile ownership disappears; durable tree unchanged |
| after first or all provisional page mutations | exact pre-MTR restoration before release | provisional bytes disappear and were never flushable |
| during record construction/reservation | cancel reservation and restore exactly | provisional bytes disappear; no complete record is replayed |
| known BTREE_MTR append failure | valid WAL end unchanged; restore exactly | incomplete persisted tail is not a record and is ignored |
| valid append success, before publication | complete publication or become noncontinuable | a surviving complete valid MTR is redone atomically; otherwise old durable pages remain |
| publication, before WAL durability | legal dirty tree; durability failure does not roll back | surviving valid MTR redoes the tree; absent MTR implies no affected page write was legal |
| after WAL durability | legal dirty tree | recovery atomically redoes/skips every affected page by page_lsn |
| during later copied writeback | ordinary failure remains dirty; no MTR rollback | torn pages are reconstructed from retained MTR full images and later WAL |
| after stable writeback | normal generation/DPT reconciliation | trusted pages skip reflected MTR actions; redo repairs any older page |

For new B+ pages, a crash before a complete valid MTR leaves only an unpublished tail removed by §4.11.3. A surviving complete MTR makes those pages published recovery targets; recovery re-extends/reconstructs them before tree use. A failed structural MTR can never leave a new root or pointer to an unpublished page visible.

MTR append itself does not require `fdatasync`; later writeback or transaction commit may make it durable.

### 12.10.4 User abort does not reverse B+ shape

User abort leaves valid B+ structural shape in place. Heap/index versions belonging to the aborted transaction remain logically invisible/vacuumable; split/merge/root/free-list system actions are not physically undone.

### 12.10.5 TXN_STATUS full-image and terminal mutation protocol

`TXN_STATUS` pages are ordinary WAL-protected pages for full-image, checksum, dirty-page, checkpoint, and WAL-retention purposes. They are not exempt from §12.10.

The two WAL roles are deliberately separate:

```text
PAGE_INIT/PAGE_IMAGE   = physical page reconstruction base
TXN_COMMIT/TXN_ABORT   = semantic terminal-outcome evidence
```

A status-page full image may preserve terminal bits whose semantic terminal records were established earlier and have since become recyclable. It MUST NOT establish the **new** terminal outcome for the update that caused the image: that pending outcome appears only in the later `TXN_COMMIT`/`TXN_ABORT` record and its subsequent page mutation.

#### 12.10.5.1 Canonical mutation order

Terminal updates to one resident status page are serialized by that page's write latch. The latch covers WAL record ordering and installation of the corresponding status bits so that status-page `page_lsn` advances in the same order as mutations reflected in the page.

For one terminal update at terminal-record LSN `T`:

```text
1. locate/pin the target status page; acquire its write latch and the
   frame/DPT-transition synchronization used by checkpoint capture
2. determine whether §12.10 condition A or B requires a full image
3. if a full image is required:
       reserve image LSN F
       construct a complete image of the current pre-terminal page contents
       set the image's embedded page_lsn = F
       finalize the image checksum
       append a complete system PAGE_IMAGE record at F
4. append the semantic TXN_COMMIT or TXN_ABORT record at T
5. only after the complete terminal record exists in WAL:
       install the target two-bit terminal state
       set the resident status-page page_lsn = T
       update dirty/frame metadata as §12.10.5.2 requires
6. release the frame/DPT-transition synchronization and page write latch/pin
```

When step 3 occurs:

```text
F < T
PAGE_IMAGE.txn_id       = 0
PAGE_IMAGE.prev_txn_lsn = 0
```

The terminal record remains on the user transaction's WAL chain and its `prev_txn_lsn` names the prior **user** WAL record, not `F`.

The full image contains every status already reflected in the page before this terminal update, but it contains neither this update's terminal bits nor any other semantic evidence for this terminal outcome. Its changed embedded `page_lsn=F` makes it the canonical physical after-image of the reconstruction-base installation.

If appending the terminal record fails after the system image was appended, the status bits, resident `page_lsn`, dirty state, `rec_lsn`, and FPI-epoch state remain unmodified by this attempt. The standalone pre-terminal image is redo-safe and semantically inert. An immediate retained-ownership retry reuses it as §4.3.2.4 requires; the one COMMIT-failure-to-ABORT transition may reevaluate and emit the second image covered by terminal headroom, but no path appends an unbounded image series.

Every reservation/append above obeys §12.12. A **known** terminal-record no-append failure therefore returns with the exact pre-terminal page/frame state, while an uncertain append outcome is noncontinuable. Once the terminal record validly appends, step 5 must complete under the retained latch/transition ownership or storage becomes noncontinuable; rolling the status page back and continuing would conflict with terminal evidence that may later become durable. A later WAL durability failure does not make the installed status-page bytes structurally illegal or roll them back; §§39.1.5–39.1.6 define the resulting COMMIT/ABORT runtime and client outcome.

The frame/DPT-transition synchronization is held from before step 2 through the dirty/`rec_lsn`/FPI-epoch publication in step 5. Checkpoint DPT capture uses the same synchronization. Therefore a checkpoint either:

- captures before this transition begins, in which case `F`/`T` are ordered after that checkpoint's BEGIN and remain inside its retained WAL range, or
- captures after the completed transition and includes the dirty page with its existing `rec_lsn`.

It cannot omit a status page whose dirty-interval image precedes the checkpoint retention floor merely because the image was appended while the frame still appeared clean.

A newly allocated status page uses a system `PAGE_INIT` at LSN `I` as its complete reconstruction base before any terminal record for that page. The first terminal record has `I < T`; `rec_lsn=I`, and installing that terminal update advances `page_lsn` to `T` while preserving `rec_lsn`. While that initialized frame remains dirty, `PAGE_INIT` satisfies the checkpoint FPI epoch in which it was emitted; an immediately following terminal update does not require a redundant `PAGE_IMAGE` unless §12.10 condition B has independently become true.

#### 12.10.5.2 Frame metadata, repeat updates, and durability

If the page was clean and step 3 emitted image LSN `F`, successful installation at step 5 performs:

```text
dirty    = true
rec_lsn  = F
page_lsn = T
```

and records that the current checkpoint FPI epoch has been satisfied for the page.

If the page was already dirty:

- a later terminal update normally appends only its terminal record and preserves the existing `rec_lsn`,
- if the first post-checkpoint-epoch modification requires a new image, that image captures all status updates currently reflected in the page, the existing dirty-interval `rec_lsn` is still preserved, and the new epoch is marked satisfied only after step 5 succeeds,
- every successful terminal update sets `page_lsn` to its terminal-record LSN and increments the frame's persistent modification generation.

After a stable-image page write successfully makes the frame clean under §4.12.2/§7.10, `rec_lsn` becomes `INVALID_LSN`. The next terminal update begins a new dirty interval with a new system full image and a new `rec_lsn`.

COMMIT MUST establish durable WAL through `T` before §9.14.1 runtime COMMITTED publication and success return. Ordinary ABORT need not synchronously flush `T`, but the status page cannot be written before WAL is durable through its current `page_lsn`. Thus §12.17 WAL-before-data applies without exception.

Until §9.14.1 terminal publication, the transaction remains nonterminal in the active registry and that runtime state dominates persistent-page lookup under §9.13. Resident status-bit installation before COMMIT's durability wait therefore does not publish COMMITTED to concurrent transactions.

#### 12.10.5.3 Checkpoint, redo, and retention consequences

Checkpoint FPI epochs apply to status pages exactly as to other ordinary pages. The first protected status mutation after the latest completed epoch emits a system full image even if the page is already dirty; the frame keeps its older dirty-interval `rec_lsn`.

Recovery processes the records in WAL LSN order:

- a trusted intact status page skips a page image or terminal update only when its `page_lsn` proves that record and all earlier serialized status-page mutations are already reflected,
- an older trusted page applies the retained `PAGE_INIT`/`PAGE_IMAGE`, then later terminal records,
- a torn/corrupt status page does not contribute a trusted `page_lsn`; recovery restores the retained complete image and then applies later terminal records,
- applying an image without a following terminal record leaves the target transaction nonterminal in that image; analysis/loser resolution establishes ABORTED when required,
- after an image has been applied but a later terminal record has not, terminal redo applies the two-bit outcome and sets `page_lsn` to the terminal-record LSN.

Redo of a terminal record maps its TxnId to the canonical §9.12 status page/bit field, writes exactly the semantic terminal code, and sets that page's `page_lsn` to the terminal-record LSN. It does not reinterpret the preceding full image as terminal evidence.

The DPT `rec_lsn` for a dirty status page therefore always identifies a retained complete image (`PAGE_INIT` or `PAGE_IMAGE`). WAL recycling MUST retain that image and every later required terminal record until a checkpoint/data-page state makes them unnecessary under §13.10. Once a status page is durably clean with a valid checksum and an installed checkpoint permits older WAL recycling, its disk image is the complete recovery base; any later dirty interval emits a new image containing all retained status bits. Ancient terminal records are therefore not required solely to reconstruct a later dirty interval.

Status pages wholly below the durable transaction-status reclamation cutoff retain the skip/retirement semantics of §13.12–§14.14; this protocol does not recreate retired sparse status history.

#### 12.10.5.4 Canonical crash outcomes

For one terminal update that requires image `F` followed by terminal record `T`, recovery outcomes are:

1. Crash before `F` is appended: no status-page mutation exists; the transaction remains nonterminal and loser resolution establishes ABORTED if needed.
2. Crash after `F` is appended but before `T`: replaying `F` restores only the pre-terminal page; it cannot publish a terminal outcome, and loser resolution establishes ABORTED.
3. Crash after `T` is appended but before an explicit WAL flush: the resident page cannot have reached its data file unless WAL-before-data first made `T` durable. If `T` is absent from the valid durable WAL tail, the transaction is a loser; if `T` survived as a complete valid record, recovery applies its semantic outcome.
4. Crash after `T` is durable but before the status bits are installed: redo applies `T` to the image/base page and establishes the terminal outcome.
5. Crash after status-bit installation but before page flush: redo applies `F`/the earlier retained base and then `T` as necessary.
6. Crash during a torn status-page write: checksum failure makes the stored `page_lsn` untrusted; redo restores the retained image and applies later terminal records.
7. Crash after a successful status-page flush: WAL-before-data guarantees durability through the flushed `page_lsn`; a trusted page skips already-reflected image/terminal records.
8. Crash after checkpoint installation and permitted WAL recycling: a still-dirty page retains its image through its DPT `rec_lsn`; a clean page's valid data-file image is the base, and its next dirty interval cannot begin without a new image.

These outcomes apply equally to ordinary `TXN_ABORT` and recovery-generated `TXN_ABORT`; only COMMIT additionally requires durable `T` before runtime terminal publication and success return.

## 12.11 Heap redo before index MTR

When a DML operation creates a new physical heap tuple version and B+ entries that reference its RID:

```text
first establish/log heap tuple-version redo
then append the B+ MTR(s) that reference that RID
```

Therefore: `heap redo LSN < later index MTR LSN` for the relevant dependency.

If an index page is forced before user commit, flushing WAL through the later index MTR also makes the earlier heap redo record durable. The user transaction may still abort; MVCC status handles visibility.

## 12.12 WAL append atomicity and runtime page publication

This section is the canonical non-crash failure contract for WAL-backed page mutation. It distinguishes:

```text
candidate LSN reservation:
    exclusive process-local right to try constructing the next record

validly appended record:
    one complete framed/CRC-valid record published in the process-local
    contiguous logical WAL stream

durable record:
    a validly appended record covered by durable_lsn after the required
    segment namespace and content synchronization

provisional page mutation:
    private runtime bytes protected from ordinary observation/writeback and
    still rollback-capable because their required record is not yet appended

published page mutation:
    ordinary dirty runtime state whose complete required WAL record set
    through its publication-authorizing record has validly appended,
    whether or not that record set is durable yet
```

Append success and WAL durability are deliberately different publication points.

An owning protocol may require more than one ordered record. It MUST identify which complete record is the final **publication-authorizing record** for the page mutation. Earlier validly appended preparatory records remain in WAL if a later required append fails, but they must be redo-safe in the exact pre-operation semantic state and do not authorize ordinary page publication by themselves. The pre-terminal status-page `PAGE_IMAGE` followed by the terminal semantic record in §§12.10.5 and 13.13.2 is the canonical example. For single-record PAGE_INIT/PAGE_IMAGE/PAGE_DELTA/BTREE_MTR operations, that record is itself publication-authorizing.

### 12.12.1 LSN reservation and no-hole rule

`WalManager` maintains one process-local contiguous valid append end. Reserving a candidate record range:

- is serialized with every competing reservation/append, or provides equivalent exclusive ownership of the current logical end,
- computes the candidate record-start LSN and complete physical span, including any required §12.3 segment padding,
- may prepare/create the next WAL segment through §12.2.1,
- does **not** advance the valid append end or make an LSN usable by ordinary page/frame metadata,
- does **not** create a legal logical hole if canceled.

Before returning the candidate, reservation also proves §4.3.2.4's mathematical
end/segment bounds and preserves every outstanding transaction terminal-headroom
credit. Capacity credit is not an LSN reservation and cannot make a hole.

The reservation includes any required preceding `WAL_PAD` or all-zero short segment tail. That padding and the target record become part of the valid logical prefix only when the target append publishes successfully; failure leaves the valid append end unchanged even if private buffer bytes or an empty next segment were prepared.

If reservation prepared an exact next segment through §12.2.1 but the record never appends, that namespace-durable empty segment is only the permitted next-contiguous artifact described in §13.11. It neither consumes an LSN nor extends the valid logical WAL stream, and startup may retain/reconcile it under the existing §4.7 rules.

A reserved candidate LSN may appear only in private record-construction buffers and canonical full-image copies that remain inaccessible under §12.12.3 ownership. A resident page's published `page_lsn`, dirty generation, DPT entry, checkpoint metadata, or user transaction's published `last_wal_lsn` MUST NOT advertise a merely reserved LSN. The transaction WAL-chain tail advances only after the complete user record validly appends; canceling a reservation leaves the prior chain tail unchanged.

If reservation, size arithmetic, memory acquisition, or record-shape validation fails, no logical WAL position is consumed. An immediate internal retry while retaining the same reservation may reuse the candidate LSN. Once the reservation is canceled/released, another record may append; a later operation retry obtains the then-current candidate LSN, which may differ. Both cases preserve a contiguous stream with no reserved-but-unwritten gaps.

### 12.12.2 Atomic append success and physical-tail failures

A WAL append succeeds exactly when:

```text
the complete header, payload, CRC, and required zero alignment bytes
have been installed in append-owned memory
AND
the valid append end is atomically advanced across required segment padding
and that one complete record
```

Until that event, the WAL writer and every other observer use the old valid append end and cannot consume partially copied bytes. After it, the record is part of the valid process-local WAL stream in its entirety. There is no architecture-visible state in which half a record is appended.

An implementation may use a private encoded record followed by one mutex-protected copy/publication, a reserved append buffer with an unpublished completion marker, or another mechanism with exactly these outcomes. It may not expose WAL holes or let a page infer append success from LSN reservation alone.

Physical WAL writing occurs after this logical append publication. A short/partial segment `pwrite`, segment-write error, or `fdatasync` error does not retroactively make the in-memory append outcome uncertain: the append buffer retains the exact valid bytes, `durable_lsn` remains at the prior proven prefix, and the writer may rewrite/resynchronize that same range. After crash, §13.11 framing, embedded LSN, zero-padding, and CRC validation accept only the complete valid persisted prefix and ignore/discard an incomplete tail record.

If the running process cannot determine whether its valid append end advanced, or cannot retain/reconstruct the exact bytes assigned to the published range, this is not an ordinary append failure. The WAL/storage owner enters the noncontinuable state in §12.12.4; no caller may guess, release provisional pages as published, or append past an uncertain logical position.

### 12.12.3 Canonical provisional-mutation and publication protocol

Before a WAL-backed operation changes one or more resident pages, it uses this protocol:

```text
1. identify every affected existing/new page and process-local structural
   metadata that can change
2. acquire the required pins, page/structural latches, new-page intents,
   append-publication lock(s), and frame/DPT-transition reservations
3. resolve the copied-writeback race: wait for any earlier writeback to
   reconcile before taking no-flush ownership; do not wait while holding a
   page latch needed by that writeback
4. acquire all fallible rollback/WAL-construction memory practical before
   byte mutation and capture exact pre-operation rollback state
5. install the owning no-flush barrier on every affected frame before any
   resident byte differs from its published generation
6. stage or install the deterministic final data/structure bytes as
   provisional state; published page_lsn/generation/dirty/rec_lsn/DPT/FPI
   metadata still describes the pre-operation generation
7. construct and validate the complete required WAL record(s), using a
   candidate LSN only in private/provisional state, then validly append them
8. only after every required record append succeeds, publish under the held
   page/structural and frame/DPT synchronization:
       final resident page bytes and authoritative page_lsn
       exactly one new modification generation per affected page
       dirty state and clean-to-dirty rec_lsn/DPT insertion
       preserved rec_lsn for pages already dirty
       checkpoint/FPI epoch state
       any new-page count/reachability and root/free-list runtime metadata
9. remove no-flush barriers and release ownership only after the complete
   publication is visible
```

Step 8 is the architecture-level volatile mutation publication point. Before it, only the operation and explicitly cooperating participants inside the same exclusive MTR/page-mutation ownership domain may inspect provisional bytes. Ordinary Fetch/page guards, scans, tree searches, checkpoint capture, background/explicit flush, eviction, and unrelated writers/readers observe either the complete pre-operation state or the complete published post-WAL state.

The owning page/MTR operation installs and releases no-flush through BufferPool's frame-transition ownership; BufferPool never guesses that ownership from changed bytes. No-flush ownership and `WRITEBACK_IN_PROGRESS` are mutually exclusive. If writeback reserves first, mutation waits for its copy/I/O/reconciliation to finish and captures rollback state afterward; a reported writeback failure causes this mutation attempt to return unchanged without acquiring provisional ownership. If no-flush reserves first, no copied writeback or eviction writeback may begin until publication or completed rollback releases it. Checkpoint DPT capture uses the same DPT-publication gate as mutation publication. A single-page mutation serializes its one transition; a multi-page MTR holds the gate across publication of its complete affected set, so capture observes either old frame/DPT state or fully published WAL-backed state and never a mixed subset.

Rollback support is runtime-only. For each existing page it retains exact pre-operation page bytes plus its PageId/frame identity token, `page_lsn`, dirty flag, modification generation, `rec_lsn`, DPT membership, FPI/checkpoint metadata, and any other owning process-local state needed for exact restoration. Multi-page operations also retain exact pre-operation root/height/generation and allocation/free-list runtime metadata. This is not persisted before-image WAL, transaction undo, ARIES undo, or a CLR.

Publication after append is designed to require no new allocation or fallible filesystem operation. Once the final publication-authorizing record validly appends, the owner MUST complete/retry step 8 while retaining ownership; it MUST NOT restore the old runtime state and continue, because that record may later become durable and recovery-visible. If a later required record has a known no-append failure before that point, exact rollback is permitted only under an owning protocol that proves every earlier appended preparatory record redo-safe for the restored state, as §12.10.5 does. Failure to establish complete post-authorizing-append publication is noncontinuable under §12.12.4.

### 12.12.4 Failure classes, retry, rollback, and escalation

The observable failure classes are:

| Failure class | Required storage-level outcome |
|---|---|
| WAL reservation/resource failure | no valid range consumed; fail before mutation where practical, otherwise restore exact pre-operation state |
| WAL construction/encoding/overflow/validation failure | no valid record; restore if provisional mutation began |
| known append failure before append publication | valid append end unchanged; restore exact pre-operation state |
| append outcome or valid-end ownership uncertain | noncontinuable; do not publish, roll back-and-continue, or append further |
| WAL write/flush/durability failure after valid append | keep published page mutation as legal dirty state, do not advance `durable_lsn`, forbid dependent page writeback, retain bytes and retry durability when exact retry is possible |
| physical page/file extension failure | restore/verify pre-extension length while append serialization is retained; uncertainty or cleanup failure is noncontinuable |
| BufferPool/page publication failure before valid append | restore/remove private state and return the error |
| BufferPool/page publication failure after the publication-authorizing append | complete/retry publication under ownership or become noncontinuable |
| page initialization failure | fail before append and perform §4.11.1.1 cleanup; post-append initialization/publication failure is noncontinuable |
| MTR construction failure before mutation | release unchanged ownership/state |
| MTR failure after provisional mutation but before known append success | restore every affected page/metadata item exactly, then release barriers |
| process crash | volatile ownership/provisional bytes disappear; recovery uses only the complete valid persisted WAL prefix and data files |

A failure is **locally recoverable** only when the operation proves either that no runtime mutation occurred or that exact pre-operation runtime state has been restored. Restoration of existing frames is memory restoration under retained exclusive ownership and performs no filesystem I/O. It restores a preexisting dirty page as the same dirty bytes/generation/`page_lsn`/`rec_lsn`/DPT/FPI state; it never converts that page to clean. A failed provisional mutation does not consume a modification generation. The sole fallible physical rollback is truncation/verification of newly appended unpublished tail pages under §4.11.1.1.

If restoration cannot be established, a required ownership invariant is uncertain, tail cleanup fails, or an append/durability range cannot be reconciled exactly, the affected database storage owner becomes **noncontinuable**:

- no further ordinary mutation, WAL append past the uncertain point, page publication, writeback of affected provisional frames, or clean shutdown claim may proceed,
- affected latches/reservations/barriers are not released as ordinary usable state,
- the database owner surfaces the storage failure and performs a non-clean controlled stop,
- the next open runs normal crash recovery from the valid persisted WAL/data prefix before ordinary traffic.

The database-level admission closure, owner-lock retention, non-clean teardown,
and fresh-open requirement are §3.3.5. This storage transition cannot be
downgraded by lifecycle cleanup.

Rollback failure is therefore never hidden by releasing incoherent pages. This section fixes storage state; §39.1 consumes its local-recoverable/noncontinuable result without weakening these ownership rules.

The storage layer automatically retries syscall-level `EINTR` as specified in §7.4.3. It MAY retry another operation internally only while retaining every reservation/latch/barrier needed to keep all attempts unobservable and only when append/physical outcomes are known. Deterministic encoding/resource exhaustion, returned append errors, and WAL durability errors are not SQL statement retries; they return their structured lower-layer error when internal retry ends.

### 12.12.5 WAL append buffer

`WalManager` owns an in-memory append buffer that retains the exact bytes needed
for contiguous logical append, physical WAL writing, durability, and exact
retry after a valid append. Its runtime data structures, capacity policy, and
synchronization strategy are not architecturally fixed. Every realization MUST
preserve §§12.12.1–12.12.4 exactly, including private record construction,
total append order, no holes, atomic valid-end publication, retained-byte
lifetime, concurrent append/flush safety, shutdown draining, and uncertainty
handling. Lock-free WAL append is not required for correctness.

## 12.13 WAL writer and durable LSN

A dedicated WAL writer/flusher:

- writes contiguous WAL bytes to segment files,
- executes `fdatasync` when a durability request requires it,
- advances a monotonic atomic `durable_lsn`,
- wakes waiters whose target record LSN has become durable.

`durable_lsn >= X` means the complete valid WAL record whose LSN is `X`, and all preceding required WAL bytes, are durable. If `X` lies in a segment created during this process lifetime, this statement also proves that segment's final directory entry was synchronized under §12.2.1 before `durable_lsn` advanced into it.

A durability request spanning a segment boundary synchronizes every segment containing previously unsynchronized bytes in the requested logical prefix. Synchronizing only the newest segment cannot advance `durable_lsn` past unsynchronized bytes in an older segment.

Background WAL may be written periodically without an explicit commit request.

The scheduling mechanism and cadence for background WAL writing are not
architecturally fixed.

The WAL subsystem, not DiskManager's logical API, owns WAL durability scheduling and `durable_lsn`.

## 12.14 Group commit and CommitCoordinator

A normal commit does not execute one independent filesystem synchronization per transaction.

Conceptually, if waiting commits target:

```text
T1 -> 1000
T2 -> 1104
T3 -> 1256
```

the flusher may:

```text
write WAL through the record at 1256
fdatasync
publish durable_lsn >= 1256
wake T1, T2, T3
```

One durability operation may acknowledge many commits. `CommitCoordinator` conceptually exposes `WaitUntilDurable(commit_lsn)` and batches naturally by waiting on monotonic `durable_lsn`, rather than enqueuing one fsync task per transaction.

Commit latency observability should distinguish:

```text
queue delay
WAL write time
fdatasync time
group size
```

## 12.15 Synchronous commit

The architecture has synchronous commit. For a transaction with persistent writes, successful COMMIT cannot return until: `its TXN_COMMIT WAL record is durable`.

If that terminal record resides in a newly created segment, “durable” includes successful prior synchronization of the segment's `wal/` directory entry under §12.2.1. WAL durability is necessary but not sufficient for successful client acknowledgement. The detailed transaction state/status/cache/lock-release and acknowledgement sequence is canonical in §§15.5 and 39.1.5.

Commit does not force the transaction's heap or B+ pages. Asynchronous commit remains deferred and, if introduced, must expose its weaker durability semantics explicitly.

## 12.16 BufferPool recLSN

Every WAL-protected dirty frame tracks `rec_lsn` meaning:

> the LSN of the full page image that began the frame's current clean-to-dirty interval.

Because §12.10 requires a full image on every clean-to-dirty transition, `rec_lsn` is both:

- the first WAL LSN of the current dirty interval,
- an LSN from which the complete page can be reconstructed without trusting the current data-file image.

For `TXN_STATUS`, the full-image LSN and later semantic terminal-record LSN are intentionally distinct as specified by §12.10.5: `rec_lsn` names the image while `page_lsn` advances to the latest terminal record reflected in the page.

Transitions are:

```text
clean -> dirty at full-image LSN X:
    dirty   = true
    rec_lsn = X

already dirty -> later modification:
    preserve rec_lsn
```

After a successful stable-image page write, dirty state can be cleared only if §4.12.2/§7.10 prove no newer frame modification raced the image written.

Only then:

```text
dirty   = false
rec_lsn = INVALID_LSN
```

`rec_lsn` is process-local frame/checkpoint metadata.

## 12.17 WAL-before-data and temporary no-flush state

Before BufferPool writes any WAL-protected dirty page with: `page_lsn = X` it must establish: `WalManager.durable_lsn >= X`.

This applies to BufferPool-managed:

- heap pages,
- transaction-status pages,
- WAL-protected FSM pages,
- catalog pages,
- B+ pages,
- superblock/control-like database pages when routed through BufferPool.

B+ MTR no-flush barriers add a stronger temporary requirement:

```text
the frame is not flushable at all
until the complete BTREE_MTR record exists and page_lsn is installed
```

Only after that barrier is removed does ordinary WAL-before-data writeback apply.

All WAL-backed page mutations use the broader §12.12 provisional-publication rule: no-flush begins before unpublished resident bytes appear, excludes copied writeback/eviction, and ends only after complete post-append publication or exact known-failure rollback. A merely reserved LSN cannot satisfy this section. A valid append permits legal volatile dirty publication; only later WAL durability through that LSN permits data-page writeback.

The complete copied-image, file-synchronization, generation-reconciliation, and failure state machine is canonical in §§7.10–7.11. WAL durability is established before `pwrite`; dirty/DPT clean publication occurs only after the covering page-file `fdatasync` succeeds.

## 12.18 WAL and commit invariants

1. The database has one logical WAL stream segmented into 64 MiB files.
2. Every ordinary record starts at an 8-byte-aligned LSN and has `total_length = 48 + payload_length`.
3. External record-alignment bytes are zero and excluded from CRC/`total_length`.
4. An ordinary logical record never crosses a segment boundary.
5. The WAL header is exactly 48 bytes with zero flags/reserved fields.
6. Persisted record-type codes are explicit and stable.
7. WAL PageId encoding is exactly 16 bytes and independent of ABI layout.
8. PAGE_DELTA contains at least one nonempty patch; its patch entries are canonical, ordered, nonoverlapping, bounds checked, and never patch page_lsn/checksum bytes.
9. PAGE_INIT/PAGE_IMAGE carry canonical complete 8192-byte after-images.
10. Every clean-to-dirty page transition begins with a full page image and sets `rec_lsn` to that image LSN.
11. The first post-checkpoint-epoch modification also carries a full image when required by the FPI epoch rule.
12. Every physical B+ mutation is an atomic redoable MTR system action with at least one affected page; every nested PATCH_SET contains at least one nonempty patch.
13. Every MTR-modified page receives one common MTR LSN.
14. A frame protected by an MTR no-flush barrier cannot be written before the complete MTR record exists.
15. Heap tuple-version redo precedes index MTR redo that references the new RID.
16. `durable_lsn` advances monotonically only after the relevant WAL bytes reach durable storage.
17. Group commit is the normal synchronous commit durability path.
18. Successful COMMIT with persistent writes waits for durable TXN_COMMIT WAL.
19. WAL-protected data-page writeback requires durable WAL through the stable write image's page_lsn.
20. Every persisted ordinary random-access page has a valid whole-page checksum.
21. A `TXN_STATUS` dirty interval has a retained system full-image reconstruction base; `TXN_COMMIT`/`TXN_ABORT` remain separate semantic terminal evidence.
22. Status-page WAL append and page mutation are serialized so a later status-page `page_lsn` implies that all earlier same-page terminal updates are reflected.
23. A WAL segment is namespace-durable before any record inside it may advance `durable_lsn` or satisfy commit/WAL-before-data.
24. Segment creation uses an exact final basename and immediate `fsync(database_root/wal)`; segment content durability still requires `fdatasync` through the requested record.
25. Candidate LSN reservation consumes no logical position and never creates a WAL hole; only one complete framed/CRC-valid append advances the contiguous valid end.
26. A page cannot publish `page_lsn`, dirty generation, `rec_lsn`, or DPT/FPI state from a merely reserved LSN.
27. A known failure before the publication-authorizing append restores exact pre-operation runtime page/metadata state before no-flush ownership releases.
28. Publication-authorizing append success permits legal volatile dirty publication but does not imply durability; a later WAL flush failure preserves that state and blocks dependent data writeback.
29. Append uncertainty, failed exact restoration, or inability to finish post-append publication makes the database storage owner noncontinuable.
30. A publication-authorizing `TXN_COMMIT` append is uncancellable; its durability makes COMMITTED irreversible even before runtime terminal publication.
31. A COMMIT success acknowledgement follows required runtime terminal publication and coherent lock/cache cleanup, not merely `durable_lsn` advancement.
