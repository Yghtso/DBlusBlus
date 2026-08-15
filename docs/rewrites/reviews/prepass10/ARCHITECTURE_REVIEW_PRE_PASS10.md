# Pre-Pass-10 Architecture and Data-Structure Review

## Review boundary

Reviewed working architecture:

```text
ARCHITECTURE_NEW.md
SHA-256: c2e94c7ebc70c64ffa3db684e4fdd30e679f7906e574c15ffa1b0963315d96c8
```

Migration state:

```text
legacy §§0..300   migrated / explicitly disposed
legacy §§301..725 pending
```

This task is a **review only**.

It does not perform Pass 10.

It does not modify `ARCHITECTURE_NEW.md`, legacy `ARCHITECTURE.md`, or production code.

The issue register is updated only to record gaps discovered by the review.

## Review questions

The review answers three questions:

1. Is the current rewritten architecture internally coherent?
2. Are its important data structures and persistent layouts mutually coherent?
3. For every gap found, should it be solved now or intentionally wait for a later rewrite pass?

# 1. Executive verdict

## Architectural coherence

**MOSTLY COHERENT, BUT DO NOT START PASS 10 YET.**

The storage/transaction core now has a strong ownership model and its major semantic choices agree:

```text
heap-version MVCC
physical (key,RID) indexes
logical write locks separate from latches
STEAL + NO-FORCE
redo + terminal loser resolution
B+ system MTRs
vacuum-delayed exact index cleanup
two-phase RID reclamation
```

No broad redesign is required.

However, the review found several correctness interactions that should be closed while Chapters 1–15 are still the active focus.

The highest-priority new findings are:

```text
R-030 checksum lifecycle / torn-page detection
R-031 crash-safe new-page publication
R-032 DEAD -> UNUSED / vacuum retry state machine
R-033 unsafe READ COMMITTED retry after partial writes
R-034 terminal status / active registry / lock-release race
R-035 FPI retention versus fuzzy checkpoint WAL recycling
```

Existing open issues:

```text
R-025 WAL binary grammar
R-026 database.control slot codec
R-027 checkpoint framing/recovery start semantics
R-028 exact read-epoch grace protocol
R-029 transaction-status physical history reclamation
```

also belong to the already migrated core.

**Recommendation: resolve these core issues before Pass 10, then run one short post-resolution coherence check.**

## Data-structure coherence

**STRONG.**

Mechanical checks passed for all major already-defined fixed layouts and mappings.

No offset/size arithmetic contradiction was found in the existing byte-exact structures.

The problems found are primarily **state-transition and crash/publication protocols**, not broken field arithmetic.

# 2. Data-structure audit

The following mechanical checks passed:

```text
common page header                    32 bytes
base FileSuperblock prefix            72 bytes
BTREE superblock header              128 bytes
BTREE superblock suffix              8064 bytes
complete BTREE superblock           8192 bytes

HEAP_DATA header                      48 bytes
heap slot                              8 bytes
max raw tuple + new slot + header   8191 bytes
                                      (one-byte strict margin)

FSM_DATA header                       48 bytes
FSM category region                 8144 bytes
complete FSM_DATA page              8192 bytes

persisted RID                         16 bytes

B+ node header                        64 bytes
B+ node slot                           8 bytes
BTREE_FREE page                     8192 bytes

tuple header                           48 bytes

TXN_STATUS header                      32 bytes
TXN_STATUS payload                   8160 bytes
status entries/page                 32640

database.control                    8192 bytes
control slots                       2 x 4096

WAL segment                         64 MiB
```

FSM pinned category examples were recomputed and all match the canonical formulas.

Transaction-status mapping boundaries were recomputed:

```text
TxnId 2       -> page 1, byte 32,   shift 0
TxnId 32641   -> page 1, byte 8191, shift 6
TxnId 32642   -> page 2, byte 32,   shift 0
```

All current internal `§chapter.section` references resolve.

No duplicate current numbered section anchor was found.

# 3. Architecture relationships that are coherent

## 3.1 Physical identity and MVCC

RID consistently denotes one physical tuple version. UPDATE creates a new RID; indexes return through heap visibility; vacuum removes old physical index entries later.

## 3.2 User abort and recovery

The document consistently uses transaction-status visibility rather than physical ordinary-user-DML undo. B+ physical mutations are system MTRs and vacuum eventually removes garbage.

## 3.3 Lock/latch ownership

Page/B+ latches remain short physical protection. TUPLE_WRITE and UNIQUE_KEY locks remain transaction-duration logical conflict protection. Logical waits occur after physical latches are released.

## 3.4 Snapshot/vacuum horizon

Snapshot self-visibility is separated from the global vacuum horizon. The owner is excluded from `snapshot.active`; vacuum uses registered SQL snapshots rather than transaction existence.

## 3.5 WAL-before-data

BufferPool consistently owns `durable_lsn >= page_lsn` before WAL-protected writeback. B+ no-flush barriers are stronger temporary constraints rather than competing policies.

# 4. Gaps that should be solved NOW

These are owned by already migrated Chapters 1–15. Later catalog/SQL/execution/optimizer passes will not provide authoritative storage semantics for them.

## 4.1 R-025 — complete WAL binary grammar

**Solve now.** Numeric record codes, flags/reserved rules, total-length/padding semantics, WAL_PAD, PageId payload codec, patch/count widths, MTR discriminators, checkpoint payload framing, and maximum-record behavior remain incomplete.

## 4.2 R-026 — database.control slot codec

**Solve now.** The semantic alternating-slot algorithm is complete, but the 4096-byte slot cannot yet be independently encoded/validated.

## 4.3 R-027 — checkpoint identity and recovery scan boundary

**Solve now.** In addition to BEGIN/DATA/END association, `latest_checkpoint_lsn` versus `latest_checkpoint_begin_lsn` must be unified, and recovery must define how terminal records before BEGIN are processed when `checkpoint_redo_lsn` reaches earlier WAL.

## 4.4 R-028 — exact ReadEpochManager grace algorithm

**Solve now.** Executor integration can be refined later, but the registration/retirement/advance/safe-reuse algorithm itself belongs to the storage core.

## 4.5 R-029 — physical transaction-status history reclamation

**Solve now.** Semantic retirement is defined, but current absolute TxnId-to-page mapping cannot physically truncate the prefix without a new mechanism.

## 4.6 R-030 — checksum finalization

**Solve now.** Torn-page recovery relies on checksum failure, yet ordinary checksum enablement/finalization is not strong enough or complete.

## 4.7 R-031 — crash-safe page allocation/publication

**Solve now.** PAGE_INIT does not by itself define extension-before-init crash behavior or concurrent allocation holes. The same task should complete heap/FSM growth ordering.

## 4.8 R-032 — DEAD -> UNUSED and vacuum crash re-entry

**Solve now.** Current compaction ordering is contradictory, canonical UNUSED fields are absent, free-slot discovery after safe reuse is unspecified, and partial index cleanup must be idempotent after crash.

## 4.9 R-033 — READ COMMITTED retry after partial writes

**Solve now, correctness-critical.** Buffering external output does not undo persistent writes from an abandoned statement attempt. Without subtransactions/physical undo, v1 needs a stricter retry boundary.

## 4.10 R-034 — terminal publication linearization

**Solve now, correctness-critical.** Terminal status, snapshot-active membership, active-registry lookup, and lock release need one coherent linearization rule.

## 4.11 R-035 — full-page image retention

**Solve now, correctness-critical.** Current fuzzy-checkpoint retention does not prove the full image needed after a later torn write remains in WAL.

# 5. Gaps that SHOULD WAIT for later passes

These are not failures of the current storage core; their owning upper subsystem has not been migrated yet.

## 5.1 Historical schema interpretation — wait for Pass 10

Persisted tuples carry `schema_version`, and vacuum may need to decode old versions to derive historical index keys. Pass 10 must establish a stable `(table_id, schema_version) -> historical physical Schema/Layout` contract.

## 5.2 Catalog file / CATALOG_DATA exact role — wait for Pass 10

FileKind CATALOG and PageType CATALOG_DATA are reserved, but bootstrap/catalog-system-table storage has not yet been migrated. Do not invent its final format before §§301–358 are processed.

## 5.3 TableId / IndexId object allocation — wait for Pass 10/11

Widths and use are already locked. Allocation and atomic catalog/file creation belong to catalog + DDL. `database.control.next_file_id` can be completed in R-026 now; TableId/IndexId creation should wait.

## 5.4 DATE/TIMESTAMP/VARCHAR/FLOAT SQL semantics — wait for Pass 10/11

Storage/index bytes are locked. The future type system must define SQL semantics compatible with persisted ordering/collation.

## 5.5 Transactional DDL and schema locks — wait for Pass 11

Catalog state, file creation, FileId allocation, schema versions, index creation, descriptor invalidation, and transaction/WAL semantics need to be reviewed together when DDL planning is migrated.

## 5.6 RETURNING execution mechanics — wait for Pass 13

The semantic retry rule must be corrected now under R-033. The actual DataChunk/output buffering mechanism belongs to the physical executor later.

# 6. Data-structure watch items for future passes

## Catalog / schema history

Vacuum exact index cleanup requires decoding a tuple according to its historical `schema_version`. Pass 10 must retain enough metadata for every still-persisted tuple version.

## Index key/type compatibility

The future SQL comparison/type layer must match the already-persisted B+ rules, especially NULLS FIRST, binary VARCHAR, FLOAT64 NaN/zero total order, and DATE/TIMESTAMP scalar ordering.

## DDL + old queries

Catalog descriptors are expected to be immutable/versioned while queries hold them. Pass 10/11 must make this agree with tuple schema versions and transactional DDL visibility.

# 7. Documentation/rewrite-process issues

Do not treat R-003/R-004/R-005/R-006/R-007/R-008/R-009 as storage design gaps. They continue to be handled by later-pass classification and final Pass 16 reconciliation.

R-001 is not an architecture gap anymore; it is a frozen-code implementation mismatch to fix after the rewrite.

# 8. Small editorial cleanup found

Appendix A contains both a current sentence saying catalog/spill formats remain to be added and a stale second sentence saying transaction-status/WAL/catalog/spill formats remain to be added, even though transaction-status and WAL are already registered above.

Remove the stale duplicate during the next coherence-resolution edit.

# 9. Gate decision

## Do not start Pass 10 yet

Recommended sequence:

```text
1. Pre-Pass-10 Core Resolution
       resolve the SOLVE NOW items above

2. Short post-resolution architecture/data-structure audit

3. Rewrite Pass 10
       legacy §§301–358
```

Legacy §300 says the next upper layers should consume the transactional storage contract rather than redefine it. That is exactly why the core correctness holes should be resolved before moving upward.

# 10. Review status

```text
ARCHITECTURE_NEW modified: NO
legacy ARCHITECTURE modified: NO
production code modified: NO
Pass 10 performed: NO
issue register updated: YES
```

**PRE-PASS-10 REVIEW: COMPLETE.**
