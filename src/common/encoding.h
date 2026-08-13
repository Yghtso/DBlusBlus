#ifndef DBLUSBLUS_COMMON_ENCODING_H_
#define DBLUSBLUS_COMMON_ENCODING_H_

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>

namespace dblusblus {
namespace detail {

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

} // namespace dblusblus

#endif // DBLUSBLUS_COMMON_ENCODING_H_
