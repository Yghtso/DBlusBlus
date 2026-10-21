# 0032 — Full Documentation Review and D006 Integration

- Date: 2026-10-21
- Milestone: Land the full documentation review begun 2026-09-26

## Scope

Lands the documentation-only full review of `README.md`, `docs/ARCHITECTURE.md`,
`docs/DEVELOPMENT.md`, `docs/VERIFICATION.md`, and `docs/PROJECT_STATE.md`. No source, test,
or build file changed.

The review ran from baseline `db0e66a` as actions A001–A111 under a local controller
directory (`docs/reviews/2026-09-26_full_review/`, about 112 MB of evidence). That
directory is not committed; it stays as local archival material.

## What the review changed

- Removed history, milestone, and audit chronology from the live documents, and moved the
  material that belonged elsewhere to its owning document (the Appendix C guidance, the
  Verification closure bookkeeping, and duplicate format/state descriptions).
- Normalized heading hierarchies. Section numbers referenced from other documents were
  preserved.
- Repaired local contract contradictions in the storage/bootstrap, isolation, recovery,
  binder/subquery, and optimizer chapters, and matched each repair with a Verification
  procedure.
- Integrated the accepted decisions D001–D005: durable empty terminal reclamation, per-target
  pre-bootstrap namespace coordination, cooperative external namespace behavior, VACUUM /
  CREATE INDEX manifest publication exclusion, and DISTINCT-sensitive EXISTS demand under
  OFFSET.
- PROJECT_STATE now points to Architecture for format sizes instead of repeating them,
  records that database-root lifecycle is not implemented, and lists the remaining
  implementation mismatches.

## D006 — fractional memory needs at target assignment

The user chose Option A, which rounds up to whole bytes:

- Architecture §38.19: each finite nonnegative raw memory estimate is rounded up to whole
  bytes exactly once at the allocation boundary, with checked saturation. Invalid raw values
  keep their existing `OptimizerError`/invariant ownership. Every operand of the
  demand-capped sharing loop is therefore an integer.
- Architecture §38.20: the fit/spill comparison uses the same converted integer need.
- Verification V38-049 now covers sub-byte, boundary, saturating, and invalid raw needs, with
  the counterexample `B=1, raw_need=[0.5]` (target 1) and `B=3, raw_need=[0.5,1.5]`
  (targets 1/2).

## Reconciliation with merged storage work

The review predates devlogs 0030 (BufferPool) and 0031 (superblock flags and HEAP owner
validation). The merge with main resolved PROJECT_STATE as follows:

- kept main's BufferPool and complete HeapPage validation descriptions, and dropped the
  review's stale "buffer management not implemented" section;
- removed the mismatches that 0031 closed (superblock flags, HEAP owner validation);
- narrowed the tuple-header `cmax` mismatch to the standalone codec, because heap owner
  validation enforces it for stored tuples;
- kept the superblock decode-ordering and `SchemaVer` domain mismatches, which were checked
  against the current source;
- reworded two DEVELOPMENT sentences that still described the BufferPool gate as uncrossed.

## Checks

- `git diff --check`: clean.
- All relative Markdown links and heading anchors in `README.md` and `docs/*.md` resolve.
- No build, test, or benchmark ran, because this is a documentation-only change.

## Limitations

The review's own controller had not granted a final broad independent audit (P6/P9/P10)
before closure. The user chose to close it with the bounded check above instead of another
full audit cycle. Gap G31 (a focused post-recycling restart oracle) was queued and is not
added.
