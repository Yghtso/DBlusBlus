• Resolved M-012 in docs/ARCHITECTURE.md:1961.

  1. Canonical rule: V1 is strict by default. Persisted semantics may be interpreted only when explicitly defined by the owning v1 format.
  2. Terminology: Defined KNOWN, RESERVED_ZERO, KNOWN_UNSUPPORTED, UNKNOWN, IGNORABLE_EXTENSION, UNSUPPORTED_FORMAT, and CORRUPTION.
  3. Corruption versus unsupported: Invalid values inside a claimed v1 grammar are corruption. A recognizable positive newer version is unsupported. REDIRECT_RESERVED is the named unsupported-
     state exception.

  4. Reserved-zero: Writers emit zero; readers require zero; rewriters cannot preserve nonzero reserved bytes.
  5. Preserve/round-trip: V1 has no generic unknown-field preservation capability.
  6. Common page flags: No bits are assigned; flags must equal zero.
  7. Superblocks: Generic and specialized BTREE superblocks require version 1, exact known flags, and zero reserved regions. Generic decoding still rejects/dispatches BTREE.
  8. FileKind: Codes 1..5 are complete. Zero or any unassigned code in v1 is corrupt; filename/catalog mismatches are also corrupt.
  9. PageType: Codes 0..7 are complete. Unknown or context-incompatible codes in a v1 page are corrupt.
  10. Heap slots: UNUSED, NORMAL, and DEAD are supported. REDIRECT_RESERVED remains writer-forbidden and returns UNSUPPORTED_RESERVED_STATE; unassigned codes are corrupt.
  11. Tuple flags: Known mask is HAS_NULLS | HAS_VARLEN (0x0003). Unknown bits are corrupt and cannot be cleared during rewrite.
  12. TXN_STATUS: All four two-bit patterns are known. RESERVED retains its established nonterminal semantics.
  13. WAL: Header flags/reserved fields must be zero. Unknown complete record types are UNSUPPORTED_WAL_FORMAT and cannot be skipped. Unknown BTREE_MTR entry encodings are corrupt v1 WAL.
  14. Control slots: Any exact-magic slot advertising a positive newer format blocks fallback to an older v1 slot. Version zero remains an invalid/corrupt slot.
  15. Catalog schema: Only catalog_schema_version=1 may use the v1 descriptors. Zero is corrupt; a recognizable greater version is unsupported.
  16. Catalog enums: Unknown constraint kinds are corrupt required catalog state. Unknown statistics scope kinds invalidate only the affected statistics generation.
  17. PersistedScalarV1: It has no embedded version. Its enclosing grammar selects the codec; unknown TypeIds, flags, lengths, padding, or reserved fields are malformed v1 data.
  18. DefaultValueBlob: Version 1 is exact. Zero/malformed v1 is corrupt; positive newer versions are unsupported required metadata and prevent descriptor reconstruction.
  19. Statistics: Unsupported or malformed payload generations use older-valid/missing-statistics fallback after the ordinary catalog tuple/chunk envelope decodes safely. M-013 numerical tolerance
     remains untouched.

  20. Index key schema: Version 1 is required. Nonpositive catalog values are corrupt; positive newer versions are unsupported. A v1 fingerprint mismatch is corruption.
  21. Identifier sentinels: Sentinels are known field-specific values, not future enum space. Their legality depends on the owning field.
  22. Trailing bytes: Unknown trailing data is corrupt unless explicitly designated ignorable. No required v1 format has such an extension area.
  23. Ignorable-extension whitelist: Only rebuildable sys_statistics scope/version/payload failures qualify.
  24. Required versus rebuildable: Unknown required control, WAL, catalog, page, schema, default, status, or committed-index semantics prevent use or READY. Statistics may fall back.
  25. Open/READY: All deterministically reachable required roots and descriptors must use supported semantics before READY.
  26. Lazy access: Open need not scan every user page. Unsupported/corrupt state discovered later fails access without reinterpretation.
  27. Recovery: Unknown WAL or required page semantics stop recovery; recovery is never best-effort.
  28. Writer canonicality: V1 writers emit only supported versions, known codes/bits, legal sentinels, exact lengths, and zero reserved/padding bytes.
  29. Rewrite/vacuum: Rewriters refuse unsupported source state rather than clearing, normalizing, or downgrading it.
  30. Error taxonomy: Distinguished unsupported database/file/page/WAL/catalog/default/reserved-state results from corrupt WAL/page/heap/index/catalog/database results.
  31. Backward/forward compatibility: V1 is the first format and supports no earlier versions. It cannot open newer databases for reading or writing.
  32. Format upgrades: Migration and online/offline upgrade machinery remain deferred.
  33. Forbidden implementations: The contract now explicitly forbids unknown-flag preservation, nonzero reserved bytes, v2-as-v1 parsing, older-control-slot fallback around newer formats, unknown-
     WAL skipping, redirect guessing, opaque unknown pages, tuple-flag deletion, ignored constraints, unsupported B+ comparators, treating stale FSM data as extensions, failing solely on
     rebuildable stats, filename-based compatibility assumptions, downgrading, ABI-padding extensions, and collapsing unsupported states into corruption.

  34. Sections changed: §§3.3.3, 4.7–4.10, 4.13, new canonical §4.14, renumbered storage invariants §4.15, 5.8, 8.2, 9.11, 12.4, 12.7, 12.10.2, 13.2.3, 13.11, 16.5, 16.9, 17.13, 21.12.1, and
     34.14.5.

  35. Persisted byte layouts: None changed.
  36. Numeric codes: None changed.
  37. Format/schema versions: None changed; catalog_schema_version remains 1.
  38. Remaining M-012 questions: None.
  39. Prerequisite dependencies: None discovered.
  40. New architecture questions: None introduced.
  41. Production code: Untouched. Tests and all other documentation were also untouched. git diff --check passed; only docs/ARCHITECTURE.md is modified.
  42. Implementation Phase 2: NOT entered.