# 13. Checkpointing and Crash Recovery

## 13.1 Scope and recovery model

Crash recovery is ARIES-inspired in its use of:

- LSNs,
- page LSNs,
- dirty-page `rec_lsn` values,
- fuzzy checkpoints,
- analysis,
- redo,
- transaction outcome reconstruction.

The architecture deliberately replaces generic physical user-DML undo with:

```text
analysis
redo
loser-transaction status resolution
```

Ordinary aborted/crash-loser heap/index bytes may remain physically present. Correctness is restored through terminal transaction status plus MVCC visibility. No ordinary user-DML CLR stream is required.

## 13.2 Database control file

Global recovery/bootstrap metadata lives in exactly `database.control` with total size: `8192 bytes` composed of two alternating 4096-byte slots.

### 13.2.1 Control-slot v1 byte layout

Each slot is independently encoded as:

| Slot offset | Size | Field / meaning |
|---:|---:|---|
| `0` | 8 | ASCII `DBLUSCTL` |
| `8` | 2 | `format_version = 1` |
| `10` | 2 | `header_size = 88` |
| `12` | 4 | `flags = 0` |
| `16` | 8 | `generation` |
| `24` | 8 | `latest_checkpoint_lsn` = installed CHECKPOINT_BEGIN LSN, or `0` |
| `32` | 8 | `latest_checkpoint_end_lsn`, or `0` |
| `40` | 8 | `checkpoint_redo_lsn`, or `0` |
| `48` | 8 | `reserved_txn_id_end` exclusive upper bound |
| `56` | 8 | `txn_status_reclaim_before` |
| `64` | 4 | `next_file_id` |
| `68` | 4 | `reserved32 = 0` |
| `72` | 8 | `next_catalog_object_id` |
| `80` | 4 | `crc32c` |
| `84` | 4 | `reserved32 = 0` |
| `88` | 4008 | reserved bytes, all zero |
|  | **4096** | **total slot** |

All multi-byte integers are little-endian. V1 assigns no control flags. Nonzero `flags` or reserved bytes are rejected. CRC32C is computed over exactly all 4096 slot bytes with bytes `80..83` logically zero.

### 13.2.2 Initial valid state

A freshly initialized database writes:

```text
slot 0:
    generation                 = 1
    latest_checkpoint_lsn      = 0
    latest_checkpoint_end_lsn  = 0
    checkpoint_redo_lsn        = 0
    reserved_txn_id_end        = FIRST_NORMAL_TXN_ID = 2
    txn_status_reclaim_before  = FIRST_NORMAL_TXN_ID = 2
    next_file_id               = 1
    next_catalog_object_id     = 1
    valid CRC

slot 1:
    all zero bytes
    therefore invalid as a control slot
```

`FileId{0}` remains invalid.

### 13.2.3 Validation and slot selection

Startup validates each slot independently:

```text
magic
format_version
header_size
zero flags/reserved bytes
CRC32C
nonzero generation
reserved_txn_id_end >= FIRST_NORMAL_TXN_ID
txn_status_reclaim_before >= FIRST_NORMAL_TXN_ID
txn_status_reclaim_before <= reserved_txn_id_end
(txn_status_reclaim_before - FIRST_NORMAL_TXN_ID) % 32640 == 0
next_file_id != INVALID_FILE_ID
next_catalog_object_id != 0
checkpoint field consistency
```

For no installed checkpoint:

```text
latest_checkpoint_lsn     = 0
latest_checkpoint_end_lsn = 0
checkpoint_redo_lsn       = 0
```

Otherwise all three are nonzero and: `latest_checkpoint_lsn <= latest_checkpoint_end_lsn`.

During ordinary open, choose the structurally valid slot with the highest `generation`. During crash recovery, a checkpoint-bearing slot is usable only if its referenced checkpoint sequence is still present and validates; candidates may be considered in descending generation order.

Falling back to an older structurally valid slot is legal only when every recovery object it references is still retained and valid.

Lifecycle format dispatch applies before this fallback under §4.14.6. Exact control-family magic with `format_version>1` causes `UNSUPPORTED_DATABASE_FORMAT` and forbids selecting an older v1 slot; v1 cannot validate or compare the future generation/layout. Version zero is an invalid/corrupt slot and remains eligible only for the ordinary independent-slot fallback rules, never as a future format.

Equal-generation valid slots with different contents are corruption. Generation is uint64 monotonic and MUST NOT wrap. An update when the selected generation is `UINT64_MAX` fails rather than wrapping.

### 13.2.4 Alternating update protocol

All control-state updates are serialized by one control-file update mutex and are constructed from the latest selected in-memory control state so unrelated fields cannot be lost by concurrent TxnId/FileId/checkpoint/reclamation updates.

To persist a new control state:

```text
1. choose the slot other than the currently selected valid slot
2. construct a complete new slot with generation = old_generation + 1
3. compute its CRC
4. pwrite exactly that 4096-byte slot
5. fdatasync(database.control)
6. only after successful sync publish that generation as current in memory
```

The previous valid slot remains a fallback if the new write tears.

### 13.2.5 FileId allocation

`next_file_id` is the next unassigned database FileId.

FileId allocation is serialized through the control-file update path:

```text
candidate = next_file_id
new_next  = candidate + 1
persist/sync new_next in a new control slot
only then return candidate
```

FileId allocation therefore tolerates crash gaps but never reuses a FileId that may have been published.

The exact last candidate is `UINT32_MAX-1` under §4.3.2.1. If increment would
overflow uint32 or produce `INVALID_FILE_ID`, allocation fails rather than wrapping.

### 13.2.6 Catalog-object ID allocation

`next_catalog_object_id` is the next unassigned value in one database-wide uint64 catalog-object namespace.

This allocator is used for:

```text
TableId
IndexId
ConstraintId
```

The type wrappers remain distinct even though their numeric values come from one common allocator.

Allocation is serialized through the same alternating control-slot update path:

```text
candidate = next_catalog_object_id
new_next  = candidate + 1

durably publish new_next
only then return candidate
```

`0` is never assigned by this allocator. Crash gaps are acceptable. A catalog-object ID that may have appeared in persistent state is never reused, including after transaction abort. If increment would overflow uint64 or produce `0`, allocation fails rather than wrapping.

The exact last candidate is `UINT64_MAX-1`; the carrier maximum remains the
persisted exhausted `next_catalog_object_id` and is never returned as an object ID.

`ColumnId` and built-in `TypeId` do not use this allocator.

## 13.3 Control-file update frequency

`database.control` is durably rewritten only for important global metadata transitions such as:

- durable transaction-ID block reservation,
- durable FileId or catalog-object-ID allocation,
- successful checkpoint installation,
- future database-format/global metadata changes.

Ordinary transactions do not rewrite/synchronize the control file. This keeps commit scalability independent of control-file update latency.

## 13.4 Fuzzy checkpoint goal

A checkpoint does not stop transaction processing merely to force every dirty page. It captures enough WAL/recovery state to bound later crash recovery while ordinary transactions and page modifications continue. A completed fuzzy checkpoint does **not** imply that all pages dirtied before it are already in their data files.

## 13.5 Checkpoint identity and capture protocol

A checkpoint is identified persistently by: `checkpoint_id = CHECKPOINT_BEGIN.lsn`.

The installed `database.control.latest_checkpoint_lsn` is exactly that BEGIN LSN.

The conceptual protocol is:

```text
1. append CHECKPOINT_BEGIN at LSN B
2. capture DPT + active writer state under their required short synchronization
3. append CHECKPOINT_DATA chunks, each referencing B
4. append CHECKPOINT_END referencing B
5. flush WAL through CHECKPOINT_END
6. persist database.control:
       latest_checkpoint_lsn     = B
       latest_checkpoint_end_lsn = END.lsn
       checkpoint_redo_lsn       = computed redo bound
7. fdatasync database.control
8. only then mark the new checkpoint/FPI epoch complete in memory
```

BEGIN/DATA/END record construction, DATA chunk/count totals, control generation,
LSNs, and the process-local FPI epoch all apply §4.3.2 before any checkpoint
record or control update is published. An oversized/exhausted checkpoint is not
installed; earlier complete inert checkpoint records may remain in WAL.

Checkpointing remains fuzzy and does not force all dirty pages.
The recovery-completion checkpoint and controlled-close final checkpoint are
ordered by §3.3.3 and §3.3.6 respectively and use this same format/protocol.
Controlled close first establishes no active writer and an empty DPT; that does
not create a separate clean-shutdown flag or checkpoint format.

## 13.6 CHECKPOINT_BEGIN payload

`CHECKPOINT_BEGIN` is a system WAL record (`txn_id=0`, `prev_txn_lsn=0`) with exact 32-byte payload:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | previous installed checkpoint BEGIN LSN, or `0` |
| `8` | 8 | captured `next_txn_id` high-water value |
| `16` | 8 | captured `reserved_txn_id_end` |
| `24` | 4 | captured `next_file_id` |
| `28` | 4 | `reserved32 = 0` |

The record's own LSN is the checkpoint ID.

## 13.7 CHECKPOINT_DATA payload and dirty-page/writer entries

One checkpoint may use zero or more `CHECKPOINT_DATA` system records (`txn_id=0`, `prev_txn_lsn=0`).

Each DATA payload starts with exactly 24 bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | `checkpoint_begin_lsn` |
| `8` | 4 | zero-based `chunk_index` |
| `12` | 4 | `dpt_count` |
| `16` | 4 | `writer_count` |
| `20` | 4 | `reserved32 = 0` |

Then come exactly `dpt_count` DPT entries, each 24 bytes:

```text
16-byte WAL PageId
8-byte rec_lsn
```

followed by exactly `writer_count` active-writer entries, each 16 bytes:

```text
8-byte TxnId
8-byte last_wal_lsn
```

No internal padding exists between entries. A DPT snapshot includes every page already dirty before CHECKPOINT_BEGIN that remains dirty when captured, plus any page whose synchronized capture rules require inclusion. Each `rec_lsn` retains §12.16 semantics and therefore identifies a full recovery image.

BufferPool dirty publication, stable-clean removal, and this capture use the canonical §7.10.5 frame/DPT-transition synchronization. Capture may conservatively retain an entry for a page made durably clean immediately afterward; it MUST NOT miss a clean-to-dirty transition whose recovery image would otherwise precede the installed retention range.

The active-writer table includes every transaction that has produced persistent WAL state and is nonterminal at capture. A transaction with no persistent writes need not appear.

## 13.8 CHECKPOINT_END payload and completeness validation

`CHECKPOINT_END` is a system record (`txn_id=0`, `prev_txn_lsn=0`) with exactly 32 payload bytes:

| Offset | Size | Field |
|---:|---:|---|
| `0` | 8 | `checkpoint_begin_lsn` |
| `8` | 4 | `data_record_count` |
| `12` | 4 | total DPT entries across DATA chunks |
| `16` | 4 | total writer entries across DATA chunks |
| `20` | 4 | `reserved32 = 0` |
| `24` | 8 | `checkpoint_redo_lsn` |

A checkpoint sequence is complete only when recovery can validate all of:

```text
BEGIN exists and its LSN == checkpoint_begin_lsn
END references that same BEGIN LSN
DATA records referencing that BEGIN have chunk_index exactly 0..N-1
N == END.data_record_count
summed DPT/writer counts equal END totals
all record CRC/length rules are valid
END.checkpoint_redo_lsn is valid for the reconstructed DPT
```

An incomplete, duplicate-index, mismatched-count, or cross-linked sequence is not installable and is ignored unless database.control incorrectly points to it, in which case startup reports control/checkpoint corruption and may fall back only to another independently valid control generation.

## 13.9 Checkpoint redo bound

The checkpoint redo bound is:

```text
if captured DPT nonempty:
    checkpoint_redo_lsn = minimum captured rec_lsn
else:
    checkpoint_redo_lsn = checkpoint_begin_lsn
```

Because every dirty interval begins with a full page image, every nonempty DPT `rec_lsn` is itself a reconstructible page-image LSN. `checkpoint_redo_lsn` may be earlier than `checkpoint_begin_lsn`.

## 13.10 WAL recycling

The architecture has no replication/PITR retention requirement.

The oldest installed recovery LSN is:

```text
wal_retention_floor = min(
    database.control.latest_checkpoint_lsn,
    database.control.checkpoint_redo_lsn
)
```

when a checkpoint is installed. No WAL segment containing any byte at or after this floor may be recycled. Also retain every segment containing the installed BEGIN/DATA/END sequence through `latest_checkpoint_end_lsn`. Because each dirty page's `rec_lsn` identifies a full image, retaining from `checkpoint_redo_lsn` retains a complete reconstructible base for every dirty page captured by the installed checkpoint.

WAL recycling is conservative: retaining too much is permitted; deleting required WAL is corruption.

Once a segment is wholly older than every retention requirement, physical recycling is:

```text
unlinkat(database_root/wal, exact_segment_basename)
fsync(database_root/wal)
```

Only the successful directory sync makes segment removal durable. A crash after unlink but before that sync may make the old segment reappear; startup ignores/re-unlinks such a segment only after proving it is wholly below the installed retention floor. Reappearance consumes space but cannot extend the valid logical WAL range or override a newer segment index.

## 13.11 Recovery startup and WAL-tail validation

Startup executes only after §3.3.2 has established exclusive process/database
ownership. The complete manager/catalog/status/READY dependency order is
§3.3.3; within its recovery portion:

1. opens/validates the final database root plus required `pending/` and `wal/` directories under §4.7,
2. inventories exact managed pending/final object basenames without yet treating final-file existence as catalog ownership,
3. validates required bootstrap singleton/system-relation names and both control slots, then chooses the highest usable control generation,
4. validates the referenced installed checkpoint sequence if nonzero,
5. validates the exact retained WAL segment-name sequence and scans WAL forward to identify the last complete valid record,
6. validates record length, alignment, segment containment, zero padding, embedded LSN, flags/reserved rules, and CRC,
7. stops at the first invalid/incomplete/torn **tail** record,
8. logically discards/zeroes bytes after the last valid WAL record while preserving the fixed 64 MiB segment-file length,
9. reconciles unpublished page-file append tails per §4.11.3 during recovery,
10. completes final ownership/orphan reconciliation at the §13.19 recovery gate.

The scanner requires segment `0` bytes `0..7` to be zero and begins record
framing at LSN `8`; every later segment begins at offset `0`. An empty database
therefore reconstructs append end `8`, not invalid LSN `0`. End arithmetic and
the terminal one-past exhausted condition are §4.3.2.4.

Every segment needed from the installed recovery start through the valid WAL tail has its exact §12.2 basename and no missing interior segment index. An extra next-contiguous all-zero segment durably created under §12.2.1 is an empty tail, not corruption. Segments proven wholly older than the installed retention floor may be absent or may be re-unlinked if a precrash unlink was not directory-durable.

The scan reconstructs the same contiguous no-hole valid prefix defined by §12.12. A candidate reservation or partially copied/written record never advances that prefix; recovery either recognizes the complete record in full or excludes it and every later byte. A restart after the §12.12.4 noncontinuable gate is therefore an ordinary crash-recovery open, not continuation from uncertain in-memory publication state.

A next-contiguous empty/short segment left by a failed creation attempt, containing no valid WAL record beyond the prior durable logical end, is an unacknowledged namespace artifact: recovery may unlink/synchronize and recreate it through §12.2.1. A malformed, short, or missing segment inside the required retained WAL range is corruption.

Malformed required WAL before the valid-tail boundary is corruption, not ordinary crash-tail truncation.

An otherwise complete, correctly framed and CRC-valid record with an unknown `record_type` is not a torn tail. It is `UNSUPPORTED_WAL_FORMAT` under §4.14/§12.7 and stops recovery before READY; recovery never skips it by using `total_length`.

## 13.12 Recovery phase 1: analysis

When an installed checkpoint exists:

```text
1. validate/load its complete CHECKPOINT_DATA state
2. initialize reconstructed DPT and active-writer table from that state
3. discard any checkpoint DPT entry for a TXN_STATUS page wholly below the durable `txn_status_reclaim_before` cutoff
4. initialize allocator high-water metadata from BEGIN/control information
5. scan WAL forward starting at checkpoint_begin_lsn
```

Starting at BEGIN is required because transactions/pages can change while the fuzzy checkpoint DATA records themselves are being emitted.

During the forward analysis scan:

- a redoable page record, including a system status-page full image from §12.10.5, inserts/updates DPT information without replacing an earlier required `rec_lsn`,
- each BTREE_MTR does the same for every affected page,
- TXN_COMMIT marks its transaction terminal COMMITTED,
- TXN_ABORT marks it terminal ABORTED,
- later activity updates active/loser writer state,
- observed TxnIds advance the recovered high-water requirement.

A writer that remains nonterminal at analysis end is a crash loser. When no checkpoint exists, analysis starts at the oldest retained WAL record required for database creation/recovery.

## 13.13 Recovery phase 2: redo

Redo begins at `database.control.checkpoint_redo_lsn` for an installed checkpoint, or the earliest required WAL LSN otherwise.

This redo scan may therefore begin **before CHECKPOINT_BEGIN**. That is intentional.

### 13.13.1 Page redo

For PAGE_INIT/PAGE_IMAGE/PAGE_DELTA and BTREE_MTR page entries, apply the
page-record semantics in §§12.8–12.10.3 using §4.8.2's canonical `page_lsn`
meaning. The WAL-before-data rule in §12.17 remains applicable to any resulting
dirty-page writeback.

A redo action targeting a TXN_STATUS page wholly below the durable reclaim cutoff is skipped as retired history. A trusted valid page whose `page_lsn >= record.lsn` skips that page's redo action.

Recovery MUST NOT fail merely because WAL references a missing FileId that is later proven to belong only to uncommitted/private DDL. Redo for an unresolved non-bootstrap FileId may be deferred until catalog ownership is known. It is skipped only when §4.7.6 proves the target unpublished/orphaned; if committed catalog or bootstrap state requires the file, absence is corruption.

### 13.13.2 Terminal status redo

`TXN_COMMIT` and `TXN_ABORT` are also redoable terminal-status evidence.

When encountered anywhere in the redo scan, including before CHECKPOINT_BEGIN, recovery repairs the corresponding transaction-status entry if its page does not yet reflect that terminal record **and** the TxnId is not below `database.control.txn_status_reclaim_before`.

The canonical physical base and ordering are §12.10.5. A retained system `PAGE_INIT`/`PAGE_IMAGE` reconstructs an older, missing, or untrusted status page before a later terminal record is applied. Terminal redo maps the record's TxnId through §9.12, installs exactly `COMMITTED` or `ABORTED`, and sets the status-page `page_lsn` to the terminal-record LSN.

For an intact trusted status page, the same-page serialization rule permits `page_lsn >= terminal_record.lsn` to skip that terminal update. A torn/corrupt page's stored LSN is never used for this decision. Terminal records below the durable reclaim cutoff are logically retired history and do not recreate punched status pages.

This is required for a retained dirty transaction-status page whose `rec_lsn` predates checkpoint begin and whose terminal status was never forced to the status file.

### 13.13.3 BTREE_MTR atomicity

A complete valid BTREE_MTR is one committed system mini-transaction for recovery. For each affected page with older/untrusted state, apply its full image or patches and set `page_lsn = mtr.lsn`. A torn/incomplete MTR record is not a record and is never partly replayed.

Runtime provisional MTR bytes are irrelevant after crash. If the complete MTR belongs to the valid persisted WAL prefix, redo may reconstruct every affected page/new allocation and root/free-list effect; otherwise recovery retains the pre-MTR durable tree and removes any unpublished appended tail under §4.11.3.

Recovery redo reproduces already-authorized physical heap/index mutations and MUST NOT acquire `UNIQUE_KEY` locks or invoke §11.10 as a fresh SQL constraint check. Page-LSN idempotence and complete `BTREE_MTR` replay handle an already-reflected `(user_key,RID)`. Aborted/loser-created physical entries may remain after redo and are semantically ignored by transaction status until vacuum; their presence is not a recovery-time uniqueness failure.

## 13.14 Torn/corrupt data pages during redo

If a data-page checksum is invalid during recovery: `do not trust its stored page_lsn`.

Recovery must find an applicable retained complete page image from:

```text
PAGE_INIT
PAGE_IMAGE
full-image portion of BTREE_MTR
```

restore that page state, then apply later valid deltas/MTR changes.

For a `TXN_STATUS` page, the applicable retained complete image is the system `PAGE_INIT`/`PAGE_IMAGE` required by §12.10.5; later `TXN_COMMIT`/`TXN_ABORT` records reestablish semantic outcomes. Recovery does not depend on retaining every ancient terminal record that preceded that complete image.

If WAL-retention/checkpoint invariants say the required full image should exist but no valid recoverable image is available, recovery reports unrecoverable corruption. It MUST NOT guess page contents or trust a torn page's LSN.

This reconstruction is the explicit recovery-only exception to ordinary BufferPool load publication in §7.6.4. Recovery may hold untrusted bytes privately while finding a full image, but the complete reconstructed page—or complete atomic B+ MTR result set—must pass §4.13 L0/L1 and applicable L2 validation before ordinary `RESIDENT` publication. Recovery cannot permanently waive a structural invariant.

## 13.15 Recovery phase 3: loser resolution

For each crash-loser **user** transaction: `terminal outcome = ABORTED`.

No heap/index byte-by-byte undo is performed.

Before normal SQL traffic begins, recovery:

```text
1. applies §12.10.5 for unresolved losers, appending any required
   system status-page image before each page's later recovery TXN_ABORT records
2. updates the relevant transaction-status entries to ABORTED
3. assigns each transaction-status page_lsn from the latest terminal WAL
   record reflected in that page
4. durably flushes the WAL required for those terminal outcomes
5. after §3.3.3 has reconstructed catalog ownership, completed every required
   deferred redo/append-tail reconciliation, and rejected missing required
   files, completes and installs a recovery checkpoint
```

This makes the repaired terminal outcomes durable and avoids repeatedly rediscovering the same loser set on every subsequent restart.

## 13.16 No ordinary user-DML CLRs

Compensation log records are not required for ordinary user transactions because recovery does not physically undo their heap/index DML. The architecture must not add CLRs solely to resemble textbook ARIES terminology. If a future subsystem introduces physical undo that itself must be crash-restartable, that subsystem may introduce an appropriate CLR/undo-progress mechanism.

## 13.17 Recovery of transaction-status pages

`TXN_COMMIT` and `TXN_ABORT` WAL records are authoritative terminal-outcome evidence.

During recovery, terminal records may repair/update stale transaction-status pages even when their prior data-file image did not include the status change. The physical reconstruction base, record ordering, and FPI/checkpoint behavior are canonical in §12.10.5.

When a terminal status record at LSN `X` is reflected into a status page: `status_page.page_lsn = X` where appropriate for that page mutation.

This is why normal commit does not force the status page itself. Successful acknowledgement still waits for §15.5 C4–C5 runtime terminal publication and coherent cleanup after durable commit WAL.

## 13.18 Approximate/rebuildable metadata

Approximate performance metadata does not become a mandatory crash-consistency dependency merely because it is persistent.

Examples allowed to be stale/rebuildable include:

```text
FSM free-space estimates
some optimizer statistics
non-critical pruning hints
```

After recovery such metadata may be repaired:

- lazily,
- by scanning authoritative storage,
- by vacuum/analyze or its owning maintenance subsystem.

Correctness-critical transaction/status/page metadata is not downgraded to this category.

## 13.19 Recovery completion gate

After a crash, the database does not accept normal SQL traffic until all required recovery gates hold:

```text
valid WAL tail established
analysis complete
redo complete
losers published ABORTED
transaction status consistent
B+ structural MTRs recovered
control metadata valid
every bootstrap/committed-catalog physical file present at its exact durable final name
all required deferred redo and append-tail reconciliation complete
recovery checkpoint installed
pending and unowned managed-final files classified for durable orphan cleanup
```

Pending entries are never required for READY state and may be unlinked before open completes. An unowned final orphan may be removed immediately or queued for the same durable unlink protocol, but it cannot be opened as a committed object. Unknown non-managed directory entries are not removed automatically. Classification must complete before READY; §3.3.3–§3.3.4 define when deletion may remain queued without weakening semantic invisibility.

Only then may database state become `READY`.

The `RECOVERING -> READY` publication and normal/background admission
linearization are §3.3.4; recovery completion alone does not let an individual
subsystem admit work early.

## 13.20 Crash-recovery correctness principle

For a transaction whose COMMIT returned success:

```text
after every later successful crash/restart,
its durable logical changes remain committed
```

For committed CREATE DDL this guarantee includes every object-file directory
entry still required by the committed catalog state. Sections 4.7 and 12.2.1
establish object-file and WAL-segment namespace durability before acknowledgement.
Subsequent object retirement and WAL recycling remain governed by their owners;
the guarantee does not require retaining a dropped object or a terminal record
whose outcome is safely represented by the retained status/checkpoint state
under §§12.10.5.3 and 13.10.

For a user writer reconstructed as nonterminal by §13.12 analysis, if no
complete `TXN_COMMIT` for that transaction belongs to the valid retained WAL prefix: `recovery classifies the transaction as a loser/ABORTED`.

If a complete commit record survived in that valid prefix—even though the process had not advanced `durable_lsn` or acknowledged the client—recovery classifies it COMMITTED. This is the crash form of §39.1.5 client outcome uncertainty.

Absence of an already-recycled terminal record does not revoke a committed
outcome preserved by valid status-page recovery state. Nor does the absence of
terminal WAL make an explicitly read-only transaction a persistent crash loser;
§9.15 defines its exemption.

For an explicitly aborted or crash-loser transaction:

```text
physical heap/index garbage may remain
but
no SQL-visible state may depend on that transaction as committed
```

This separation between physical residue and logical visibility is central to the recovery architecture.

## 13.21 Recovery invariants

1. `database.control` has two independently checksummed 4096-byte generation slots.
2. Control generation never wraps; highest independently valid generation wins.
3. `latest_checkpoint_lsn` means the installed CHECKPOINT_BEGIN LSN exactly.
4. A completed checkpoint does not require all dirty data pages to have been flushed.
5. A checkpoint DATA sequence is identified by BEGIN LSN and validates contiguous chunk indexes/count totals.
6. The installed checkpoint has durable END and a durably synchronized control slot.
7. Every DPT `rec_lsn` identifies a complete recovery page image.
8. WAL recycling retains from `min(latest_checkpoint_lsn, checkpoint_redo_lsn)` and cannot discard a needed dirty-page base image.
9. Analysis initializes from checkpoint DATA and scans forward from CHECKPOINT_BEGIN.
10. Redo starts at checkpoint_redo_lsn, which may precede CHECKPOINT_BEGIN.
11. Pre-BEGIN TXN_COMMIT/TXN_ABORT records in the redo range can repair dirty transaction-status pages.
12. Recovery validates/truncates only an invalid WAL tail; malformed required earlier WAL is corruption.
13. Redo is page-LSN idempotent for trusted pages.
14. A corrupt/torn page LSN is not trusted until reconstructed from retained full-image WAL.
15. A complete BTREE_MTR is replayed as one system action; a torn MTR is never partly replayed.
16. Crash losers are resolved by publishing ABORTED, not by ordinary physical user-DML undo.
17. Ordinary user-DML recovery requires no CLRs.
18. Unpublished append-tail pages are removed only after durable initialization publications are reconstructed.
19. Rebuildable approximate metadata is not on the critical recovery path.
20. SQL traffic begins only after recovery completion and a recovery checkpoint.
21. A retained dirty `TXN_STATUS` page is reconstructible from its system full-image `rec_lsn` plus later semantic terminal records; terminal records are not substitutes for the image.
22. A transaction acknowledged committed remains logically committed across every later successful crash/restart.
23. Every physical file required by bootstrap or committed catalog state exists at its exact durably synchronized final name before READY.
24. Missing recovery targets are skipped only after proving that no bootstrap/committed catalog owner requires them.
25. WAL-segment creation and recycling include the required `wal/` directory synchronization; reappearing recycled segments below the retention floor do not extend the logical WAL stream.
26. Recovery's valid WAL range is a contiguous prefix of complete records; candidate reservations and incomplete tails create no replayable holes.
27. Volatile provisional page/MTR mutations disappear on crash and have no recovery meaning unless their complete publication record is in the valid persisted WAL prefix.
28. Runtime ACTIVE/MUST_ABORT and pre-commit-record COMMITTING transactions are crash losers; a complete valid persisted `TXN_COMMIT` is recovered COMMITTED regardless of missing client acknowledgement.
29. Durable COMMIT is never converted to ABORTED by loser handling or a later runtime/cache/transport failure.
30. Recovery redo replays authorized physical index history without rerunning SQL UNIQUE checks; normal execution alone authorizes new logical owners through §11.10.
31. Recovery starts only under §3.3's exclusive database owner, and ordinary/background work begins only at the atomic READY publication point.
