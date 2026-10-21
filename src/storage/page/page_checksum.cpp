#include "storage/page/page_checksum.h"

#include "common/crc32c.h"
#include "common/encoding.h"

#include <algorithm>
#include <array>

namespace dblusblus {
namespace {

constexpr std::size_t CHECKSUM_OFFSET = detail::COMMON_PAGE_HEADER_CHECKSUM_OFFSET;
constexpr std::size_t CHECKSUM_SIZE = sizeof(std::uint32_t);

} // namespace

std::uint32_t ComputePageChecksum(std::span<const std::byte, PAGE_SIZE> page) noexcept {
    std::array<std::byte, PAGE_SIZE> checksum_input{};
    std::copy_n(page.begin(), PAGE_SIZE, checksum_input.begin());
    std::fill_n(checksum_input.begin() + CHECKSUM_OFFSET, CHECKSUM_SIZE, std::byte{0});
    return Crc32c(checksum_input);
}

void FinalizePageChecksum(std::span<std::byte, PAGE_SIZE> page) noexcept {
    const auto checksum = ComputePageChecksum(page);
    // The destination subspan is always exactly four bytes, so encoding cannot fail.
    [[maybe_unused]] const bool encoded =
        EncodeLittleEndian(page.subspan(CHECKSUM_OFFSET, CHECKSUM_SIZE), checksum);
}

bool VerifyPageChecksum(std::span<const std::byte, PAGE_SIZE> page) noexcept {
    const auto stored =
        DecodeLittleEndian<std::uint32_t>(page.subspan(CHECKSUM_OFFSET, CHECKSUM_SIZE));
    return stored.has_value() && *stored == ComputePageChecksum(page);
}

} // namespace dblusblus
