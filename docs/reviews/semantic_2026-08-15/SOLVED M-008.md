• M-008 is resolved in docs/ARCHITECTURE.md:1658.

  1. Validation layers: L0 raw integrity, L1 page-local structure, L2 bounded reference checks, and L3 global graph verification are now normative.
  2. BufferPool boundary: BufferPool performs universal validation and invokes a bounded, nonmutating owner validator before RESIDENT publication. Owner validation cannot recursively fetch or
     query catalogs/MVCC.

  3. Heap invariants: Exact geometry, slot limits, offsets, tuple decoding, free-space boundaries, reserved fields, and checked arithmetic are mandatory L1 checks.
  4. Tuple overlap: All retained NORMAL and DEAD ranges must be mutually disjoint and avoid headers/slot directories.
  5. Slot states: NORMAL, retained DEAD, reclaimed DEAD, and UNUSED now have exact field, payload, and free-list rules. Unknown states are corruption.
  6. REDIRECT_RESERVED: Recognized numerically but unsupported in v1. Writers cannot emit it; ordinary readers reject the page with UNSUPPORTED_RESERVED_STATE.
  7. UNUSED/free list: Every UNUSED slot appears exactly once in the acyclic local list; links are bounded, canonical, and terminate at INVALID_SLOT_ID.
  8. Heap compaction: Unowned holes are valid. Dense packing is canonical compactor output, not a validity requirement. Retained DEAD tuples may move or become canonical reclaimed DEAD slots.
  9. RID dereference: Requires ReadEpoch protection, correct FileId/PageId, published page range, valid SlotId, and caller-permitted slot state.
  10. B+ local invariants: Exact node geometry, flags, levels, key decoding, RIDs, child fields, entry lengths, and reserved bytes are mandatory.
  11. B+ entry overlap: Entry ranges must be in bounds, independently owned, and pairwise disjoint.
  12. B+ ordering: Leaf and internal physical keys must be strictly ordered during every ordinary page load, not only verifier/debug operation.
  13. Duplicate physical entries: Duplicate exact (user_key,RID) storage is corruption. Equal user keys with different RIDs remain structurally legal.
  14. Child PageNos: Must be published ordinary pages in the same B+ file, excluding page 0, invalid, self, root, and duplicate local child references.
  15. Sibling validation: Sentinel use must agree exactly with first/last endpoints. Handoffs validate owner, leaf type, reciprocity, strictly increasing boundary, and bounded progress.
  16. Root/superblock: Index/Table/File identity, schema fingerprint/version, height, root/endpoints, free head, and descriptor agreement are mandatory. Empty-tree and root-contraction states are
     exact.

  17. Free-list PageNos: Head and links must be in range, non-self, and exclude root and leaf endpoints.
  18. Free-list cycles/duplicates: Acyclicity and unique membership are unconditional invariants. Allocation uses serialized, bounded validation; the explicit verifier proves them exhaustively.
  19. Free/live disjointness: A page may never be both free and reachable. Detection may be lazy during ordinary access but cannot be ignored.
  20. Traversal checks: Parent-to-child movement checks ownership, level decrement, declared height, cycles, and exact immediate separator intervals.
  21. Recovery validation: Torn bytes may remain recovery-private only while repaired. Completed page or atomic MTR results must pass ordinary validation before publication.
  22. Zero pages: Zero/uninitialized pages inside the published range are corruption. Only M-003 unpublished append tails may contain them.
  23. FSM: Exact mapping, prefix, suffix-zero, bounds, and reserved fields are structural; category accuracy remains stale/repairable metadata.
  24. TXN_STATUS: Exact page layout and allocation-high-water rules apply. All four assigned codes decode structurally; RESERVED retains its established nonterminal semantics.
  25. Read-validation table: The architecture now specifies L0/L1/L2/L3 requirements for heap, FSM, B+ superblock/internal/leaf/free, catalog, status, ordinary superblocks, and control state.
  26. Writer rule: Final after-images must pass canonical local validation before M-003 WAL-backed publication and ownership release.
  27. Redo rule: Atomic B+ MTR results are validated as a complete set; recovery cannot permanently waive structural invariants.
  28. Error classification: CORRUPT_PAGE, CORRUPT_HEAP, CORRUPT_INDEX, CORRUPT_DATABASE, UNSUPPORTED_RESERVED_STATE, recovery failure, and noncontinuable escalation are distinguished.
  29. Expensive verifier: Owns exhaustive heap/RID checks, complete B+ tree/free graphs, global ordering, sibling coherence, orphans, catalog fixed points, and status consistency.
  30. Progress bounds: Heap chains are bounded by slot_count; B+ descent by height; sibling/free walks by published page count; repeated identities fail as corruption.
  31. Checked arithmetic: Overflow in persisted offset, length, count, ordinal, and page-offset calculations is corruption.
  32. M-012 boundary: Existing required-zero fields remain strict and REDIRECT_RESERVED is resolved narrowly. Broader unknown-version/flag policy remains deferred to M-012.
  33. Forbidden implementations: The architecture explicitly prohibits all listed wrong-but-plausible behaviors, including overlap acceptance, verifier-only sortedness, unchecked pointers,
     unbounded cycles, guessed redirect semantics, invalid recovery publication, and silent malformed traversal.

  34. Sections changed: New canonical §4.13.1–§4.13.10; former storage invariants renumbered §4.14; focused reconciliation in §§5.4.2–5.4.3, 5.21, 6.11, 6.13, 7.6.4, 8.18, 8.20, 8.27–8.29, 9.12,
     13.14, and 41.1–41.2.

  35. Persisted format: No byte layout, numeric code, field width, or persisted version changed.
  36. Remaining M-008 questions: None.
  37. Prerequisite dependency: None discovered.
  38. New architecture questions: None introduced.
  39. Production code: Untouched. Only docs/ARCHITECTURE.md changed; git diff --check passed.
  40. Implementation Phase 2: NOT entered. docs/PROJECT_STATE.md remains unchanged and still records Phase 2 as not started.