#include "common/crc32c.h"

#include <cstddef>
#include <cstdint>

namespace dblusblus {
namespace {

constexpr std::uint32_t CRC32C_REFLECTED_POLYNOMIAL = 0x82F63B78U;
constexpr std::uint32_t CRC32C_INITIAL_REMAINDER = 0xFFFFFFFFU;

} // namespace

std::uint32_t Crc32c(std::span<const std::byte> bytes) noexcept {
    auto remainder = CRC32C_INITIAL_REMAINDER;

    for (const auto byte : bytes) {
        remainder ^= std::to_integer<std::uint32_t>(byte);

        for (unsigned int bit = 0; bit < 8U; ++bit) {
            if ((remainder & 1U) != 0U) {
                remainder = (remainder >> 1U) ^ CRC32C_REFLECTED_POLYNOMIAL;
            } else {
                remainder >>= 1U;
            }
        }
    }

    return remainder ^ CRC32C_INITIAL_REMAINDER;
}

} // namespace dblusblus
