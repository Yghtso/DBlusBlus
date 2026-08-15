#ifndef DBLUSBLUS_COMMON_ENCODING_H_
#define DBLUSBLUS_COMMON_ENCODING_H_

#include "common/types.h"

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace dblusblus {

inline constexpr std::size_t PAGE_ID_ENCODED_SIZE = 12;
inline constexpr std::size_t RID_ENCODED_SIZE = 16;

namespace detail {

inline constexpr std::size_t PAGE_ID_FILE_ID_OFFSET = 0;
inline constexpr std::size_t PAGE_ID_PAGE_NO_OFFSET = 4;
inline constexpr std::size_t RID_SLOT_ID_OFFSET = 12;
inline constexpr std::size_t RID_RESERVED_OFFSET = 14;

template <typename Integer>
concept FixedWidthInteger =
    std::same_as<Integer, std::uint8_t> || std::same_as<Integer, std::uint16_t> ||
    std::same_as<Integer, std::uint32_t> || std::same_as<Integer, std::uint64_t> ||
    std::same_as<Integer, std::int8_t> || std::same_as<Integer, std::int16_t> ||
    std::same_as<Integer, std::int32_t> || std::same_as<Integer, std::int64_t>;

} // namespace detail

template <detail::FixedWidthInteger Integer>
[[nodiscard]] constexpr bool EncodeLittleEndian(std::span<std::byte> destination,
                                                Integer value) noexcept {
    if (destination.size() < sizeof(Integer)) {
        return false;
    }

    using Unsigned = std::make_unsigned_t<Integer>;
    auto bits = static_cast<Unsigned>(value);

    for (std::size_t offset = 0; offset < sizeof(Integer); ++offset) {
        destination[offset] = static_cast<std::byte>(bits & static_cast<Unsigned>(0xFFU));
        bits = static_cast<Unsigned>(bits >> 8U);
    }

    return true;
}

template <detail::FixedWidthInteger Integer>
[[nodiscard]] constexpr std::optional<Integer>
DecodeLittleEndian(std::span<const std::byte> source) noexcept {
    if (source.size() < sizeof(Integer)) {
        return std::nullopt;
    }

    using Unsigned = std::make_unsigned_t<Integer>;
    Unsigned bits{0};

    for (std::size_t offset = sizeof(Integer); offset > 0; --offset) {
        bits = static_cast<Unsigned>(bits << 8U);
        bits = static_cast<Unsigned>(
            bits | static_cast<Unsigned>(std::to_integer<unsigned int>(source[offset - 1])));
    }

    return std::bit_cast<Integer>(bits);
}

[[nodiscard]] constexpr bool EncodePageId(std::span<std::byte> destination,
                                          const PageId& page_id) noexcept {
    if (destination.size() < PAGE_ID_ENCODED_SIZE) {
        return false;
    }

    const bool file_id_encoded = EncodeLittleEndian(
        destination.subspan(detail::PAGE_ID_FILE_ID_OFFSET, sizeof(FileId)), page_id.file_id);
    const bool page_no_encoded = EncodeLittleEndian(
        destination.subspan(detail::PAGE_ID_PAGE_NO_OFFSET, sizeof(PageNo)), page_id.page_no);
    return file_id_encoded && page_no_encoded;
}

[[nodiscard]] constexpr std::optional<PageId>
DecodePageId(std::span<const std::byte> source) noexcept {
    if (source.size() < PAGE_ID_ENCODED_SIZE) {
        return std::nullopt;
    }

    const auto file_id =
        DecodeLittleEndian<FileId>(source.subspan(detail::PAGE_ID_FILE_ID_OFFSET, sizeof(FileId)));
    if (!file_id.has_value()) {
        return std::nullopt;
    }

    const auto page_no =
        DecodeLittleEndian<PageNo>(source.subspan(detail::PAGE_ID_PAGE_NO_OFFSET, sizeof(PageNo)));
    if (!page_no.has_value()) {
        return std::nullopt;
    }

    return PageId{.file_id = *file_id, .page_no = *page_no};
}

[[nodiscard]] constexpr bool EncodeRid(std::span<std::byte> destination, const Rid& rid) noexcept {
    if (destination.size() < RID_ENCODED_SIZE) {
        return false;
    }

    const bool page_encoded = EncodePageId(destination.first(PAGE_ID_ENCODED_SIZE), rid.page);
    const bool slot_encoded = EncodeLittleEndian(
        destination.subspan(detail::RID_SLOT_ID_OFFSET, sizeof(SlotId)), rid.slot);
    const bool reserved_encoded = EncodeLittleEndian(
        destination.subspan(detail::RID_RESERVED_OFFSET, sizeof(std::uint16_t)), std::uint16_t{0});
    return page_encoded && slot_encoded && reserved_encoded;
}

[[nodiscard]] constexpr std::optional<Rid> DecodeRid(std::span<const std::byte> source) noexcept {
    if (source.size() < RID_ENCODED_SIZE) {
        return std::nullopt;
    }

    if (source[detail::RID_RESERVED_OFFSET] != std::byte{0} ||
        source[detail::RID_RESERVED_OFFSET + 1] != std::byte{0}) {
        return std::nullopt;
    }

    const auto page = DecodePageId(source.first(PAGE_ID_ENCODED_SIZE));
    if (!page.has_value()) {
        return std::nullopt;
    }

    const auto slot =
        DecodeLittleEndian<SlotId>(source.subspan(detail::RID_SLOT_ID_OFFSET, sizeof(SlotId)));
    if (!slot.has_value()) {
        return std::nullopt;
    }

    return Rid{.page = *page, .slot = *slot};
}

} // namespace dblusblus

#endif // DBLUSBLUS_COMMON_ENCODING_H_
