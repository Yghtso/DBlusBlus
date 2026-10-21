#pragma once

#include "storage/page/page_header.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace dblusblus::storage {

// Random-access page checksum rule (ARCHITECTURE.md §4.12): CRC32C over all 8192 bytes with the
// checksum field (bytes 16..19) treated as zero, stored little-endian in bytes 16..19.

[[nodiscard]] std::uint32_t
ComputePageChecksum(std::span<const std::byte, PAGE_SIZE> page) noexcept;

// Writes the checksum of the current bytes into bytes 16..19 of a private page image.
void FinalizePageChecksum(std::span<std::byte, PAGE_SIZE> page) noexcept;

// True when bytes 16..19 hold the checksum of the page.
[[nodiscard]] bool VerifyPageChecksum(std::span<const std::byte, PAGE_SIZE> page) noexcept;

} // namespace dblusblus::storage
