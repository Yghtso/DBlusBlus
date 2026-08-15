# Pre-Pass-7 Architecture / Implementation Consistency Review

## Purpose

This review is a checkpoint between Rewrite Pass 6 and Rewrite Pass 7.

It does not implement code and does not change the legacy architecture authority.

Its purpose is to compare the rewritten architecture completed through legacy §179 with the already implemented Phase 1 storage history recorded in devlogs `0001` through `0020`, while preserving the distinction between:

- architecture contract,
- current implementation behavior,
- implementation-only choices,
- project state,
- known mismatches,
- unresolved architecture gaps.

## Scope

Reviewed implementation history:

```text
0001  identifiers and sentinels
0002  little-endian encoding
0003  PageId/RID codecs
0004  common page header
0005  CRC32C
0006  file superblock
0007  DiskManager POSIX I/O
0008  raw Page
0009  PageFile lifecycle/allocation
0010  heap page structure
0011  raw heap insertion
0012  NORMAL -> DEAD
0013  compaction
0014  tuple header
0015  physical tuple layout
0016  fixed-width tuple codec
0017  varlen tuple codec
0018  persisted FSM page
0019  in-memory FSM candidate index
0020  Phase 1 completion/hardening audit
```

Reviewed rewritten architecture:

```text
Passes 1..6
legacy §§0..179
especially Chapters 4..8
```

## Review rule

Devlogs are implementation evidence, not architecture authority.

The rewrite must not blindly copy every implemented choice into the architecture.

Each implementation fact belongs to one of four classes:

1. **Architecture-conforming contract**
   - already locked/accepted by architecture,
   - must be preserved in `ARCHITECTURE_NEW.md`.

2. **Implementation detail**
   - real current behavior,
   - but not a system/persistent compatibility contract,
   - remains in devlogs / PROJECT_STATE rather than being promoted automatically.

3. **Implementation-selected architecture candidate**
   - implementation chose a persistent or cross-subsystem semantic rule that architecture did not fully specify,
   - must be explicitly accepted by architecture before becoming normative.

4. **Implementation/architecture mismatch**
   - must be tracked explicitly and reconciled before normal development/cutover.

## Findings

### 1. Identifier and base persistence layer

The rewritten architecture preserves the implemented fixed-width identifiers and sentinels.

It also preserves the explicit little-endian persistence model, the 32-byte common page header, persisted page-type codes, and the base superblock contract.

No new mismatch was found here.

The standalone 12-byte `PageId` codec currently implemented is a helper-level persisted representation, but the legacy architecture did not establish it as a globally independent persistent format. It therefore should not automatically be promoted into the architecture solely because the helper exists.

### 2. RID reserved-byte mismatch

This remains the one known Phase 1 implementation mismatch.

Architecture now requires:

```text
RID bytes 14..15 = 0
v1 encoder writes zero
v1 decoder rejects nonzero
```

Current implementation behavior recorded by milestone `0020` is:

```text
encoder writes zero
decoder currently ignores the reserved bytes
```

This remains tracked as rewrite issue `R-001`.

It should be reconciled before normal implementation resumes after the architecture rewrite.

### 3. FileSuperblock

The rewritten Chapter 4 preserves the implementation-aligned v1 contract:

```text
PAGE_SIZE = 8192
magic = DBLUSBLS
format_version = 1
header_size = 72
FileKind explicit codes 1..4
page_type = SUPERBLOCK
page_no = 0
strict zero reserved bytes
CRC32C over bytes 0..8191 with bytes 16..19 treated as zero
```

No migration error was found.

The implementation's current checksum algorithm uses an 8192-byte stack copy. That is an implementation choice, not an architecture requirement.

### 4. DiskManager and PageFile

The rewritten Chapters 4 and 7 preserve the architecture-relevant implemented behavior:

- raw files start at zero pages,
- page zero belongs to higher-level superblock creation,
- ordinary writes target allocated pages only,
- sparse implicit extension is forbidden,
- positional `pread`/`pwrite` is used,
- short I/O is explicit,
- failed reads do not expose partial output,
- offset arithmetic is checked,
- aligned page-file sizes are required,
- extension is serialized,
- `fdatasync` is the v1 page-file durability primitive,
- file descriptors remain private to DiskManager.

Some current `PageFile` behavior is intentionally not promoted because it was not locked as architecture:

- the exact C++ result/error API,
- move-only implementation details,
- exact failed-create partial-path behavior,
- exact requested POSIX creation mode,
- the fact that current `PageFile::Create` reports success only after synchronizing page zero.

The last item is technically important current behavior. If the project wants file-creation success itself to have a normative durability contract, that should become an explicit architecture decision rather than being silently inferred from implementation.

### 5. Raw Page / HeapPage ownership

The rewritten architecture correctly preserves:

```text
Page owns one 8192-byte byte buffer in the current implementation
HeapPage is a lightweight/non-owning page-format controller
HeapPage does not call DiskManager
normal future resident-page access goes through BufferPool
```

The exact `Page` C++ ownership representation is implementation detail.

The critical architectural boundary is preserved.

### 6. Heap page format and mutations

All implementation-selected persistent rules that were later accepted into the legacy architecture are present in the rewrite:

```text
HEAP_DATA format_version = 1
48-byte total header
8-byte slots
UNUSED=0
NORMAL=1
DEAD=2
REDIRECT_RESERVED=3
empty free_slot_head = INVALID_SLOT_ID
new NORMAL aux = 0
maximum raw tuple = 8135
8136 rejected
NORMAL -> DEAD retains coordinates initially
post-compaction DEAD coordinates = (0,0)
DEAD aux preserved
SlotId stable across compaction
DEAD is not reusable merely after compaction
```

No contradiction was found.

Current deterministic compaction movement order and stack-scratch strategy are implementation details, not architecture requirements.

The raw `HeapPage` layer currently accepts a zero-length opaque payload. The architecture does not prohibit this at the raw-byte layer, so this is not a mismatch and does not need to become a SQL tuple-format rule.

### 7. Tuple header and physical tuple representation

The rewritten architecture preserves the Phase 1 persistent tuple decisions:

- exact 48-byte header,
- strict reserved-zero rule,
- previous-version sentinel pair,
- `CommandId 0` validity,
- `HAS_NULLS = 0x0001`,
- `HAS_VARLEN = 0x0002`,
- unknown v1 flag rejection,
- one LSB-first null bit per physical schema column,
- unused high bitmap bits zero,
- NOT NULL schema validation,
- deterministic writer zeroing beneath NULL fixed values,
- BOOLEAN `0x00`/`0x01`,
- two's-complement signed scalar persistence,
- exact IEEE-754 binary64 storage-bit preservation,
- raw DATE/TIMESTAMP physical scalar representation,
- 8-byte VARCHAR descriptors,
- NULL VARCHAR `(0,0)`,
- schema-order packed varlen payload,
- empty-present VARCHAR distinct from NULL,
- exact tuple length / no trailing bytes.

No migration error was found.

Temporary construction allocations, per-column validation strategy, and current `std::variant` construction APIs remain implementation details.

### 8. FSM persisted format

The rewritten Chapter 6 preserves the implemented persistent FSM contract:

```text
category range 0..255
conservative 8-byte slot deduction
8135-byte tuple capacity cap
exact category formula
exact inverse lower bound
48-byte FSM_DATA header
8144 entries
deterministic heap->FSM mapping
entry_count initialized prefix
zero uninitialized suffix
pre-WAL checksum/page-LSN staging
```

No mismatch was found.

### 9. In-memory FSM candidate index

The current implementation additionally has:

```text
256 ordered category buckets
reverse PageNo -> category mapping
best-sufficient-category search
smallest PageNo tie-break
exact runtime tuple-request -> minimum-category helper
```

This behavior is real and must remain visible in `PROJECT_STATE.md` / devlog `0019`.

It is intentionally **not all promoted into the architecture** because:

- the representation is rebuildable runtime metadata,
- the tie-break is not a persisted compatibility rule,
- devlog `0019` itself records no new architecture question,
- the underlying correctness contract is already expressed by the advisory FSM/category semantics.

If a future benchmark/concurrency design makes a particular candidate policy architecturally significant, it can be promoted deliberately.

### 10. Phase 1 / BufferPool boundary

Milestone `0020` classifies Phase 1 as:

```text
COMPLETE AT BUFFERPOOL BOUNDARY
```

The rewritten ownership model is consistent with that finding:

- no current BufferPool/frame/guard/CLOCK implementation is claimed,
- HeapFile remains above the buffering boundary,
- implementing HeapFile directly through DiskManager would violate the intended ownership model.

No Phase 2 implementation should be inferred from the architecture rewrite itself.

### 11. B+ tree chapter versus implemented code

The B+ tree architecture is mostly future/unimplemented relative to Phase 1.

The Phase 1 implementation only directly constrains it through shared contracts such as:

- PageId/RID identity,
- persisted RID encoding,
- BufferPool ownership boundary,
- common page types.

Therefore devlogs should not be used to pretend B+ node/split/concurrency behavior already exists.

The known RID decoder mismatch remains the only direct Phase 1 implementation conflict in this chapter.

## Current recheck result

**No rewrite rollback is required before Pass 7.**

Passes 2–6 are broadly consistent with the Phase 1 implementation history.

The important distinctions to retain are:

```text
known mismatch:
    R-001 RID decoder strictness

open architecture gaps:
    R-010 ordinary-page checksum coverage
    R-014 B+ persistent metadata/page-format completion
    R-015 exact FLOAT64 memcomparable bytes

implemented but intentionally non-architectural examples:
    FSM candidate-index STL representation/tie-break
    deterministic HeapPage compaction movement strategy
    PageFile C++ API/lifetime shape
    raw zero-length HeapPage payload acceptance
```

## Review schedule for the remaining rewrite

### Review A — now, before Pass 7

**Type:** semi-complete implementation-conformance review.

Purpose:

```text
rewritten storage architecture
vs
implemented Phase 1 storage
```

This document is that review.

### Review B — after Pass 9

Passes 7–9 complete the transaction/durability/reclamation cluster:

```text
Pass 7  transactions / snapshots / MVCC / locks
Pass 8  WAL / commit / checkpoint / recovery
Pass 9  vacuum / RID reclamation / transactional write protocols
```

At that point perform a deep cross-system semantic review across:

```text
heap
B+ tree
BufferPool contract
MVCC
locks
WAL
recovery
vacuum
RID reuse
```

This is the most important intermediate correctness review after the current storage review.

### Review C — after Pass 13

Passes 10–13 cover catalog/types/SQL/planning/runtime/execution.

Perform a cross-layer review of:

```text
SQL semantics
catalog/schema interpretation
logical plans
physical plans
execution values/vectors
DML
transaction/storage interaction
```

### Review D — after Pass 15

All legacy numbered architecture content has been migrated.

Perform a whole-document pre-cutover review focused on:

- duplicate normative rules,
- terminology consistency,
- persistent-format registry,
- cross-references,
- deferred-feature consistency,
- unresolved issue register,
- architecture-vs-project-state separation.

### Pass 16 — final complete review / cutover audit

Pass 16 remains the formal complete semantic reconciliation.

It should include:

1. verify all legacy coverage rows,
2. re-read every unresolved issue,
3. resolve or explicitly retain every architecture gap,
4. compare `ARCHITECTURE_NEW.md` against current implementation/project state,
5. recheck relevant devlogs where historical implementation choices matter,
6. verify no project-status/AI/workflow material leaked back into architecture,
7. verify every persistent format is byte-exact where the architecture claims compatibility,
8. verify cross-subsystem invariants end-to-end,
9. only then replace/adopt the legacy architecture.

## Workflow rule from Pass 7 onward

For every remaining rewrite pass, the review input should be:

```text
1. exact legacy sections assigned to the pass
2. current ARCHITECTURE_NEW owner chapters
3. later legacy sections that refine/supersede the current block
4. PROJECT_STATE when current implementation status is relevant
5. relevant devlogs for already implemented subsystems
6. issue register
```

Devlogs should be consulted selectively by subsystem, not mechanically copied.

Whenever implementation and architecture differ, the rewrite must state which of the following is true:

```text
implementation bug / stale implementation
architecture gap
later architecture refinement
intentional implementation freedom
project-state-only fact
```

No difference should be silently reconciled.
