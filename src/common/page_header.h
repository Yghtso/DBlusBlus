#ifndef DBLUSBLUS_COMMON_PAGE_HEADER_H_
#define DBLUSBLUS_COMMON_PAGE_HEADER_H_

#include "common/encoding.h"
#include "common/types.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace dblusblus {

inline constexpr std::size_t COMMON_PAGE_HEADER_ENCODED_SIZE = 32;

// The persisted page_type field is locked at 16 bits.
// NOLINTNEXTLINE(performance-enum-size)
enum class PageType : std::uint16_t {
    SUPERBLOCK = 0,
    HEAP_DATA = 1,
    FSM_DATA = 2,
    BTREE_INTERNAL = 3,
    BTREE_LEAF = 4,
    BTREE_FREE = 5,
    CATALOG_DATA = 6,
};

struct CommonPageHeader {
    PageType page_type{PageType::SUPERBLOCK};
    std::uint16_t format_version{0};
    std::uint32_t flags{0};
    Lsn page_lsn{INVALID_LSN};
    std::uint32_t checksum_crc32c{0};
    std::uint16_t header_size{COMMON_PAGE_HEADER_ENCODED_SIZE};
    std::uint16_t reserved16{0};
    PageNo page_no{INVALID_PAGE_NO};

    bool operator==(const CommonPageHeader&) const = default;
};

namespace detail {

inline constexpr std::size_t COMMON_PAGE_HEADER_PAGE_TYPE_OFFSET = 0;
inline constexpr std::size_t COMMON_PAGE_HEADER_FORMAT_VERSION_OFFSET = 2;
inline constexpr std::size_t COMMON_PAGE_HEADER_FLAGS_OFFSET = 4;
inline constexpr std::size_t COMMON_PAGE_HEADER_PAGE_LSN_OFFSET = 8;
inline constexpr std::size_t COMMON_PAGE_HEADER_CHECKSUM_OFFSET = 16;
inline constexpr std::size_t COMMON_PAGE_HEADER_HEADER_SIZE_OFFSET = 20;
inline constexpr std::size_t COMMON_PAGE_HEADER_RESERVED16_OFFSET = 22;
inline constexpr std::size_t COMMON_PAGE_HEADER_PAGE_NO_OFFSET = 24;

} // namespace detail

[[nodiscard]] constexpr bool EncodeCommonPageHeader(std::span<std::byte> destination,
                                                    const CommonPageHeader& header) noexcept {
    if (destination.size() < COMMON_PAGE_HEADER_ENCODED_SIZE) {
        return false;
    }

    using PageTypeValue = std::underlying_type_t<PageType>;
    const bool page_type_encoded = EncodeLittleEndian(
        destination.subspan(detail::COMMON_PAGE_HEADER_PAGE_TYPE_OFFSET, sizeof(PageTypeValue)),
        static_cast<PageTypeValue>(header.page_type));
    const bool format_version_encoded =
        EncodeLittleEndian(destination.subspan(detail::COMMON_PAGE_HEADER_FORMAT_VERSION_OFFSET,
                                               sizeof(header.format_version)),
                           header.format_version);
    const bool flags_encoded = EncodeLittleEndian(
        destination.subspan(detail::COMMON_PAGE_HEADER_FLAGS_OFFSET, sizeof(header.flags)),
        header.flags);
    const bool page_lsn_encoded = EncodeLittleEndian(
        destination.subspan(detail::COMMON_PAGE_HEADER_PAGE_LSN_OFFSET, sizeof(header.page_lsn)),
        header.page_lsn);
    const bool checksum_encoded =
        EncodeLittleEndian(destination.subspan(detail::COMMON_PAGE_HEADER_CHECKSUM_OFFSET,
                                               sizeof(header.checksum_crc32c)),
                           header.checksum_crc32c);
    const bool header_size_encoded =
        EncodeLittleEndian(destination.subspan(detail::COMMON_PAGE_HEADER_HEADER_SIZE_OFFSET,
                                               sizeof(header.header_size)),
                           header.header_size);
    const bool reserved16_encoded =
        EncodeLittleEndian(destination.subspan(detail::COMMON_PAGE_HEADER_RESERVED16_OFFSET,
                                               sizeof(header.reserved16)),
                           header.reserved16);
    const bool page_no_encoded = EncodeLittleEndian(
        destination.subspan(detail::COMMON_PAGE_HEADER_PAGE_NO_OFFSET, sizeof(header.page_no)),
        header.page_no);

    return page_type_encoded && format_version_encoded && flags_encoded && page_lsn_encoded &&
           checksum_encoded && header_size_encoded && reserved16_encoded && page_no_encoded;
}

[[nodiscard]] constexpr std::optional<CommonPageHeader>
DecodeCommonPageHeader(std::span<const std::byte> source) noexcept {
    if (source.size() < COMMON_PAGE_HEADER_ENCODED_SIZE) {
        return std::nullopt;
    }

    using PageTypeValue = std::underlying_type_t<PageType>;
    const auto page_type = DecodeLittleEndian<PageTypeValue>(
        source.subspan(detail::COMMON_PAGE_HEADER_PAGE_TYPE_OFFSET, sizeof(PageTypeValue)));
    if (!page_type.has_value()) {
        return std::nullopt;
    }

    const auto format_version = DecodeLittleEndian<std::uint16_t>(
        source.subspan(detail::COMMON_PAGE_HEADER_FORMAT_VERSION_OFFSET, sizeof(std::uint16_t)));
    if (!format_version.has_value()) {
        return std::nullopt;
    }

    const auto flags = DecodeLittleEndian<std::uint32_t>(
        source.subspan(detail::COMMON_PAGE_HEADER_FLAGS_OFFSET, sizeof(std::uint32_t)));
    if (!flags.has_value()) {
        return std::nullopt;
    }

    const auto page_lsn = DecodeLittleEndian<Lsn>(
        source.subspan(detail::COMMON_PAGE_HEADER_PAGE_LSN_OFFSET, sizeof(Lsn)));
    if (!page_lsn.has_value()) {
        return std::nullopt;
    }

    const auto checksum = DecodeLittleEndian<std::uint32_t>(
        source.subspan(detail::COMMON_PAGE_HEADER_CHECKSUM_OFFSET, sizeof(std::uint32_t)));
    if (!checksum.has_value()) {
        return std::nullopt;
    }

    const auto header_size = DecodeLittleEndian<std::uint16_t>(
        source.subspan(detail::COMMON_PAGE_HEADER_HEADER_SIZE_OFFSET, sizeof(std::uint16_t)));
    if (!header_size.has_value()) {
        return std::nullopt;
    }

    const auto reserved16 = DecodeLittleEndian<std::uint16_t>(
        source.subspan(detail::COMMON_PAGE_HEADER_RESERVED16_OFFSET, sizeof(std::uint16_t)));
    if (!reserved16.has_value()) {
        return std::nullopt;
    }

    const auto page_no = DecodeLittleEndian<PageNo>(
        source.subspan(detail::COMMON_PAGE_HEADER_PAGE_NO_OFFSET, sizeof(PageNo)));
    if (!page_no.has_value()) {
        return std::nullopt;
    }

    return CommonPageHeader{
        .page_type = static_cast<PageType>(*page_type),
        .format_version = *format_version,
        .flags = *flags,
        .page_lsn = *page_lsn,
        .checksum_crc32c = *checksum,
        .header_size = *header_size,
        .reserved16 = *reserved16,
        .page_no = *page_no,
    };
}

} // namespace dblusblus

#endif // DBLUSBLUS_COMMON_PAGE_HEADER_H_
