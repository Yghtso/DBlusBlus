• Resolved M-010 in docs/ARCHITECTURE.md:776.

  1. Checked-advance rule: all allocation, alignment, multiplication, and narrowing checks occur before any identity or mutation publication. Wrapping, truncation, and increment-then-test are forbidden.
  2. Exhaustive domain table: added for persistent IDs, PageNo/page counts, TxnId, CommandId, object IDs, LSNs, WAL segments, statistics chunks, B+ height, encoded lengths, and runtime generations.
  3. TxnId: first 2; last allocatable 18,446,744,073,708,503,041, derived from exact 2^20 reservation blocks. Never reused; exhaustion rejects new transactions.
  4. StatsVersion: TxnId never wraps or reuses, so lexicographic StatsVersion ordering remains monotonic under M-009.
  5. CommandId: 0..UINT32_MAX; the maximum is usable once. Thereafter ordinary statements are rejected, while COMMIT/ROLLBACK remain legal.
  6. TableId: built-ins are 1..6; subsequent shared-allocator IDs begin at least at 7; maximum UINT64_MAX-1; never reused.
  7. ColumnId: table-local 1..UINT32_MAX, subject to smaller schema/tuple limits. V1 CREATE assigns declaration order; no reuse.
  8. IndexId: shared allocator, first possible value 7, maximum UINT64_MAX-1, never reused.
  9. ConstraintId: same shared-allocator boundary as IndexId.
  10. FileId: 1..UINT32_MAX-1; UINT32_MAX may be the exhausted persisted next-value but is never returned.
  11. PageNo: page 0 is valid; UINT64_MAX is invalid. Signed-64-bit physical I/O makes 1,125,899,906,842,622 the last allocatable ordinary page.
  12. File arithmetic: maximum page count is 1,125,899,906,842,623. Page count, offset, and count * 8192 are checked before extension.
  13. SlotId/RID: heap geometry permits slots 0..1017; capacity exhaustion is page-local. RID has no allocator and inherits FileId/PageNo/SlotId reuse rules.
  14. LSN: first record start is 8; exclusive WAL ends are mathematical values through 2^64; the last minimum-record start is 2^64-48. Persisted LSNs always identify record starts.
  15. WAL reservation: validates complete padding, segment transition, aligned end, and terminal credits before returning a candidate. Failure consumes no position or hole.
  16. WAL segment index: 0..2^38-1; filename formatting cannot wrap or truncate.
  17. WAL total_length: includes the 48-byte header, excludes external alignment, and is limited to 67,108,864; maximum payload is 67,108,816.
  18. Oversized BTREE_MTR: fails before publication and follows M-003 rollback. An atomic MTR is not split to evade the bound.
  19. Terminal headroom: each transaction with persistent WAL retains exactly 33,128 bytes of numeric capacity—two independently positioned full-image records plus one terminal record.
  20. COMMIT near exhaustion: a credited transaction can append its terminal sequence. Missing numeric capacity despite credit is a noncontinuable accounting invariant failure.
  21. ABORT near exhaustion: uses the retained credit. Immediate retry reuses its preparatory image; unbounded duplicate images are forbidden.
  22. Numeric exhaustion versus disk-full: exhaustion means no representable successor; resource-full means a representable operation failed operationally. Neither is corruption.
  23. SchemaVer: 1..UINT32_MAX; v1 emits only 1 because schema-changing ALTER is deferred.
  24. BufferPool generations: runtime-only tokens cannot repeat while stale completions exist. Safe quiescent reseeding or pre-mutation failure is required.
  25. Checkpoint/FPI generations: no wrap or collision is permitted. Oversized/exhausted checkpoints remain uninstalled.
  26. Read epochs: UINT64_MAX may be current, but no retirement or increment occurs from it. RID retirement/reuse pauses until safe restart/reinitialization.
  27. Statistics chunks: count 1..1,048,576, index 0..1,048,575, with checked total payload arithmetic through UINT32_MAX.
  28. B+ height: tree height 1..UINT16_MAX; node levels are at most UINT16_MAX-1. Root growth fails before MTR mutation if unrepresentable.
  29. Writer counts/lengths: exact boundaries now cover heap/FSM/B+ counts, tuple/entry lengths, WAL, defaults, scalars, statistics, and checkpoint records.
  30. Error taxonomy: distinguishes ID_EXHAUSTED, TXN_ID_EXHAUSTED, COMMAND_ID_EXHAUSTED, PAGE_NUMBER_EXHAUSTED, WAL_POSITION_EXHAUSTED, and ENCODED_LENGTH_EXCEEDED.
  31. M-005 relationship: pre-write exhaustion may be statement-recoverable; after published effects it causes MUST_ABORT. CommandId exhaustion itself preserves COMMIT eligibility.
  32. M-014 implications: an already-READY database may continue operations not requiring the exhausted domain. Mandatory recovery/checkpoint WAL exhaustion prevents a fresh READY transition.
  33. Crash/restart: durable high-water advances and published identities are never reused. Recovery reconstructs allocator/page/WAL authorities and preserves consumed gaps.
  34. Failed DDL: globally consumed FileId/catalog IDs remain consumed even if only orphan/private artifacts resulted.
  35. Forbidden implementations now explicitly include identity/LSN wrap, premature file extension, length truncation, segment-name wrap, late MTR splitting, terminal-credit consumption, generation-token collision, and B+ height overflow.
  36. Sections changed: canonical §4.3.2 plus concise reconciliation in §4.11, §7.4/§7.6, §8.15, §§9.2–9.6, §§12.2–12.12, §§13.2–13.11, §§15.5–15.6, §16.5, and §39.1.5.
  37. Persisted byte layouts changed: no.
  38. Numeric codes or sentinels changed: no.
  39. Format or schema versions changed: no.
  40. Unresolved M-010 questions: none.
  41. Prerequisite dependencies discovered: none.
  42. New architecture questions introduced: none.
  43. Production code and tests were untouched. git diff --check passes.
  44. Implementation Phase 2 was not entered.