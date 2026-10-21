#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace dblusblus {

[[nodiscard]] std::uint32_t Crc32c(std::span<const std::byte> bytes) noexcept;

} // namespace dblusblus
