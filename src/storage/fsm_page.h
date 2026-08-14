#ifndef DBLUSBLUS_STORAGE_FSM_PAGE_H_
#define DBLUSBLUS_STORAGE_FSM_PAGE_H_

#include "common/page_header.h"
#include "common/types.h"
#include "storage/heap_page.h"
#include "storage/page.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace dblusblus {

inline constexpr std::uint16_t FSM_PAGE_FORMAT_VERSION = 1;
inline constexpr std::size_t FSM_PAGE_HEADER_OFFSET = COMMON_PAGE_HEADER_ENCODED_SIZE;
inline constexpr std::size_t FSM_PAGE_HEADER_ENCODED_SIZE = 16;
inline constexpr std::uint16_t FSM_PAGE_TOTAL_HEADER_SIZE = 48;
inline constexpr std::size_t FSM_PAGE_ENTRIES_OFFSET = FSM_PAGE_TOTAL_HEADER_SIZE;
inline constexpr std::size_t FSM_PAGE_ENTRY_CAPACITY = PAGE_SIZE - FSM_PAGE_ENTRIES_OFFSET;
inline constexpr std::size_t FSM_MAX_CONTIGUOUS_FREE_BYTES =
    PAGE_SIZE - HEAP_PAGE_TOTAL_HEADER_SIZE;
inline constexpr std::size_t FSM_MAX_USABLE_INSERTION_BYTES = HEAP_PAGE_MAX_RAW_TUPLE_SIZE;

// Converts a heap page's contiguous upper-lower gap to an approximate category after accounting
// for the mandatory append slot. Inputs beyond the physical maximum are clamped.
[[nodiscard]] std::uint8_t FsmCategoryForFreeBytes(std::size_t free_bytes) noexcept;

// Returns the inclusive lower bound of usable tuple bytes represented by category.
[[nodiscard]] std::size_t FsmCategoryMinimumUsableBytes(std::uint8_t category) noexcept;

enum class FsmTupleRequestError : std::uint8_t {
    NONE,
    TUPLE_TOO_LARGE,
};

struct FsmMinimumCategoryResult {
    std::optional<std::uint8_t> category;
    FsmTupleRequestError error{FsmTupleRequestError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return category.has_value();
    }
};

// Returns the smallest category whose represented lower bound can satisfy the raw tuple size.
[[nodiscard]] FsmMinimumCategoryResult
MinimumFsmCategoryForTupleBytes(std::size_t required_tuple_bytes) noexcept;

struct FsmEntryLocation {
    PageNo fsm_page_no{INVALID_PAGE_NO};
    std::uint16_t entry_index{0};

    bool operator==(const FsmEntryLocation&) const = default;
};

enum class FsmPageMappingError : std::uint8_t {
    NONE,
    INVALID_HEAP_PAGE_NUMBER,
    OVERFLOW,
};

struct FsmPageMappingResult {
    std::optional<FsmEntryLocation> location;
    FsmPageMappingError error{FsmPageMappingError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return location.has_value();
    }
};

[[nodiscard]] FsmPageMappingResult FsmLocationForHeapPage(PageNo heap_page_no) noexcept;

struct FsmPageHeader {
    PageNo first_heap_page_no{1};
    std::uint16_t entry_count{0};
    std::uint16_t reserved16{0};
    std::uint32_t reserved32{0};

    bool operator==(const FsmPageHeader&) const = default;
};

[[nodiscard]] bool EncodeFsmPageHeader(std::span<std::byte> destination,
                                       const FsmPageHeader& header) noexcept;
[[nodiscard]] std::optional<FsmPageHeader>
DecodeFsmPageHeader(std::span<const std::byte> source) noexcept;

enum class FsmPageInitializeError : std::uint8_t {
    NONE,
    INVALID_FSM_PAGE_NUMBER,
    HEAP_PAGE_RANGE_OVERFLOW,
    ENTRY_COUNT_OUT_OF_RANGE,
    ENCODING_FAILED,
};

struct FsmPageInitializeResult {
    FsmPageInitializeError error{FsmPageInitializeError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FsmPageInitializeError::NONE;
    }
};

struct FsmPageInitialization {
    std::uint16_t entry_count{0};
    std::uint32_t flags{0};
    Lsn page_lsn{INVALID_LSN};
};

enum class FsmPageValidationError : std::uint8_t {
    NONE,
    COMMON_HEADER_DECODE_FAILED,
    WRONG_PAGE_TYPE,
    WRONG_PAGE_NUMBER,
    WRONG_HEADER_SIZE,
    UNSUPPORTED_FORMAT_VERSION,
    NONZERO_COMMON_RESERVED,
    FSM_HEADER_DECODE_FAILED,
    INVALID_FSM_PAGE_NUMBER,
    FIRST_HEAP_PAGE_OVERFLOW,
    FIRST_HEAP_PAGE_MISMATCH,
    ENTRY_COUNT_OUT_OF_RANGE,
    INITIALIZED_HEAP_RANGE_OVERFLOW,
    NONZERO_UNINITIALIZED_ENTRY,
    NONZERO_FSM_RESERVED,
};

struct FsmPageValidationResult {
    std::optional<CommonPageHeader> common_header;
    std::optional<FsmPageHeader> fsm_header;
    FsmPageValidationError error{FsmPageValidationError::NONE};
    std::size_t entry_index{0};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FsmPageValidationError::NONE;
    }
};

enum class FsmPageEntryError : std::uint8_t {
    NONE,
    PAGE_INVALID,
    ENTRY_OUT_OF_RANGE,
};

struct FsmPageCategoryResult {
    std::optional<std::uint8_t> category;
    FsmPageEntryError error{FsmPageEntryError::NONE};
    FsmPageValidationError page_error{FsmPageValidationError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return category.has_value();
    }
};

struct FsmPageUpdateResult {
    FsmPageEntryError error{FsmPageEntryError::NONE};
    FsmPageValidationError page_error{FsmPageValidationError::NONE};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error == FsmPageEntryError::NONE;
    }
};

class FsmPage {
  public:
    explicit FsmPage(Page& page) noexcept;

    [[nodiscard]] FsmPageInitializeResult
    Initialize(const FsmPageInitialization& initialization = {}) noexcept;
    [[nodiscard]] std::optional<FsmPageHeader> Header() const noexcept;
    [[nodiscard]] FsmPageValidationResult Validate() const noexcept;
    [[nodiscard]] FsmPageCategoryResult GetCategory(std::size_t entry_index) const noexcept;
    [[nodiscard]] FsmPageUpdateResult SetCategory(std::size_t entry_index,
                                                  std::uint8_t category) noexcept;

  private:
    Page* page_;
};

static_assert(FSM_PAGE_HEADER_OFFSET + FSM_PAGE_HEADER_ENCODED_SIZE == FSM_PAGE_TOTAL_HEADER_SIZE);
static_assert(FSM_PAGE_ENTRIES_OFFSET + FSM_PAGE_ENTRY_CAPACITY == PAGE_SIZE);
static_assert(FSM_PAGE_ENTRY_CAPACITY <= std::numeric_limits<std::uint16_t>::max());

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_FSM_PAGE_H_
