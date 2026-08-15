# 0021 — Persistent RID Reserved-Byte Decoder Enforcement

- Date: 2026-08-15
- Milestone: persistent RID reserved-byte decoder enforcement

## Scope

Reconciled the Phase 1 RID decoder with the settled persistent RID contract. This task changed only
RID reserved-byte validation, its focused tests, and current-state documentation. It did not alter
identifier semantics, field offsets or widths, the encoder format, or any later subsystem.

## Architecture followed

- `docs/ARCHITECTURE.md` §4.3, Identifier Types
- `docs/ARCHITECTURE.md` §8.4.1, Persistent RID Encoding in Indexes

The authoritative v1 RID representation remains 16 bytes. Bytes 14 and 15 are reserved, encoders
write both bytes as zero, and decoders reject an encoding when either byte is nonzero.

## Previous mismatch

`EncodeRid` already emitted zero at bytes 14 and 15. `DecodeRid` checked the encoded size and
decoded the PageId and SlotId, but ignored the two reserved bytes and therefore accepted
noncanonical persisted input.

## Files changed

- `src/common/encoding.h`
  - made `DecodeRid` return the existing empty `std::optional` failure result when byte 14 or byte
    15 is nonzero
- `tests/encoding_test.cpp`
  - pinned both encoder-reserved bytes to zero and added focused rejection coverage
- `docs/PROJECT_STATE.md`
  - removed the resolved RID item from the current mismatch inventory, advanced the checkpoint,
    and recorded the current verified test totals
- `devlog/0021-rid-reserved-byte-validation.md`
  - recorded this milestone

No older devlog and no architecture document was modified.

## Decode behavior

`DecodeRid` first requires the complete 16-byte representation. It then checks bytes 14 and 15.
If either byte is nonzero, decoding fails through the codec's existing `std::optional` convention
and no `Rid` is returned. Canonical encodings with both bytes zero continue to decode normally.

There is no permissive alternate decoder and no normalization of malformed reserved bytes.

## Encoder and persisted-format compatibility

`EncodeRid` is unchanged and continues to emit zero at bytes 14 and 15. The RID remains 16 bytes
with the same PageId and SlotId offsets and widths. No persistent-format version was changed or
added. Existing canonical database-produced bytes are unchanged; the decoder now rejects only
inputs forbidden by the established v1 contract.

## Tests added and updated

The exact-byte encoder test now explicitly verifies both reserved offsets are zero. Three focused
decoder tests verify rejection when:

- byte 14 alone is nonzero, using both `0x01` and `0xFF`,
- byte 15 alone is nonzero, using both `0x01` and `0xFF`,
- both bytes are nonzero.

Existing canonical round-trip, sentinel, unaligned-span, PageId, and undersized-buffer tests remain
unchanged and passing.

## Verification

- `cmake --preset clang-debug`: configured successfully.
- `cmake --build --preset clang-debug`: built successfully without project warnings.
- `ctest --preset clang-debug -R '^RidCodecTest\.' --output-on-failure`: 8/8 passed.
- `ctest --preset clang-debug --output-on-failure`: 187/187 passed.
- `cmake --preset clang-asan`: configured successfully.
- Initial sanitizer test discovery encountered the documented ptrace/LeakSanitizer environment
  limitation.
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`: built successfully with ASan and
  UBSan still enabled.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan --output-on-failure`: 187/187 passed with no
  AddressSanitizer or UndefinedBehaviorSanitizer findings.
- `cmake --preset gcc-debug && cmake --build --preset gcc-debug`: configured and built successfully
  without project warnings.
- `ctest --preset gcc-debug --output-on-failure`: 187/187 passed.
- `cmake --preset clang-tidy && cmake --build --preset clang-tidy`: passed without diagnostics.
- `clang-format --dry-run --Werror src/common/encoding.h tests/encoding_test.cpp`: passed.
- `git diff --check`: passed before the documentation update and was rerun on the final diff.

## Current state and deferred work

The resolved RID decoder mismatch was removed from `docs/PROJECT_STATE.md`. The remaining verified
Phase 1 implementation/architecture mismatch set is:

1. BTREE FileSuperblock codec,
2. HEAP_DATA/FSM_DATA common flags,
3. heap UNUSED/free-list structural validation.

Those mismatches were not changed by this task. BufferPool, HeapFile, B+ tree, WAL, MVCC, catalog,
and all other later subsystems remain deferred. Implementation Phase 2 was not entered.

## Architecture questions discovered

None. This task implemented an already-settled persisted-format validation requirement.
