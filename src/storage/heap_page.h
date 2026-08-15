#ifndef DBLUSBLUS_STORAGE_HEAP_PAGE_H_
#define DBLUSBLUS_STORAGE_HEAP_PAGE_H_

#include "common/page_header.h"
#include "common/types.h"
#include "storage/page.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dblusblus {

inline constexpr std::uint16_t HEAP_PAGE_FORMAT_VERSION = 1;
inline constexpr std::size_t HEAP_PAGE_HEADER_OFFSET = COMMON_PAGE_HEADER_ENCODED_SIZE;
inline constexpr std::size_t HEAP_PAGE_HEADER_ENCODED_SIZE = 16;
inline constexpr std::uint16_t HEAP_PAGE_TOTAL_HEADER_SIZE = 48;
inline constexpr std::size_t HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE = 8;
inline constexpr std::size_t HEAP_PAGE_SLOT_DIRECTORY_OFFSET = HEAP_PAGE_TOTAL_HEADER_SIZE;
inline constexpr std::size_t HEAP_PAGE_MAX_RAW_TUPLE_SIZE =
    PAGE_SIZE - HEAP_PAGE_TOTAL_HEADER_SIZE - HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE - 1;

inline constexpr std::size_t HEAP_PAGE_SLOT_COUNT_OFFSET = 32;
inline constexpr std::size_t HEAP_PAGE_FREE_SLOT_HEAD_OFFSET = 34;
inline constexpr std::size_t HEAP_PAGE_LOWER_OFFSET = 36;
inline constexpr std::size_t HEAP_PAGE_UPPER_OFFSET = 38;
inline constexpr std::size_t HEAP_PAGE_PRUNE_HINT_OFFSET = 40;
inline constexpr std::size_t HEAP_PAGE_RESERVED_OFFSET = 44;

inline constexpr std::size_t HEAP_SLOT_TUPLE_OFFSET_OFFSET = 0;
inline constexpr std::size_t HEAP_SLOT_TUPLE_LENGTH_OFFSET = 2;
inline constexpr std::size_t HEAP_SLOT_FLAGS_OFFSET = 4;
inline constexpr std::size_t HEAP_SLOT_AUX_OFFSET = 6;

// Persisted values are explicit and independent from declaration order.
// NOLINTNEXTLINE(performance-enum-size)
enum class HeapSlotState : std::uint16_t {
    UNUSED = 0,
    NORMAL = 1,
    DEAD = 2,
    REDIRECT_RESERVED = 3,
};

struct HeapPageHeader {
    std::uint16_t slot_count{0};
    SlotId free_slot_head{INVALID_SLOT_ID};
    std::uint16_t lower{HEAP_PAGE_TOTAL_HEADER_SIZE};
    std::uint16_t upper{PAGE_SIZE};
    std::uint32_t prune_hint{0};
    std::uint32_t reserved{0};

    bool operator==(const HeapPageHeader&) const = default;
};

struct HeapSlotEntry {
    std::uint16_t tuple_offset{0};
    std::uint16_t tuple_length{0};
    HeapSlotState state{HeapSlotState::UNUSED};
    std::uint16_t aux{0};

    bool operator==(const HeapSlotEntry&) const = default;
};

enum class HeapSlotEntryDecodeError : std::uint8_t {
    NONE,
    BUFFER_TOO_SMALL,
    INVALID_SLOT_STATE,
};

struct HeapSlotEntryDecodeResult {
    std::optional<HeapSlotEntry> entry;
    HeapSlotEntryDecodeError error{HeapSlotEntryDecodeError::NONE};
};

[[nodiscard]] bool EncodeHeapPageHeader(std::span<std::byte> destination,
                                        const HeapPageHeader& header) noexcept;
[[nodiscard]] std::optional<HeapPageHeader>
DecodeHeapPageHeader(std::span<const std::byte> source) noexcept;

[[nodiscard]] bool EncodeHeapSlotEntry(std::span<std::byte> destination,
                                       const HeapSlotEntry& entry) noexcept;
[[nodiscard]] HeapSlotEntryDecodeResult
DecodeHeapSlotEntry(std::span<const std::byte> source) noexcept;

enum class HeapPageValidationError : std::uint8_t {
    NONE,
    COMMON_HEADER_DECODE_FAILED,
    WRONG_PAGE_TYPE,
    WRONG_PAGE_NUMBER,
    WRONG_HEADER_SIZE,
    UNSUPPORTED_FORMAT_VERSION,
    NONZERO_COMMON_FLAGS,
    NONZERO_COMMON_RESERVED,
    HEAP_HEADER_DECODE_FAILED,
    NONZERO_HEAP_RESERVED,
    LOWER_BEFORE_HEADER,
    UPPER_AFTER_PAGE,
    LOWER_AFTER_UPPER,
    SLOT_DIRECTORY_OUT_OF_BOUNDS,
    SLOT_COUNT_LOWER_MISMATCH,
    INVALID_SLOT_STATE,
    NORMAL_TUPLE_OUT_OF_BOUNDS,
    NONCANONICAL_UNUSED_SLOT,
    INVALID_FREE_SLOT_HEAD,
    INVALID_FREE_SLOT_LINK,
    FREE_SLOT_CYCLE,
    FREE_SLOT_MEMBERSHIP_MISMATCH,
};

struct HeapPageValidationResult {
    std::optional<CommonPageHeader> common_header;
    std::optional<HeapPageHeader> heap_header;
    HeapPageValidationError error{HeapPageValidationError::NONE};
    SlotId slot_id{INVALID_SLOT_ID};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == HeapPageValidationError::NONE;
    }
};

enum class HeapPageInsertError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    TUPLE_TOO_LARGE,
    INSUFFICIENT_SPACE,
    SLOT_ID_EXHAUSTED,
};

struct HeapPageInsertResult {
    std::optional<Rid> rid;
    HeapPageInsertError error{HeapPageInsertError::NONE};
    HeapPageValidationError page_error{HeapPageValidationError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return rid.has_value();
    }
};

enum class HeapPageMarkDeadError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    SLOT_OUT_OF_RANGE,
    INVALID_SLOT_STATE,
    ALREADY_DEAD,
};

struct HeapPageMarkDeadResult {
    HeapPageMarkDeadError error{HeapPageMarkDeadError::NONE};
    HeapPageValidationError page_error{HeapPageValidationError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == HeapPageMarkDeadError::NONE;
    }
};

enum class HeapPageCompactError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    UNSUPPORTED_SLOT_STATE,
    TUPLE_RANGE_OUT_OF_BOUNDS,
    OVERLAPPING_TUPLE_RANGES,
};

struct HeapPageCompactResult {
    HeapPageCompactError error{HeapPageCompactError::NONE};
    HeapPageValidationError page_error{HeapPageValidationError::NONE};
    SlotId slot_id{INVALID_SLOT_ID};
    SlotId other_slot_id{INVALID_SLOT_ID};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == HeapPageCompactError::NONE;
    }
};

class HeapPage {
  public:
    explicit HeapPage(Page& page) noexcept;

    [[nodiscard]] bool Initialize(Lsn page_lsn = INVALID_LSN) noexcept;
    [[nodiscard]] std::optional<HeapPageHeader> Header() const noexcept;
    [[nodiscard]] HeapPageValidationResult Validate() const noexcept;
    [[nodiscard]] HeapPageInsertResult Insert(std::span<const std::byte> tuple) noexcept;
    [[nodiscard]] HeapPageMarkDeadResult MarkDead(SlotId slot_id) noexcept;
    [[nodiscard]] HeapPageCompactResult Compact() noexcept;
    [[nodiscard]] std::optional<std::span<const std::byte>>
    TupleBytes(SlotId slot_id) const noexcept;

  private:
    Page* page_;
};

static_assert(HEAP_PAGE_HEADER_OFFSET + HEAP_PAGE_HEADER_ENCODED_SIZE ==
              HEAP_PAGE_TOTAL_HEADER_SIZE);
static_assert(HEAP_PAGE_RESERVED_OFFSET + sizeof(std::uint32_t) == HEAP_PAGE_TOTAL_HEADER_SIZE);
static_assert(HEAP_SLOT_AUX_OFFSET + sizeof(std::uint16_t) == HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_HEAP_PAGE_H_
