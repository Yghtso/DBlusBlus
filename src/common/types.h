#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>

namespace dblusblus {

// An integer identifier that does not convert to or from other identifiers. Construction from
// the raw integer is explicit and Value() reads it back, so passing a FileId where a SchemaVer
// is expected, or swapping a TableId and an IndexId, fails to compile. Same size and layout as
// Rep; no runtime cost.
template <typename Tag, std::unsigned_integral Rep>
class StrongId {
  public:
    using RepType = Rep;

    constexpr StrongId() noexcept = default;
    constexpr explicit StrongId(Rep value) noexcept : value_(value) {}

    [[nodiscard]] constexpr Rep Value() const noexcept {
        return value_;
    }

    auto operator<=>(const StrongId&) const = default;

  private:
    Rep value_{0};
};

template <typename T>
inline constexpr bool IS_STRONG_ID = false;
template <typename Tag, typename Rep>
inline constexpr bool IS_STRONG_ID<StrongId<Tag, Rep>> = true;

template <typename T>
concept StrongIdType = IS_STRONG_ID<T>;

using FileId = StrongId<struct FileIdTag, std::uint32_t>;
using TxnId = StrongId<struct TxnIdTag, std::uint64_t>;
using CommandId = StrongId<struct CommandIdTag, std::uint32_t>;
using Lsn = StrongId<struct LsnTag, std::uint64_t>;
using TableId = StrongId<struct TableIdTag, std::uint64_t>;
using IndexId = StrongId<struct IndexIdTag, std::uint64_t>;
using SchemaVer = StrongId<struct SchemaVerTag, std::uint32_t>;

// Page numbers and slot ids are also positions: they are counted, compared with counts and used
// as array indexes, so they stay plain integers.
using PageNo = std::uint64_t;
using SlotId = std::uint16_t;

inline constexpr FileId INVALID_FILE_ID{0};
inline constexpr PageNo INVALID_PAGE_NO = std::numeric_limits<PageNo>::max();
inline constexpr SlotId INVALID_SLOT_ID = std::numeric_limits<SlotId>::max();
inline constexpr TxnId INVALID_TXN_ID{0};
inline constexpr TxnId FROZEN_TXN_ID{1};
inline constexpr TxnId FIRST_NORMAL_TXN_ID{2};
inline constexpr Lsn INVALID_LSN{0};

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

template <typename Tag, typename Rep>
struct std::hash<dblusblus::StrongId<Tag, Rep>> {
    std::size_t operator()(dblusblus::StrongId<Tag, Rep> id) const noexcept {
        return std::hash<Rep>{}(id.Value());
    }
};

template <>
struct std::hash<dblusblus::PageId> {
    std::size_t operator()(const dblusblus::PageId& page_id) const noexcept {
        const auto file_hash = std::hash<dblusblus::FileId>{}(page_id.file_id);
        const auto page_hash = std::hash<dblusblus::PageNo>{}(page_id.page_no);
        return file_hash ^ (page_hash + 0x9e3779b9U + (file_hash << 6U) + (file_hash >> 2U));
    }
};
