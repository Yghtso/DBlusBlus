# Pass 7 Gap Resolution — Learning-Focused Decisions

## Scope

Resolved only:

```text
R-022 transaction-status persistent format
R-023 TxnId reservation boundary
R-024 snapshot owner/xmin convention
```

Rewrite Pass 8 was **not** performed.

Legacy `ARCHITECTURE.md` and production code were not modified.

`ARCHITECTURE_NEW.md` SHA-256:

```text
before: 6422288b52e13263f6eb7ea31ffee987c6493adcef6f7b77219d1ea9f67ff054
after:  fb3525769221d94ba95924970897789181357a07c81a2596e2b8cfda8751270a
```

## R-022 — transaction-status format

Chosen design emphasizes direct persistent-address arithmetic and bit packing.

### File/page codes

```text
FileKind::TXN_STATUS = 5
PageType::TXN_STATUS = 7
```

The database-wide singleton status file uses:

```text
txn_status.dat
FileSuperblock.object_id = 0
```

### Status codes

Packed LSB-first:

```text
00 INVALID
01 COMMITTED
10 ABORTED
11 RESERVED
```

Zero-filled pages therefore naturally initialize to INVALID.

`RESERVED` is recognized as nonterminal but normal v1 BEGIN does not write it. This preserves the architecture's cheap in-memory active-registry model instead of creating one persistent status update per transaction start.

### Mapping

Reserved TxnIds `0` and `1` consume no status slots.

For normal `txn_id >= 2`:

```text
ordinal = txn_id - 2
page    = 1 + ordinal / 32640
entry   = ordinal % 32640
byte    = 32 + entry / 4
shift   = 2 * (entry % 4)
```

Representative checked mappings:

```text
TxnId 2       -> page 1, byte 32, shift 0
TxnId 3       -> page 1, byte 32, shift 2
TxnId 5       -> page 1, byte 32, shift 6
TxnId 6       -> page 1, byte 33, shift 0
TxnId 32641   -> page 1, byte 8191, shift 6
TxnId 32642   -> page 2, byte 32, shift 0
```

This makes the persistent mapping inspectable by hand and independent of any in-memory hash structure.

## R-023 — exclusive reservation boundary

Chosen:

```text
reserved_txn_id_end =
    first TxnId NOT in the durable reservation
```

The durable range is half-open:

```text
[2, reserved_txn_id_end)
```

A fresh database starts:

```text
next_txn_id         = 2
reserved_txn_id_end = 2
```

The first reservation produces:

```text
new_end = 2 + 1,048,576
        = 1,048,578

reserved range:
[2, 1,048,578)
```

Only after `new_end` is durable may any ID in that interval be handed out.

Half-open ranges were chosen because they remove fencepost ambiguity and compose cleanly with allocator exhaustion checks.

## R-024 — owner-excluded snapshot active set

Chosen:

```text
snapshot.active =
    other nonterminal normal TxnIds
    below snapshot.xmax
```

The owner is excluded.

Self visibility is already handled explicitly through:

```text
owner_txn_id
cmin
cmax
command_id
```

Exact horizon:

```text
active nonempty:
    xmin = smallest active TxnId

active empty:
    xmin = xmax
```

This is especially important for READ COMMITTED: an old transaction between/within statements should not pin the vacuum horizon merely because its own TxnId is old.

The later vacuum architecture explicitly says active SQL snapshots—not merely existing transactions—are the authoritative reclamation horizon.

## Result

All three Pass-7 architecture gaps are now resolved.

The transaction architecture is coherent enough to enter Rewrite Pass 8 on WAL/commit/checkpoint/recovery in the next task.
