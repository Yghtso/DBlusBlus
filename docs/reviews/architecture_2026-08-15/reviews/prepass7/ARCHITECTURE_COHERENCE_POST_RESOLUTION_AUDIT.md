# Post-Resolution Architecture Coherence Audit

## Snapshot

`ARCHITECTURE_NEW.md` before resolution:

```text
bed8a5ce0db0fe980ccf7f4dbb6d19620d2de0be6bef3f343aed0e2b3dc845c1
```

After resolution:

```text
4981ae3a5bae707e64605c26545757c9fb92e822373f59983e5177e5c45c9648
```

## Result

**PASS for the pre-Pass-7 coherence findings.**

The direct FileSuperblock/BTREE-superblock contradiction is resolved.

The following previously open architecture choices are now exact:

```text
R-010 ordinary-page checksum coverage
R-014 B+ persistent metadata/page-format completion
R-015 FLOAT64 memcomparable bytes
R-017 ordinary-page common flags
R-018 tuple fixed-area derivation
R-020 B+ root metadata/page-latch protocol
```

R-019 remains resolved.

R-021 remains synchronized but intentionally points forward to Pass 9, where the complete physical RID-reuse/read-epoch protocol becomes canonical.

R-001 remains an implementation mismatch, not an architecture ambiguity.

## Mechanical validation completed

- BTREE superblock extension ends exactly at byte 128.
- BTREE trailing reserved suffix is exactly 8064 bytes.
- Base page size remains exactly 8192 bytes.
- Common FileSuperblock prefix remains 72 bytes.
- B+ node header remains 64 bytes.
- B+ slot remains 8 bytes.
- Leaf entry exact length is `user_key_length + 16`.
- Internal entry exact length is `user_key_length + 24`.
- BTREE_FREE complete size is 8192 bytes.
- Tuple physical fixed layout now has a deterministic no-padding derivation.
- Whole-page CRC32C range is byte exact.
- Representative FLOAT64 transformed values are monotonic in the required total order.
- `-0.0` and `+0.0` normalize identically.
- canonical NaN sorts after `+infinity`.
- key-schema FNV-1a descriptor is deterministic.
- unresolved wording for B+ flags/FLOAT64 format was removed.
- optimistic root protocol contains an explicit rule forbidding waiting for a page latch while root metadata is held.

## Concurrency coherence of D5-B

The root protocol avoids the prior cycle because:

```text
root acquisition:
    metadata
    -> pin only
    -> release metadata
    -> wait for page latch
    -> metadata validation while page latch held

structural publication:
    page latch(es)
    -> metadata
```

No path waits for a B+ page latch while holding root metadata.

The temporary metadata->pin step is not metadata->page-latch ordering.

Root replacement uses generation validation to force stale traversals to restart.

Free-list and first/last-leaf metadata use analogous optimistic validation.

## Remaining known non-blocking pre-Pass-7 items

- R-001: current implementation RID decoder still needs future strict-reserved-byte hardening.
- R-021: Pass 9 must become canonical owner of the full RID-reuse/read-epoch gate.
- rewrite-process wording remains in the working document until final cutover.
- later transaction/WAL/vacuum/catalog/execution/optimizer chapters are still pending their scheduled passes.

None of these prevents Rewrite Pass 7.

## Gate

The architecture is now coherent enough to proceed to Rewrite Pass 7 without carrying the previously discovered pre-Pass-7 design holes forward.
