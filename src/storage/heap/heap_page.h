#ifndef DBLUSBLUS_STORAGE_HEAP_PAGE_H_
#define DBLUSBLUS_STORAGE_HEAP_PAGE_H_

#include "common/types.h"
#include "storage/heap/heap_page_format.h"
#include "storage/page/page.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dblusblus {

enum class HeapPageValidationError : std::uint8_t {
    NONE,
    COMMON_HEADER_DECODE_FAILED,
    WRONG_PAGE_TYPE,
    WRONG_PAGE_NUMBER,
    INVALID_PAGE_NUMBER,
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

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_HEAP_PAGE_H_
