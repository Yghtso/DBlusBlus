# 0020 — Phase 1 Raw-Storage Completion and Hardening Audit

- Date: 2026-08-14
- Milestone: Phase 1 raw-storage completion and hardening audit
- Final classification: **A. COMPLETE AT BUFFERPOOL BOUNDARY**

## Audit scope

Audited the complete BufferPool-independent Phase 1 implementation against the current locked
contracts in `ARCHITECTURE.md`. The audit covered persisted bytes, identifier and sentinel use,
checked arithmetic, page-local failure atomicity, corruption validation, ABI-independent
serialization, allocation and borrowed-view lifetimes, dependency direction, the Phase 2 boundary,
and the primitives available to a future `HeapFile`.

No Phase 2 subsystem or placeholder was introduced. In particular, this task did not add a
`BufferPool`, `BufferFrame`, page guard, replacement policy, dirty-page mechanism, `HeapFile`,
relation-wide `FreeSpaceMap` owner, latch, WAL, recovery, transaction, MVCC, or B+ tree component.

## Files inspected

- `AGENTS.md` and `ARCHITECTURE.md`
- Every Phase 1 devlog from `devlog/0001-*` through `devlog/0019-*`
- All production files under `src/common`:
  - identifier types
  - little-endian and PageId/RID encoding
  - common page header
  - CRC32C
  - file superblock
- All production files under `src/storage`:
  - `DiskManager`
  - `Page`
  - `PageFile`
  - `HeapPage`
  - tuple header, physical layout, and tuple codec
  - persisted FSM page and category helpers
  - in-memory FSM candidate index
- Every test source under `tests`
- Root, source, test, and benchmark CMake files; `CMakePresets.json`; `.clang-format`;
  `.clang-tidy`; and CMake warning/sanitizer/tooling modules

Historical devlog claims were treated as context, not evidence. Current source, current tests, the
locked architecture, and fresh validation runs were used for the conclusions below.

## Files changed

- `src/storage/tuple_codec.cpp`
  - made schema-directed whole-tuple validation reject malformed present BOOLEAN bytes
- `src/storage/tuple_codec.h`
  - added a compile-time IEEE-754 capability assertion for the persisted FLOAT64 contract
- `tests/tuple_codec_test.cpp`
  - changed the BOOLEAN corruption test to require whole-tuple rejection
- `tests/tuple_codec_property_test.cpp`
  - added a deterministic complete tuple-codec round-trip/canonicalization property test
- `tests/CMakeLists.txt`
  - registered the new property test source
- `devlog/0020-phase1-storage-audit.md`
  - recorded this audit

No older devlog and no architecture file was modified.

## Architecture sections audited

- §6, Persistent Storage Model
- §9, On-Disk Serialization
- §11, Heap Page Layout
- §13, Free-Space Management
- §14, Buffer Pool
- §44, Error Handling
- §49, Suggested Implementation Order
- §§54–63, identifier types, PageId/RID, file kinds, superblock, allocation, common header, page
  types, and checksum staging
- §§64–78, heap-file geometry, slots, tuple header/flags/layout/nulls/scalars/VARCHAR/alignment,
  schema versioning, and INSERT
- §§82–84, persisted/runtime FSM semantics and heap compaction
- §§86–100, DiskManager, I/O, BufferPool boundary, storage object ownership, HeapPage/TupleCodec
  boundaries, zero-copy policy, and lifetime safety
- §113, Persistent RID Encoding in Indexes
- §263, DEAD Slot Persistence Across Crash
- §310, Schema Versioning

## Persisted formats audited

### FileSuperblock

The implementation matches the locked 8192-byte page and 72-byte superblock header, including the
32-byte common prefix, magic, explicit `FileKind` codes, page size, `FileId`, object identity,
creation epoch, format version, page number zero, checksum field treatment, and strict zeroed
reserved suffix. Encoding is explicit and does not serialize a C++ struct.

### CommonPageHeader

The implementation matches the exact 32-byte field offsets and little-endian widths. Every current
page controller validates its expected page type and stored page number. Page-specific validators
enforce their locked format version, header size, and reserved-field rules. Pre-WAL page LSN and
checksum staging remains as documented.

### HeapPage

The implementation matches heap format version 1, the 48-byte total header, 16-byte heap-specific
header, 8-byte slot entries, explicit slot-state codes, `INVALID_SLOT_ID` free-list sentinel,
`lower`/`upper` geometry, append-only slot creation, and strict 8135-byte raw tuple maximum. New
NORMAL slots write `aux = 0`; `MarkDead` preserves coordinates and identity; compaction writes
canonical zero coordinates for reclaimed DEAD payloads while preserving `aux` and every SlotId.
No slot reuse is implemented.

### TupleHeader, null bitmap, and tuple body

The implementation matches the exact 48-byte tuple header, known flag mask, fixed header size,
zero reserved field, previous-version sentinel-pair rule, and layout-owned schema version.
`CommandId` zero remains valid.

The bitmap has one LSB-first bit per physical column, including NOT NULL columns. Writers zero
unused high bits and fixed-area bytes for NULL values. Validation enforces NOT NULL declarations,
unused-bit zeroing, and exact `HAS_NULLS` equivalence.

Fixed scalar bytes match the locked BOOLEAN, two's-complement signed integer, raw DATE/TIMESTAMP,
and exact little-endian binary64 contracts. BOOLEAN is exactly `0x00` or `0x01`. FLOAT64 uses a
standards-safe bit representation followed by explicit integer endian encoding, preserving every
payload bit without NaN canonicalization.

VARCHAR uses the exact 8-byte little-endian offset/length descriptor, tuple-relative offsets,
canonical `(0,0)` NULL descriptors, schema-order no-gap payload packing, distinct zero-length
present values, exact tuple length, and the 8135-byte inline limit. Validation rejects gaps,
overlaps, backward or out-of-range descriptors, overflow, noncanonical NULL descriptors, and
trailing bytes.

### FSM_DATA

The implementation matches FSM format version 1 and the 48-byte total header. The specific header
stores the exact little-endian `first_heap_page_no`, `entry_count`, and zero reserved fields,
followed by exactly 8144 one-byte entries. Mapping excludes heap page zero and implements the
locked O(1) heap-page-to-FSM-page/entry formula. `entry_count` is an initialized prefix and the
uninitialized suffix must remain zero. Initialization and validation retain the current pre-WAL
checksum/page-LSN behavior.

No current persisted path writes raw structs, C++ bitfields, pointers, native-endian values, or
alignment-dependent loads/stores.

## Identifier and sentinel findings

The aliases use their locked fixed widths: `FileId`/`CommandId`/`SchemaVer` are 32-bit,
`PageNo`/`TxnId`/`Lsn`/`TableId`/`IndexId` are 64-bit, and `SlotId` is 16-bit. Existing persisted
header assertions pin the widths they consume.

- `CommandId{0}` is accepted and round-tripped.
- `TxnId{0}` remains `INVALID_TXN_ID` and the no-`xmax` representation.
- Page number zero is valid for superblocks and rejected by ordinary heap-page FSM mapping,
  FSM_DATA initialization, and the candidate index.
- `INVALID_PAGE_NO` and `INVALID_SLOT_ID` are rejected or treated as sentinels in their respective
  contexts and are not used as ordinary arithmetic inputs.
- No identifier narrowing capable of silently truncating a valid persisted field was found.

The only unresolved reserved-byte interpretation found is recorded under architecture questions.

## Checked-arithmetic findings

- Disk offsets check `page_no * PAGE_SIZE` against the representable file-offset domain before I/O;
  file size and page-count handling require page alignment; append allocation checks the next page
  number and extent size.
- Heap geometry validates header bounds before subtraction, constrains slot-directory growth to the
  page, checks tuple size and slot cost before mutation, and validates checked tuple ranges.
- Null-bitmap sizing avoids `column_count + 7` overflow. Physical layout and varlen planning use
  checked addition, enforce uint32 VARCHAR lengths, and reject totals above 8135.
- VARCHAR descriptor end calculation checks uint32 overflow before tuple-bound comparisons.
- FSM category and inverse products are bounded integer arithmetic. Existing exhaustive tests cover
  the complete practical free-byte and tuple-request domains. Heap/FSM page mapping and initialized
  ranges reject sentinel inputs and overflow near the `PageNo` limit.

No arithmetic bug was found and no general checked-arithmetic abstraction was added.

## Failure-atomicity findings

Ordinary expected validation failures retain the documented no-partial-mutation behavior:

- fixed-size persisted encoders validate size/metadata first or stage bytes before copying;
- `EncodeTuple` validates values and plans size before its one exact owning allocation, and returns
  no tuple on failure;
- `HeapPage::Insert`, `MarkDead`, and `Compact` validate and stage metadata before page mutation;
- `FsmPage::Initialize` stages both headers before zeroing/writing the page;
- `FsmPage::SetCategory` validates the complete page and entry bound before changing one byte.

Existing byte-for-byte tests cover failed heap insertion, DEAD transition, compaction, FSM
initialization/update, and undersized persisted header/descriptor encodes. No failure-atomicity bug
was found. Crash atomicity remains intentionally deferred to WAL/recovery.

## Corruption-validation findings

One genuine corruption-validation bug was found: `ValidateTuple` accepted a present BOOLEAN byte
such as `0x02`, even though the locked tuple format declares it invalid and decoding that BOOLEAN
column rejected it. This allowed a caller validating a tuple without decoding that particular
column to accept malformed persisted data.

The fix makes schema-directed whole-tuple validation decode each present BOOLEAN through the
existing scalar codec and return `TupleCodecError::INVALID_BOOLEAN` with the offending column
index. NULL BOOLEAN values remain semantically unread, consistent with the architecture's rule
that readers need not reject nonzero fixed bytes beneath a NULL bit.

The remaining audited corruption paths already reject the locked invalid states: heap slot states
and geometry, tuple header/flags/bitmap/nullability/schema version/varlen packing/trailing bytes,
and FSM common/specific metadata, represented range, reserved fields, and nonzero uninitialized
suffix.

## ABI and serialization findings

Persisted paths use explicit byte spans and endian codecs. The only `memmove` operations copy
opaque heap tuple payload bytes; they do not serialize C++ objects. No persistence `reinterpret_cast`,
raw-struct `memcpy`, C++ bitfield layout, pointer serialization, or alignment-dependent field access
was found.

FLOAT64 already required an 8-byte `double`; the audit added
`static_assert(std::numeric_limits<double>::is_iec559)` so a target with a non-IEEE 64-bit `double`
cannot silently violate the binary64 persistence contract. FLOAT64 bits are still moved with
`std::bit_cast` and then encoded explicitly little-endian.

## Allocation and lifetime findings

Endian/header/scalar codecs, page-local heap operations, FSM page entry access, category math, and
tuple validation/decode remain allocation-free. Accepted current allocations are limited to
schema-lifetime `TuplePhysicalLayout` metadata, one exact output vector plus temporary varlen-size
planning for tuple construction, and the rebuildable STL structures in `FsmCandidateIndex`.

`Page` owns one 8192-byte buffer. `HeapPage` and `FsmPage` are non-owning controllers over it and do
not duplicate that buffer. `PageFile` remains move-only around file lifecycle state.

Decoded VARCHAR values remain non-owning spans into caller-owned tuple bytes; the public comment
states that they must not outlive the tuple backing storage. Enforcing a guarded buffer-frame
lifetime requires the later BufferPool/PageGuard layer and was not approximated here.

## Dependency and layering findings

Dependency searches and include inspection found no upward dependency violation:

- common serialization does not depend on storage objects;
- `Page` does not know tuple schemas;
- `HeapPage` treats tuple bytes as opaque;
- tuple layout/codec does not depend on I/O, `HeapFile`, or BufferPool;
- `FsmPage` and `FsmCandidateIndex` perform no storage I/O;
- `DiskManager` does not parse file-kind or page-format semantics;
- `PageFile` owns random-access file lifecycle/allocation but no buffering policy.

No interface was added solely for dependency appearance.

## Phase 2 boundary findings

No current source implements or partially implements `BufferPool`, `BufferFrame`, page guards,
CLOCK, dirty-frame management, or page latches under those or substitute names. No Phase 2 type was
introduced by this task.

The locked storage-object boundary requires relation-wide page access to flow through the future
BufferPool. Implementing `HeapFile` directly over `DiskManager`/`PageFile` would bypass that
contract, so `HeapFile` remains intentionally deferred rather than being forced into Phase 1 with
the wrong ownership and lifetime model.

## HeapFile-readiness findings

A future correctly layered `HeapFile` can rely on:

- canonical encoded tuple bytes and borrowed decode views from `TupleCodec`;
- opaque raw insertion, DEAD transition, compaction, validation, and stable SlotIds from `HeapPage`;
- append-first file-page allocation below the buffering boundary;
- deterministic heap-page/FSM location and category conversion;
- persisted category storage in `FsmPage`;
- advisory candidate selection and caller-driven stale-category repair in
  `FsmCandidateIndex`.

No missing BufferPool-independent primitive required before `HeapFile` was found. The remaining
dependency is buffered relation-wide page access and lifetime management, which belongs to Phase 2.

## Tests added or changed

- Strengthened the existing malformed BOOLEAN tuple test to require `ValidateTuple` to return
  `INVALID_BOOLEAN` and the offending column index.
- Added a fixed-seed, 2,000-iteration complete tuple property test. It generates zero-to-twelve
  column schemas mixing BOOLEAN, INT32, INT64, FLOAT64, DATE, TIMESTAMP, VARCHAR, nullable values,
  NULLs, empty strings, and opaque VARCHAR bytes. Each iteration builds a layout, encodes, validates,
  decodes every column, compares exact physical values, re-encodes the decoded views, and requires
  byte-for-byte equality with the first canonical tuple. FLOAT64 comparisons use raw payload bits.

Existing deterministic boundary, corruption, persistence/reopen, failure-atomicity, exhaustive FSM
math, and FSM candidate-index reference-model tests were retained. No HeapPage randomized test was
added because its current operation-specific suites already exercise insertion, DEAD transition,
compaction ordering, overlap rejection, zero/max sizes, byte-preserving expected failures, RID
stability, and persistence; this audit did not find a gap that justified a second large test model.

## Production bugs discovered and exact fixes

### Bug: whole-tuple validation accepted malformed present BOOLEAN data

- Finding: `ValidateTuple` checked tuple structure but not present BOOLEAN payload bytes.
- Risk: corrupted tuple bytes could pass an explicit validation call unless that BOOLEAN column was
  subsequently decoded.
- Fix: present BOOLEAN columns are checked through `DecodeFixedScalar`; malformed bytes return
  `TupleCodecError::INVALID_BOOLEAN` and the column index.
- Tests: the focused corruption test now requires whole-tuple rejection; the complete property test
  and all existing codec suites cover legal BOOLEAN round trips.

### Hardening: binary64 target assumption

- Finding: the codec asserted only that `double` and `uint64_t` have equal size.
- Risk: an unusual non-IEC-559 64-bit `double` target could compile while violating the locked
  binary64 contract.
- Fix: added a compile-time `is_iec559` assertion.
- Verification: both Clang and GCC builds satisfy the assertion.

No other production bug was found.

## Complete tests and checks run

- Initial focused Clang tuple codec regression suite before edits: 29/29 passed, confirming the
  prior malformed-BOOLEAN acceptance behavior was the baseline.
- Focused Clang tuple/corruption/property suite after the production fix: 30/30 passed; the final
  test-only tidy correction was followed by a 2/2 rerun of the changed BOOLEAN/property tests.
- Clean Clang Debug configure/build and full CTest: 184/184 passed.
- Clean Clang ASan + UBSan configure/build and full CTest: 184/184 passed with
  `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer alone was disabled for the documented ptrace
  environment limitation; AddressSanitizer and UndefinedBehaviorSanitizer remained enabled and
  reported no errors.
- Clean GCC Debug configure/build and full CTest: 184/184 passed.
- Final post-tidy-change full CTest reruns in Clang Debug, Clang ASan/UBSan, and GCC Debug: 184/184
  passed in each configuration.
- Clang-tidy preset build: passed without diagnostics after correcting test-only optional-access and
  initialization warnings.
- `clang-format --dry-run --Werror` on every changed C++ file: passed.
- `git diff --check`: passed.
- Compiler builds produced no project warnings.

No benchmark was run. The audit made no benchmark-dependent optimization, and the repository has
only its generic benchmark smoke target rather than a Phase 1 storage benchmark.

## Assumptions

- `ARCHITECTURE.md` is authoritative; devlogs are historical evidence only.
- Pre-WAL `page_lsn` and checksum staging remains intentionally incomplete where the architecture
  permits it.
- `HeapPage` remains an opaque byte container and does not validate tuple schema or MVCC semantics.
- FSM hints may be stale; real page space must still be checked by a later insertion owner.
- DATE/TIMESTAMP SQL epoch, unit, precision, calendar, and time-zone semantics remain outside the
  raw storage layer.

## Known limitations and deferred work

- No relation-wide `HeapFile` or `FreeSpaceMap` owner exists.
- No BufferPool, frame table, page guard, replacement, pinning, dirty-page, latch, or background
  flushing behavior exists.
- Decoded tuple views currently rely on caller-owned byte-span lifetime; guarded frame lifetime is
  deferred with BufferPool.
- Checksums remain in their pre-recovery staging mode.
- No automatic FSM I/O, stale-page retry/repair, or heap-allocation integration exists.
- WAL, recovery, transactions, MVCC visibility, vacuum eligibility/reuse, indexes, and SQL/catalog
  semantics remain deferred.
- The RID reserved-byte decode policy below remains architecturally unsettled but does not block
  current raw heap/FSM primitives.

## Architecture questions discovered

### Strict v1 interpretation of persisted RID reserved bytes

Current implementation behavior:

- `EncodeRid` writes bytes 14–15 of the locked 16-byte RID representation as zero.
- `DecodeRid` ignores those two bytes and accepts nonzero values.

Architecture §113 names the two-byte field `reserved` but does not explicitly require zero on write
or rejection of nonzero values on read. Historical devlog 0003 documented the current permissive
decoder as an assumption.

Proposed behavior for a future architecture synchronization, if canonical strictness is desired:

```text
RID reserved bytes = 0 on write
RID reserved bytes must be 0 on v1 decode
```

Compatibility implication: the encoder already emits canonical zero bytes, so database-produced
RIDs are unaffected. Tightening decode would reject external or previously accepted 16-byte inputs
with nonzero reserved bytes. This task did not silently choose either policy and did not change the
decoder.

No other architecture question was discovered.

## Final Phase 1 classification

**A. COMPLETE AT BUFFERPOOL BOUNDARY**

Reason: every currently required BufferPool-independent raw-storage primitive was found present and,
after the BOOLEAN validation fix, no unresolved correctness issue prevents advancing to the next
architectural dependency. The future `HeapFile` needs relation-wide page fetch, pin/lifetime, and
mutation ownership through the locked BufferPool boundary. Implementing it directly through
`DiskManager` would violate that boundary. Therefore the next architecturally valid implementation
requires Phase 2 buffer management; this milestone stops before it.
