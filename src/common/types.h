#ifndef DBLUSBLUS_COMMON_TYPES_H_
#define DBLUSBLUS_COMMON_TYPES_H_

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace dblusblus {

using FileId = std::uint32_t;
using PageNo = std::uint64_t;
using SlotId = std::uint16_t;
using TxnId = std::uint64_t;
using CommandId = std::uint32_t;
using Lsn = std::uint64_t;
using TableId = std::uint64_t;
using IndexId = std::uint64_t;
using SchemaVer = std::uint32_t;

inline constexpr FileId INVALID_FILE_ID = 0;
inline constexpr PageNo INVALID_PAGE_NO = std::numeric_limits<PageNo>::max();
inline constexpr SlotId INVALID_SLOT_ID = std::numeric_limits<SlotId>::max();
inline constexpr TxnId INVALID_TXN_ID = 0;
inline constexpr TxnId FROZEN_TXN_ID = 1;
inline constexpr TxnId FIRST_NORMAL_TXN_ID = 2;
inline constexpr Lsn INVALID_LSN = 0;

struct PageId {
    FileId file_id{INVALID_FILE_ID};
    PageNo page_no{INVALID_PAGE_NO};

    auto operator<=>(const PageId&) const = default;
};

struct Rid {
    PageId page{};
    SlotId slot{INVALID_SLOT_ID};

    auto operator<=>(const Rid&) const = default;
};

} // namespace dblusblus

template <>
struct std::hash<dblusblus::PageId> {
    std::size_t operator()(const dblusblus::PageId& page_id) const noexcept {
        const auto file_hash = std::hash<dblusblus::FileId>{}(page_id.file_id);
        const auto page_hash = std::hash<dblusblus::PageNo>{}(page_id.page_no);
        return file_hash ^ (page_hash + 0x9e3779b9U + (file_hash << 6U) + (file_hash >> 2U));
    }
};

#endif // DBLUSBLUS_COMMON_TYPES_H_
