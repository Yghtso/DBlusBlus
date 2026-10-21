#ifndef DBLUSBLUS_STORAGE_HEAP_PAGE_H_
#define DBLUSBLUS_STORAGE_HEAP_PAGE_H_

#include "common/types.h"
#include "storage/heap/heap_page_format.h"
#include "storage/page/page.h"
#include "storage/tuple/tuple_codec.h"
#include "storage/tuple/tuple_header.h"
#include "storage/tuple/tuple_layout.h"

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
    UNSUPPORTED_RESERVED_STATE,
    NONZERO_SLOT_AUX,
    EMPTY_NORMAL_TUPLE,
    NONCANONICAL_DEAD_SLOT,
    RETAINED_TUPLE_OUT_OF_BOUNDS,
    OVERLAPPING_TUPLE_RANGES,
    UNRESOLVED_SCHEMA_VERSION,
    INVALID_TUPLE,
    INVALID_TUPLE_TRANSACTION_FIELDS,
    INVALID_PREVIOUS_VERSION_POINTER,
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
    // The earlier-starting retained range for OVERLAPPING_TUPLE_RANGES.
    SlotId other_slot_id{INVALID_SLOT_ID};
    // Tuple-codec detail for INVALID_TUPLE.
    TupleCodecError tuple_error{TupleCodecError::NONE};
    TupleHeaderDecodeError tuple_header_error{TupleHeaderDecodeError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == HeapPageValidationError::NONE;
    }
};

// Immutable historical tuple layouts of one heap relation, resolved by tuple schema_version.
// HeapPage uses it only to hand each retained tuple to the tuple codec for schema-directed
// validation; HeapPage itself never interprets column types. This is a non-owning view: the
// referenced layouts must outlive every HeapPage constructed with it.
class HeapTupleSchemaHistory {
  public:
    explicit HeapTupleSchemaHistory(std::span<const TuplePhysicalLayout> layouts) noexcept;

    // Returns nullptr for SchemaVer{0} and for versions with no registered layout.
    [[nodiscard]] const TuplePhysicalLayout* Resolve(SchemaVer schema_version) const noexcept;

  private:
    std::span<const TuplePhysicalLayout> layouts_;
};

enum class HeapPageInsertError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    TUPLE_TOO_LARGE,
    INVALID_TUPLE,
    INSUFFICIENT_SPACE,
    SLOT_ID_EXHAUSTED,
};

struct HeapPageInsertResult {
    std::optional<Rid> rid;
    HeapPageInsertError error{HeapPageInsertError::NONE};
    HeapPageValidationError page_error{HeapPageValidationError::NONE};
    // Why the new tuple bytes were rejected for INVALID_TUPLE.
    HeapPageValidationError tuple_error{HeapPageValidationError::NONE};

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
};

struct HeapPageCompactResult {
    HeapPageCompactError error{HeapPageCompactError::NONE};
    HeapPageValidationError page_error{HeapPageValidationError::NONE};
    SlotId slot_id{INVALID_SLOT_ID};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == HeapPageCompactError::NONE;
    }
};

// Every operation first applies the complete page-local (L1) owner validation of
// ARCHITECTURE.md §4.13.3, including schema-directed validation of every retained tuple.
class HeapPage {
  public:
    HeapPage(Page& page, HeapTupleSchemaHistory schema_history) noexcept;

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
    HeapTupleSchemaHistory schema_history_;
};

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_HEAP_PAGE_H_
