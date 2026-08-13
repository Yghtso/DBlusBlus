#ifndef DBLUSBLUS_COMMON_CRC32C_H_
#define DBLUSBLUS_COMMON_CRC32C_H_

#include <cstddef>
#include <cstdint>
#include <span>

namespace dblusblus {

[[nodiscard]] std::uint32_t Crc32c(std::span<const std::byte> bytes) noexcept;

} // namespace dblusblus

#endif // DBLUSBLUS_COMMON_CRC32C_H_
