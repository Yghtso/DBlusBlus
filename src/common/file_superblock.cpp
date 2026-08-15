#include "common/file_superblock.h"

#include "common/crc32c.h"
#include "common/encoding.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace dblusblus {
namespace {

constexpr std::size_t MAGIC_OFFSET = COMMON_PAGE_HEADER_ENCODED_SIZE;
constexpr std::size_t FILE_KIND_OFFSET = 40;
constexpr std::size_t RESERVED16_OFFSET = 42;
constexpr std::size_t PAGE_SIZE_OFFSET = 44;
constexpr std::size_t FILE_ID_OFFSET = 48;
constexpr std::size_t RESERVED32_OFFSET = 52;
constexpr std::size_t OBJECT_ID_OFFSET = 56;
constexpr std::size_t CREATION_EPOCH_OFFSET = 64;
constexpr std::size_t TRAILING_RESERVED_OFFSET = FILE_SUPERBLOCK_HEADER_SIZE;

constexpr std::size_t CHECKSUM_OFFSET = detail::COMMON_PAGE_HEADER_CHECKSUM_OFFSET;
constexpr std::size_t CHECKSUM_SIZE = sizeof(std::uint32_t);

static_assert(MAGIC_OFFSET == 32);
static_assert(MAGIC_OFFSET + FILE_SUPERBLOCK_MAGIC.size() == FILE_KIND_OFFSET);
static_assert(FILE_KIND_OFFSET + sizeof(std::uint16_t) == RESERVED16_OFFSET);
static_assert(RESERVED16_OFFSET + sizeof(std::uint16_t) == PAGE_SIZE_OFFSET);
static_assert(PAGE_SIZE_OFFSET + sizeof(std::uint32_t) == FILE_ID_OFFSET);
static_assert(FILE_ID_OFFSET + sizeof(FileId) == RESERVED32_OFFSET);
static_assert(RESERVED32_OFFSET + sizeof(std::uint32_t) == OBJECT_ID_OFFSET);
static_assert(OBJECT_ID_OFFSET + sizeof(std::uint64_t) == CREATION_EPOCH_OFFSET);
static_assert(CREATION_EPOCH_OFFSET + sizeof(std::uint64_t) == FILE_SUPERBLOCK_HEADER_SIZE);

[[nodiscard]] constexpr bool IsRecognizedFileKind(FileKind file_kind) noexcept {
    switch (file_kind) {
    case FileKind::HEAP:
    case FileKind::BTREE:
    case FileKind::FSM:
    case FileKind::CATALOG:
        return true;
    }

    return false;
}

[[nodiscard]] constexpr bool IsSupportedGenericFileKind(FileKind file_kind) noexcept {
    switch (file_kind) {
    case FileKind::HEAP:
    case FileKind::FSM:
    case FileKind::CATALOG:
        return true;
    case FileKind::BTREE:
        return false;
    }

    return false;
}

[[nodiscard]] bool ContainsNonzeroByte(std::span<const std::byte> bytes) noexcept {
    return std::ranges::any_of(bytes, [](std::byte byte) { return byte != std::byte{0}; });
}

[[nodiscard]] FileSuperblockDecodeResult DecodeFailure(FileSuperblockDecodeError error) noexcept {
    return FileSuperblockDecodeResult{.superblock = std::nullopt, .error = error};
}

} // namespace

std::optional<std::uint32_t>
ComputeFileSuperblockChecksum(std::span<const std::byte> page) noexcept {
    if (page.size() < PAGE_SIZE) {
        return std::nullopt;
    }

    std::array<std::byte, PAGE_SIZE> checksum_input{};
    std::copy_n(page.begin(), PAGE_SIZE, checksum_input.begin());
    std::fill_n(checksum_input.begin() + CHECKSUM_OFFSET, CHECKSUM_SIZE, std::byte{0});
    return Crc32c(checksum_input);
}

bool EncodeFileSuperblock(std::span<std::byte> destination,
                          const FileSuperblock& superblock) noexcept {
    if (destination.size() < PAGE_SIZE || !IsSupportedGenericFileKind(superblock.file_kind)) {
        return false;
    }

    std::array<std::byte, PAGE_SIZE> page{};
    const CommonPageHeader common_header{
        .page_type = PageType::SUPERBLOCK,
        .format_version = FILE_SUPERBLOCK_FORMAT_VERSION,
        .flags = superblock.flags,
        .page_lsn = superblock.page_lsn,
        .checksum_crc32c = 0,
        .header_size = FILE_SUPERBLOCK_HEADER_SIZE,
        .reserved16 = 0,
        .page_no = 0,
    };

    using FileKindValue = std::underlying_type_t<FileKind>;
    const bool common_header_encoded = EncodeCommonPageHeader(page, common_header);
    std::ranges::copy(FILE_SUPERBLOCK_MAGIC, page.begin() + MAGIC_OFFSET);
    const bool file_kind_encoded =
        EncodeLittleEndian(std::span{page}.subspan(FILE_KIND_OFFSET, sizeof(FileKindValue)),
                           static_cast<FileKindValue>(superblock.file_kind));
    const bool page_size_encoded =
        EncodeLittleEndian(std::span{page}.subspan(PAGE_SIZE_OFFSET, sizeof(std::uint32_t)),
                           static_cast<std::uint32_t>(PAGE_SIZE));
    const bool file_id_encoded = EncodeLittleEndian(
        std::span{page}.subspan(FILE_ID_OFFSET, sizeof(FileId)), superblock.file_id);
    const bool object_id_encoded =
        EncodeLittleEndian(std::span{page}.subspan(OBJECT_ID_OFFSET, sizeof(superblock.object_id)),
                           superblock.object_id);
    const bool creation_epoch_encoded = EncodeLittleEndian(
        std::span{page}.subspan(CREATION_EPOCH_OFFSET, sizeof(superblock.creation_epoch)),
        superblock.creation_epoch);

    if (!common_header_encoded || !file_kind_encoded || !page_size_encoded || !file_id_encoded ||
        !object_id_encoded || !creation_epoch_encoded) {
        return false;
    }

    const auto checksum = ComputeFileSuperblockChecksum(page);
    if (!checksum.has_value() ||
        !EncodeLittleEndian(std::span{page}.subspan(CHECKSUM_OFFSET, CHECKSUM_SIZE), *checksum)) {
        return false;
    }

    std::ranges::copy(page, destination.begin());
    return true;
}

FileSuperblockDecodeResult DecodeFileSuperblock(std::span<const std::byte> source) noexcept {
    if (source.size() < PAGE_SIZE) {
        return DecodeFailure(FileSuperblockDecodeError::BUFFER_TOO_SMALL);
    }
    const auto page = source.first(PAGE_SIZE);

    const auto stored_checksum =
        DecodeLittleEndian<std::uint32_t>(page.subspan(CHECKSUM_OFFSET, sizeof(std::uint32_t)));
    const auto computed_checksum = ComputeFileSuperblockChecksum(page);
    if (!stored_checksum.has_value() || !computed_checksum.has_value() ||
        *stored_checksum != *computed_checksum) {
        return DecodeFailure(FileSuperblockDecodeError::CHECKSUM_MISMATCH);
    }

    const auto common_header = DecodeCommonPageHeader(page.first(COMMON_PAGE_HEADER_ENCODED_SIZE));
    if (!common_header.has_value()) {
        return DecodeFailure(FileSuperblockDecodeError::BUFFER_TOO_SMALL);
    }
    if (common_header->page_type != PageType::SUPERBLOCK) {
        return DecodeFailure(FileSuperblockDecodeError::WRONG_PAGE_TYPE);
    }
    if (common_header->format_version != FILE_SUPERBLOCK_FORMAT_VERSION) {
        return DecodeFailure(FileSuperblockDecodeError::UNSUPPORTED_FORMAT_VERSION);
    }
    if (common_header->page_no != 0) {
        return DecodeFailure(FileSuperblockDecodeError::WRONG_PAGE_NUMBER);
    }

    if (!std::ranges::equal(FILE_SUPERBLOCK_MAGIC,
                            page.subspan(MAGIC_OFFSET, FILE_SUPERBLOCK_MAGIC.size()))) {
        return DecodeFailure(FileSuperblockDecodeError::MAGIC_MISMATCH);
    }

    using FileKindValue = std::underlying_type_t<FileKind>;
    const auto raw_file_kind =
        DecodeLittleEndian<FileKindValue>(page.subspan(FILE_KIND_OFFSET, sizeof(FileKindValue)));
    if (!raw_file_kind.has_value()) {
        return DecodeFailure(FileSuperblockDecodeError::BUFFER_TOO_SMALL);
    }
    const auto file_kind = static_cast<FileKind>(*raw_file_kind);
    if (!IsRecognizedFileKind(file_kind)) {
        return DecodeFailure(FileSuperblockDecodeError::INVALID_FILE_KIND);
    }
    if (!IsSupportedGenericFileKind(file_kind)) {
        return DecodeFailure(FileSuperblockDecodeError::UNSUPPORTED_FILE_KIND);
    }
    if (common_header->header_size != FILE_SUPERBLOCK_HEADER_SIZE) {
        return DecodeFailure(FileSuperblockDecodeError::WRONG_HEADER_SIZE);
    }

    const auto persisted_page_size =
        DecodeLittleEndian<std::uint32_t>(page.subspan(PAGE_SIZE_OFFSET, sizeof(std::uint32_t)));
    if (!persisted_page_size.has_value()) {
        return DecodeFailure(FileSuperblockDecodeError::BUFFER_TOO_SMALL);
    }
    if (*persisted_page_size != PAGE_SIZE) {
        return DecodeFailure(FileSuperblockDecodeError::WRONG_PAGE_SIZE);
    }

    if (common_header->reserved16 != 0 ||
        ContainsNonzeroByte(page.subspan(RESERVED16_OFFSET, sizeof(std::uint16_t))) ||
        ContainsNonzeroByte(page.subspan(RESERVED32_OFFSET, sizeof(std::uint32_t))) ||
        ContainsNonzeroByte(page.subspan(TRAILING_RESERVED_OFFSET))) {
        return DecodeFailure(FileSuperblockDecodeError::NONZERO_RESERVED_BYTES);
    }

    const auto file_id = DecodeLittleEndian<FileId>(page.subspan(FILE_ID_OFFSET, sizeof(FileId)));
    const auto object_id =
        DecodeLittleEndian<std::uint64_t>(page.subspan(OBJECT_ID_OFFSET, sizeof(std::uint64_t)));
    const auto creation_epoch = DecodeLittleEndian<std::uint64_t>(
        page.subspan(CREATION_EPOCH_OFFSET, sizeof(std::uint64_t)));
    if (!file_id.has_value() || !object_id.has_value() || !creation_epoch.has_value()) {
        return DecodeFailure(FileSuperblockDecodeError::BUFFER_TOO_SMALL);
    }

    return FileSuperblockDecodeResult{
        .superblock =
            FileSuperblock{
                .file_kind = file_kind,
                .file_id = *file_id,
                .flags = common_header->flags,
                .page_lsn = common_header->page_lsn,
                .object_id = *object_id,
                .creation_epoch = *creation_epoch,
            },
        .error = FileSuperblockDecodeError::NONE,
    };
}

} // namespace dblusblus
