#ifndef DBLUSBLUS_COMMON_FILE_SUPERBLOCK_H_
#define DBLUSBLUS_COMMON_FILE_SUPERBLOCK_H_

#include "common/types.h"
#include "storage/page/page_header.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dblusblus {

inline constexpr std::array FILE_SUPERBLOCK_MAGIC{
    std::byte{'D'},
    std::byte{'B'},
    std::byte{'L'},
    std::byte{'U'},
    std::byte{'S'},
    std::byte{'B'},
    std::byte{'L'},
    std::byte{'S'},
};
inline constexpr std::uint16_t FILE_SUPERBLOCK_FORMAT_VERSION = 1;
inline constexpr std::uint16_t FILE_SUPERBLOCK_HEADER_SIZE = 72;

// The persisted file_kind field is 16 bits and uses explicit format codes.
// NOLINTNEXTLINE(performance-enum-size)
enum class FileKind : std::uint16_t {
    HEAP = 1,
    BTREE = 2,
    FSM = 3,
    CATALOG = 4,
};

struct FileSuperblock {
    FileKind file_kind{FileKind::HEAP};
    FileId file_id{INVALID_FILE_ID};
    std::uint32_t flags{0};
    Lsn page_lsn{INVALID_LSN};
    std::uint64_t object_id{0};
    std::uint64_t creation_epoch{0};

    bool operator==(const FileSuperblock&) const = default;
};

enum class FileSuperblockDecodeError : std::uint8_t {
    NONE,
    BUFFER_TOO_SMALL,
    CHECKSUM_MISMATCH,
    WRONG_PAGE_TYPE,
    UNSUPPORTED_FORMAT_VERSION,
    WRONG_HEADER_SIZE,
    WRONG_PAGE_NUMBER,
    MAGIC_MISMATCH,
    INVALID_FILE_KIND,
    UNSUPPORTED_FILE_KIND,
    WRONG_PAGE_SIZE,
    NONZERO_RESERVED_BYTES,
};

struct FileSuperblockDecodeResult {
    std::optional<FileSuperblock> superblock;
    FileSuperblockDecodeError error;
};

// This common-prefix codec supports the generic 72-byte HEAP, FSM, and CATALOG superblocks.
// BTREE requires its architecture-defined 128-byte specialized codec and is rejected here.
[[nodiscard]] bool EncodeFileSuperblock(std::span<std::byte> destination,
                                        const FileSuperblock& superblock) noexcept;

[[nodiscard]] std::optional<std::uint32_t>
ComputeFileSuperblockChecksum(std::span<const std::byte> page) noexcept;

[[nodiscard]] FileSuperblockDecodeResult
DecodeFileSuperblock(std::span<const std::byte> source) noexcept;

} // namespace dblusblus

#endif // DBLUSBLUS_COMMON_FILE_SUPERBLOCK_H_
