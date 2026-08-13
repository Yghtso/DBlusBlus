# 0006 — File superblock

Date: 2026-08-13

## Milestone/task

Phase 1: persistent random-access file-superblock representation, serialization, checksum, and
validation.

## Scope

Implemented the common base superblock for the locked HEAP, BTREE, FSM, and CATALOG page-file
kinds. Encoding produces one deterministic 8192-byte page. Decoding verifies the CRC32C checksum
and validates the common-header and superblock invariants before returning semantic fields.

The architecture's common-header and superblock requirements were reconciled by using the common
header at offsets 0–31 for `format_version`, `flags`, `page_lsn`, `checksum_crc32c`, `header_size`,
and `page_no`. These fields are not duplicated in the superblock-specific region. The minimum
logical superblock checksum is therefore the common-header checksum at offsets 16–19.

No file I/O, file creation, allocation, file-kind-specific page format, WAL, recovery, or buffer
management was added.

## Files changed

- `src/common/page_header.h` — adds the single named `PAGE_SIZE` constant.
- `src/common/file_superblock.h` — defines persisted constants, `FileKind`, the semantic
  representation, decode errors/result, and the public codec/checksum API.
- `src/common/file_superblock.cpp` — implements deterministic encoding, CRC32C calculation with a
  zeroed checksum field, decoding, and validation.
- `tests/file_superblock_test.cpp` — adds focused layout, round-trip, checksum, and validation
  tests.
- `src/CMakeLists.txt` — registers the source and public header.
- `tests/CMakeLists.txt` — registers the focused test source.
- `devlog/0006-file-superblock.md` — records this task.

## Architecture sections used

- §6 — Persistent Storage Model and 8192-byte page size
- §9 — On-Disk Serialization
- §58 — File Kinds
- §59 — File Superblock
- §61 — Common Page Header
- §62 — Page Types
- §63 — Page Checksums
- §110 — B+ Tree File and Superblock, for future-extension context only

## Public API introduced

The page-size constant is declared in `common/page_header.h`:

```cpp
inline constexpr std::size_t PAGE_SIZE = 8192;
```

The following are declared in `common/file_superblock.h`:

```cpp
inline constexpr std::array<std::byte, 8> FILE_SUPERBLOCK_MAGIC = /* DBLUSBLS */;
inline constexpr std::uint16_t FILE_SUPERBLOCK_FORMAT_VERSION = 1;
inline constexpr std::uint16_t FILE_SUPERBLOCK_HEADER_SIZE = 72;

enum class FileKind : std::uint16_t;

struct FileSuperblock {
    FileKind file_kind;
    FileId file_id;
    std::uint32_t flags;
    Lsn page_lsn;
    std::uint64_t object_id;
    std::uint64_t creation_epoch;
};

enum class FileSuperblockDecodeError : std::uint8_t;

struct FileSuperblockDecodeResult {
    std::optional<FileSuperblock> superblock;
    FileSuperblockDecodeError error;
};

[[nodiscard]] bool
EncodeFileSuperblock(std::span<std::byte> destination,
                     const FileSuperblock& superblock) noexcept;

[[nodiscard]] std::optional<std::uint32_t>
ComputeFileSuperblockChecksum(std::span<const std::byte> page) noexcept;

[[nodiscard]] FileSuperblockDecodeResult
DecodeFileSuperblock(std::span<const std::byte> source) noexcept;
```

## Persisted byte layout

All multibyte integers are little-endian.

| Offset | Size | Field              | Encoding/required value                      |
| -----: | ---: | ------------------ | -------------------------------------------- |
|      0 |    2 | `page_type`        | `PageType::SUPERBLOCK` (`0`)                 |
|      2 |    2 | `format_version`   | `FILE_SUPERBLOCK_FORMAT_VERSION` (`1`)       |
|      4 |    4 | `flags`            | raw superblock flags                         |
|      8 |    8 | `page_lsn`         | superblock page LSN                          |
|     16 |    4 | `checksum_crc32c`  | CRC32C of bytes 0–8191 with bytes 16–19 zero |
|     20 |    2 | `header_size`      | `72`                                         |
|     22 |    2 | common reserved    | zero                                         |
|     24 |    8 | `page_no`          | `0`                                          |
|     32 |    8 | magic              | ASCII bytes `DBLUSBLS`                       |
|     40 |    2 | `file_kind`        | explicit `FileKind` code                     |
|     42 |    2 | alignment reserved | zero                                         |
|     44 |    4 | `page_size`        | `8192` (`00 20 00 00`)                       |
|     48 |    4 | `file_id`          | `FileId`                                     |
|     52 |    4 | alignment reserved | zero                                         |
|     56 |    8 | `object_id`        | table/index/catalog object identifier        |
|     64 |    8 | `creation_epoch`   | opaque 64-bit creation epoch                 |
|     72 | 8120 | reserved           | zero                                         |

The representative layout test pins the canonical page checksum to `0xFCB8C685`, stored as bytes
`85 C6 B8 FC` at offsets 16–19.

## FileKind numeric codes

- `1 = HEAP`
- `2 = BTREE`
- `3 = FSM`
- `4 = CATALOG`

Code zero is intentionally not assigned and is rejected. Existing codes are explicit enum values
and do not depend on declaration order.

## Magic value

The eight persisted magic bytes are ASCII `DBLUSBLS`:

```text
44 42 4C 55 53 42 4C 53
```

## Format-version value

The initial file-superblock format version is `1`. It is stored in the common header's 16-bit
`format_version` field at offset 2; no duplicate superblock-specific version field is written.

## Checksum behavior

- The common-header field at offsets 16–19 is the only superblock checksum field.
- Encoding starts with that field zero, computes CRC32C over exactly the first `PAGE_SIZE` bytes,
  and then stores the result little-endian.
- `ComputeFileSuperblockChecksum` copies the first 8192 bytes to fixed-size stack storage, zeros
  bytes 16–19 in the copy, and computes CRC32C without modifying the caller's page.
- Changing stored checksum bytes does not change the computed logical checksum.
- Decode reports `CHECKSUM_MISMATCH` distinctly and checks CRC before semantic fields.

## Validation behavior

Encoding rejects an undersized destination or an invalid `FileKind` before modifying the
destination. It canonicalizes page type, version, header size, page number, page size, checksum,
and all reserved bytes.

Decoding returns explicit errors for:

- input shorter than 8192 bytes,
- CRC32C mismatch,
- page type other than `SUPERBLOCK`,
- unsupported format version,
- header size other than 72,
- page number other than zero,
- magic mismatch,
- unknown `FileKind`,
- persisted page size other than 8192,
- any nonzero common, alignment, or trailing reserved byte.

Unknown flag bits, `page_lsn`, `file_id`, `object_id`, and `creation_epoch` are preserved without
inventing additional semantic validation.

## Tests and checks run

- Focused Clang superblock tests: 9/9 passed.
- Full `clang-debug` CTest suite: 47/47 passed.
- Full `clang-asan` CTest suite with AddressSanitizer and UndefinedBehaviorSanitizer: 47/47 passed
  with `ASAN_OPTIONS=detect_leaks=0`; LeakSanitizer is disabled because it cannot run under the
  environment's `ptrace`.
- GCC focused superblock tests: 9/9 passed.
- Clang-tidy ran on the implementation and focused tests: clean.
- Clang-format dry-run on changed C++ files: passed.
- `git diff --check`: passed.

Tests pin field offsets and exact representative bytes, the 8192-byte page size, all four numeric
file-kind codes, canonical common-header fields, deterministic reserves, round trips and integer
boundaries, all requested validation failures, checksum corruption, and checksum-field zeroing.
Semantic-validation tests refresh the checksum after field mutation so they reach the intended
validation branch.

## Assumptions

- Common-header `format_version`, `flags`, and `checksum_crc32c` fulfill the corresponding minimum
  logical superblock fields; they are not duplicated after offset 32.
- `header_size` means the complete fixed common-plus-base-superblock header and is therefore 72.
- `creation_epoch` is currently an opaque persisted 64-bit value because its unit/generation
  semantics are not locked.
- Inputs larger than `PAGE_SIZE` are accepted; codecs consume or write only the first 8192 bytes.
- Strict zero-reserved validation is appropriate for this initial format.

## Known limitations and deferred work

- File-kind-specific superblock metadata, including the locked B+ tree superblock fields, is not
  implemented.
- Because this v1 decoder rejects nonzero trailing reserved bytes, later metadata cannot silently
  occupy that region; it requires an explicit format/layout revision or a specialized decoder.
- Checksum computation currently uses an 8192-byte stack copy to zero the checksum field without
  mutating input. Incremental CRC support or another measured optimization is deferred.
- Checksum enable/disable policy, hardware acceleration, file I/O, file creation, page allocation,
  WAL, recovery, and buffer management are deferred.

## Architecture questions discovered

The architecture does not currently lock the following persisted choices made by this task:

- FileKind numeric codes `1` through `4`, with zero invalid;
- magic bytes `DBLUSBLS`;
- initial format version `1`;
- reuse of the common header's format, flags, and checksum fields rather than duplicates;
- superblock-specific offsets 32–71 and total `header_size = 72`;
- strict rejection of nonzero reserved bytes.

These choices should be considered for addition to `ARCHITECTURE.md` if accepted as the
persistent v1 contract.
