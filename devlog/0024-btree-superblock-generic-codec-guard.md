# 0024 — Generic BTREE Superblock Codec Guard

- Date: 2026-08-15
- Milestone: Phase 1 generic FileSuperblock BTREE conformance guard

## Scope

Made the existing generic 72-byte `FileSuperblock` codec incapable of emitting or accepting an
architecture-invalid BTREE superblock. The task added explicit unsupported-kind handling and
updated focused codec/PageFile tests and current-state documentation.

No canonical BTREE extension codec, B+ tree metadata type, index page, root management, BufferPool,
or other Phase 2 or later functionality was implemented.

## Architecture followed

- `docs/ARCHITECTURE.md` §4.7, random-access file kinds and fixed numeric registry
- `docs/ARCHITECTURE.md` §4.10, FileSuperblock v1
- `docs/ARCHITECTURE.md` §4.10.1, common 72-byte FileSuperblock prefix
- `docs/ARCHITECTURE.md` §4.10.3, whole-page superblock checksum
- `docs/ARCHITECTURE.md` §4.10.4, file-kind-directed validation
- `docs/ARCHITECTURE.md` §8.2 and §8.2.1, canonical 128-byte BTREE superblock and extension

The authoritative BTREE representation remains `header_size = 128`, with its fixed extension at
bytes `72..127` and trailing reserved bytes beginning at byte `128`.

## Previous mismatch

The generic codec treated `FileKind::BTREE` like the generic file kinds. It emitted a 72-byte
header with bytes `72..8191` zero and decoded that same shape successfully. Generic `PageFile`
tests also relied on creating and reopening this noncanonical representation.

There was no specialized BTREE superblock abstraction or extension dispatch in current code.
Implementing the exact Chapter 8 extension would require future B+ metadata such as initialized
root/leaf PageNos, tree height, free-page head, and key-schema identity. That work was outside this
Phase 1 conformance task.

## Chosen resolution

The generic codec now distinguishes:

- a recognized persisted `FileKind`, and
- a file kind supported by the current generic 72-byte codec.

`HEAP`, `FSM`, and `CATALOG` remain supported by the generic codec. `BTREE` remains a recognized
architecture-defined kind but is explicitly unsupported. A future B+ owner must provide the exact
specialized Chapter 8 codec before BTREE files can be created or opened.

This guard changes capability reporting, not the canonical persisted format. No partial 128-byte
representation or placeholder extension was introduced.

## Files changed

- `src/common/file_superblock.h`
  - added `FileSuperblockDecodeError::UNSUPPORTED_FILE_KIND`
  - documented the current generic-codec boundary
- `src/common/file_superblock.cpp`
  - separated recognized kinds from generic-codec-supported kinds
  - rejected BTREE before encoding and after identifying its persisted kind during decode
- `tests/file_superblock_test.cpp`
  - moved exact generic-layout coverage to HEAP
  - limited generic round trips to supported kinds
  - added atomic BTREE encode rejection and 72/128-byte-header decode rejection tests
- `tests/page_file_test.cpp`
  - removed stale successful generic BTREE create/reopen expectations
  - added generic PageFile BTREE creation rejection coverage
- `docs/PROJECT_STATE.md`
  - removed the final current mismatch, documented the unsupported capability boundary, advanced
    the checkpoint, and recorded final verification totals
- `devlog/0024-btree-superblock-generic-codec-guard.md`
  - recorded this milestone

`docs/ARCHITECTURE.md` and all older devlogs were unchanged.

## Exact generic encoder behavior

`EncodeFileSuperblock` continues to return `bool` using its existing local error convention.

- `HEAP`, `FSM`, and `CATALOG` encode exactly as before with `header_size = 72`.
- `BTREE` returns `false` before any destination byte is modified.
- Unknown/invalid file-kind values continue to return `false` before mutation.
- No BTREE superblock bytes are emitted successfully.

Generic `PageFile::Create` naturally reports `SUPERBLOCK_ENCODING_FAILED` when supplied BTREE and
does not write a fake superblock. Its pre-existing failure sequencing leaves the newly allocated
page zero-filled and cleans up the `DiskManager` registration; no PageFile production behavior was
otherwise changed.

## Exact generic decoder behavior

The decoder still validates the whole-page checksum and common identity fields before semantic
success. Once the persisted file kind is decoded:

- unknown numeric values return `INVALID_FILE_KIND`,
- the recognized BTREE code returns `UNSUPPORTED_FILE_KIND`,
- BTREE is rejected whether its common header says `header_size = 72` or the canonical
  `header_size = 128`,
- the generic codec does not interpret bytes `72..127` as a BTREE extension,
- supported generic kinds retain the existing `header_size = 72`, page-size, reserved-zero,
  checksum, and field decoding rules.

This prevents callers from mistaking a generic object for a valid canonical BTREE superblock.

## Numeric and persisted-format stability

`FileKind::BTREE` remains in the persisted registry with unsigned 16-bit code `2`. No `FileKind`
code, offset, width, checksum rule, magic value, format version, or valid generic emitted encoding
changed.

The canonical 128-byte BTREE header and exact bytes `72..127` extension remain unchanged in
`docs/ARCHITECTURE.md`. No persistent-format version bump was made.

The representative exact generic-layout test now uses `FileKind::HEAP` and pins its corresponding
whole-page CRC32C to `0xFE76015C`; the field layout itself is unchanged.

## Tests changed and added

- Verified explicit `FileKind::BTREE == 2` remains unchanged.
- Verified exact HEAP generic-superblock bytes and deterministic zero reserves.
- Verified HEAP, FSM, and CATALOG still round-trip boundary-valued metadata.
- Verified generic BTREE encoding fails without modifying a prefilled destination.
- Verified a manually constructed checksum-valid 72-byte-style BTREE page is rejected as
  `UNSUPPORTED_FILE_KIND`.
- Verified a manually constructed checksum-valid BTREE page with `header_size = 128` is also
  rejected by the generic codec rather than partially interpreted.
- Verified generic `PageFile::Create` cannot write a fake BTREE superblock.
- Existing checksum, reserved-byte, version, page identity, file identity, allocation, and reopen
  tests remain passing for supported generic file kinds.

The first focused run exposed only the expected stale hard-coded checksum from changing the exact
layout fixture from BTREE code `2` to HEAP code `1`; the fixture was updated to the independently
computed HEAP checksum and the focused suite was rerun successfully.

## Verification

- `cmake --preset clang-debug && cmake --build --preset clang-debug`: configured and built
  successfully without project warnings.
- `ctest --test-dir build/clang-debug -R '^(FileSuperblock|PageFile)' --output-on-failure`: 23/23
  passed on the final run.
- `ctest --preset clang-debug --output-on-failure`: 205/205 passed.
- `cmake --preset gcc-debug && cmake --build --preset gcc-debug`: configured and built
  successfully without project warnings.
- `ctest --preset gcc-debug --output-on-failure`: 205/205 passed.
- `cmake --preset clang-asan`: configured successfully.
- `ASAN_OPTIONS=detect_leaks=0 cmake --build --preset clang-asan`: built successfully with
  AddressSanitizer and UndefinedBehaviorSanitizer enabled.
- `ASAN_OPTIONS=detect_leaks=0 ctest --preset clang-asan --output-on-failure`: 205/205 passed with
  no ASan or UBSan findings. LeakSanitizer alone was disabled for the documented ptrace environment
  limitation.
- `cmake --preset clang-tidy && cmake --build --preset clang-tidy`: passed without diagnostics.
- `clang-format --dry-run --Werror` on all touched C++ files: passed.
- `git diff --check`: passed.

No benchmark was run because this task removes an unsupported persisted-format path and does not
change a supported runtime hot path.

## Current state and deferred work

Currently implemented Phase 1 functionality has no known architecture mismatches.

The B+ tree subsystem and canonical specialized 128-byte BTREE superblock codec remain deferred.
This task did not implement B+ runtime functionality or claim BTREE file support. BufferPool,
HeapFile, WAL, MVCC, catalog, and all other later subsystems remain unimplemented. Implementation
Phase 2 was not entered.

## Architecture questions discovered

None. This task conforms current generic-codec capability to an already-settled persisted format.
