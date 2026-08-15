# Rewrite Pass 8 — WAL, Commit Durability, Checkpointing, and Crash Recovery

## Source and scope

- source: `ARCHITECTURE(4).md`
- SHA-256: `2eb3a6c8134aec7556c8739e8e147c929e4756bddbb234425a943892952c6d86`
- processed legacy sections: `215..255`
- processed source lines: `7283..8382`
- legacy architecture modified: **no**
- production code modified: **no**
- vacuum/reclamation §256+ migrated: **no**

Pass 8 replaces the Pass-1 WAL/recovery baselines with canonical Chapters 12–13.

## Canonical WAL

```text
one logical WAL byte stream
64 MiB segments
8-byte record alignment
48-byte ordinary WAL header
8 MiB initial WAL buffer target
dedicated writer/flusher
monotonic durable_lsn
group commit
synchronous v1 commit
```

PAGE_DELTA is one-page physiological redo.

PAGE_INIT/PAGE_IMAGE provide complete 8192-byte after-images.

The first modification after a completed checkpoint epoch carries a full page image, enabling torn-page reconstruction without a v1 doublewrite buffer.

## B+ mini-transactions

Every physical B+ mutation is a redoable system MTR encoded as one logical `BTREE_MTR`.

All affected pages receive the same MTR LSN.

A temporary no-flush barrier prevents partial in-memory MTR state from reaching data files before the complete MTR record exists.

B+ structural shape and physical garbage may survive user abort.

Heap tuple-version redo precedes index MTR redo that references its RID.

## Buffer/WAL integration

`rec_lsn` is now exact:

```text
first WAL LSN that dirtied the frame in its current dirty interval
```

and was synchronized into Chapter 7.

WAL-before-data remains centralized in BufferPool:

```text
durable_lsn >= page_lsn
```

before WAL-protected writeback.

## Fuzzy checkpoint

`database.control` is 8192 bytes with two alternating 4096-byte slots.

Checkpoint flow is:

```text
BEGIN
capture DPT + active writers + global metadata
DATA...
END
flush END
install/sync control pointer
advance completed FPI epoch
```

A DPT entry is `(PageId,rec_lsn)`.

An active writer entry is `(TxnId,last_wal_lsn)` and is needed only for nonterminal transactions with persistent WAL state.

No full dirty-page flush is part of checkpoint completion.

## Recovery

Startup validates the WAL tail and ignores/truncates the first torn/incomplete tail record onward.

Recovery is canonically:

```text
analysis
redo
loser-status resolution
```

Analysis reconstructs DPT, writer outcomes/losers, max TxnId, checkpoint state, and valid WAL end.

Redo is page-LSN idempotent; complete BTREE_MTR records replay atomically.

A page with an invalid checksum has an untrusted page_lsn and must be reconstructed from PAGE_INIT/PAGE_IMAGE/MTR full-image WAL before later deltas.

Crash losers are published ABORTED.

Ordinary user-DML physical undo and CLRs are not part of v1.

Terminal WAL records can repair stale transaction-status pages.

Recovery installs a recovery checkpoint before the database becomes ONLINE.

## New format/protocol gaps

- `R-025`: complete WAL binary grammar / numeric codes / payload codecs.
- `R-026`: exact 4096-byte database.control slot codec.
- `R-027`: exact CHECKPOINT_BEGIN/DATA/END sequence identity and chunk framing.

These are source gaps; Pass 8 does not guess them.

## Coverage

```text
legacy §§0..255     complete
legacy §§256..725   pending
```

All 41 Pass-8 sections have non-PENDING dispositions. No §256+ section was migrated.

## Validation

- pinned legacy SHA unchanged,
- §215 begins at line 7283,
- §256 begins at line 8383,
- every coverage row through §255 is complete,
- every row from §256 onward remains PENDING,
- Chapters 12 and 13 occur once,
- BufferPool rec_lsn and B+ MTR cross-references were synchronized,
- legacy source and production code were untouched.

## Exit status

**COMPLETE WITH R-025, R-026, AND R-027 EXPLICITLY OPEN.**
