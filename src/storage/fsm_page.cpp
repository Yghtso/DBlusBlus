#include "storage/fsm_page.h"

#include "common/encoding.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace dblusblus {
namespace {

constexpr std::size_t FIRST_HEAP_PAGE_NO_OFFSET = 0;
constexpr std::size_t ENTRY_COUNT_OFFSET = 8;
constexpr std::size_t RESERVED16_OFFSET = 10;
constexpr std::size_t RESERVED32_OFFSET = 12;
constexpr std::size_t FSM_CATEGORY_MAX = std::numeric_limits<std::uint8_t>::max();

[[nodiscard]] bool FirstHeapPageForFsmPage(PageNo fsm_page_no,
                                           PageNo& first_heap_page_no) noexcept {
    if (fsm_page_no == 0 || fsm_page_no == INVALID_PAGE_NO) {
        return false;
    }

    const PageNo fsm_page_index = fsm_page_no - 1U;
    if (fsm_page_index > (std::numeric_limits<PageNo>::max() - 1U) / FSM_PAGE_ENTRY_CAPACITY) {
        return false;
    }
    first_heap_page_no = 1U + (fsm_page_index * FSM_PAGE_ENTRY_CAPACITY);
    return first_heap_page_no != INVALID_PAGE_NO;
}

[[nodiscard]] bool InitializedRangeFits(const FsmPageHeader& header) noexcept {
    if (header.entry_count == 0) {
        return true;
    }
    const auto final_entry_offset = static_cast<PageNo>(header.entry_count - 1U);
    return header.first_heap_page_no <=
           (std::numeric_limits<PageNo>::max() - 1U) - final_entry_offset;
}

[[nodiscard]] FsmPageValidationResult
ValidationFailure(FsmPageValidationError error,
                  std::optional<CommonPageHeader> common_header,
                  std::optional<FsmPageHeader> fsm_header = std::nullopt,
                  std::size_t entry_index = 0) noexcept {
    return {
        .common_header = common_header,
        .fsm_header = fsm_header,
        .error = error,
        .entry_index = entry_index,
    };
}

} // namespace

std::uint8_t FsmCategoryForFreeBytes(std::size_t free_bytes) noexcept {
    const std::size_t bounded_free_bytes = std::min(free_bytes, FSM_MAX_CONTIGUOUS_FREE_BYTES);
    if (bounded_free_bytes <= HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE) {
        return 0;
    }

    const std::size_t usable_bytes = std::min(
        bounded_free_bytes - HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE, FSM_MAX_USABLE_INSERTION_BYTES);
    return static_cast<std::uint8_t>((usable_bytes * FSM_CATEGORY_MAX) /
                                     FSM_MAX_USABLE_INSERTION_BYTES);
}

std::size_t FsmCategoryMinimumUsableBytes(std::uint8_t category) noexcept {
    const std::size_t scaled = static_cast<std::size_t>(category) * FSM_MAX_USABLE_INSERTION_BYTES;
    return (scaled + FSM_CATEGORY_MAX - 1U) / FSM_CATEGORY_MAX;
}

FsmPageMappingResult FsmLocationForHeapPage(PageNo heap_page_no) noexcept {
    if (heap_page_no == 0 || heap_page_no == INVALID_PAGE_NO) {
        return {
            .location = std::nullopt,
            .error = FsmPageMappingError::INVALID_HEAP_PAGE_NUMBER,
        };
    }

    const PageNo heap_data_index = heap_page_no - 1U;
    const PageNo fsm_page_index = heap_data_index / FSM_PAGE_ENTRY_CAPACITY;
    if (fsm_page_index == std::numeric_limits<PageNo>::max()) {
        return {.location = std::nullopt, .error = FsmPageMappingError::OVERFLOW};
    }
    const PageNo fsm_page_no = 1U + fsm_page_index;
    const auto entry_index = static_cast<std::uint16_t>(heap_data_index % FSM_PAGE_ENTRY_CAPACITY);
    return {
        .location =
            FsmEntryLocation{
                .fsm_page_no = fsm_page_no,
                .entry_index = entry_index,
            },
        .error = FsmPageMappingError::NONE,
    };
}

bool EncodeFsmPageHeader(std::span<std::byte> destination, const FsmPageHeader& header) noexcept {
    if (destination.size() < FSM_PAGE_HEADER_ENCODED_SIZE) {
        return false;
    }

    std::array<std::byte, FSM_PAGE_HEADER_ENCODED_SIZE> encoded{};
    const auto bytes = std::span<std::byte>{encoded};
    const bool encoded_all =
        EncodeLittleEndian(
            bytes.subspan(FIRST_HEAP_PAGE_NO_OFFSET, sizeof(header.first_heap_page_no)),
            header.first_heap_page_no) &&
        EncodeLittleEndian(bytes.subspan(ENTRY_COUNT_OFFSET, sizeof(header.entry_count)),
                           header.entry_count) &&
        EncodeLittleEndian(bytes.subspan(RESERVED16_OFFSET, sizeof(header.reserved16)),
                           header.reserved16) &&
        EncodeLittleEndian(bytes.subspan(RESERVED32_OFFSET, sizeof(header.reserved32)),
                           header.reserved32);
    if (!encoded_all) {
        return false;
    }

    std::ranges::copy(encoded, destination.begin());
    return true;
}

std::optional<FsmPageHeader> DecodeFsmPageHeader(std::span<const std::byte> source) noexcept {
    if (source.size() < FSM_PAGE_HEADER_ENCODED_SIZE) {
        return std::nullopt;
    }

    const auto first_heap_page_no =
        DecodeLittleEndian<PageNo>(source.subspan(FIRST_HEAP_PAGE_NO_OFFSET, sizeof(PageNo)));
    const auto entry_count = DecodeLittleEndian<std::uint16_t>(
        source.subspan(ENTRY_COUNT_OFFSET, sizeof(std::uint16_t)));
    const auto reserved16 =
        DecodeLittleEndian<std::uint16_t>(source.subspan(RESERVED16_OFFSET, sizeof(std::uint16_t)));
    const auto reserved32 =
        DecodeLittleEndian<std::uint32_t>(source.subspan(RESERVED32_OFFSET, sizeof(std::uint32_t)));
    if (!first_heap_page_no.has_value() || !entry_count.has_value() || !reserved16.has_value() ||
        !reserved32.has_value()) {
        return std::nullopt;
    }

    return FsmPageHeader{
        .first_heap_page_no = *first_heap_page_no,
        .entry_count = *entry_count,
        .reserved16 = *reserved16,
        .reserved32 = *reserved32,
    };
}

FsmPage::FsmPage(Page& page) noexcept : page_(&page) {}

FsmPageInitializeResult FsmPage::Initialize(const FsmPageInitialization& initialization) noexcept {
    PageNo first_heap_page_no = 0;
    if (page_->Id().page_no == 0 || page_->Id().page_no == INVALID_PAGE_NO) {
        return {.error = FsmPageInitializeError::INVALID_FSM_PAGE_NUMBER};
    }
    if (!FirstHeapPageForFsmPage(page_->Id().page_no, first_heap_page_no)) {
        return {.error = FsmPageInitializeError::HEAP_PAGE_RANGE_OVERFLOW};
    }
    if (initialization.entry_count > FSM_PAGE_ENTRY_CAPACITY) {
        return {.error = FsmPageInitializeError::ENTRY_COUNT_OUT_OF_RANGE};
    }

    const CommonPageHeader common_header{
        .page_type = PageType::FSM_DATA,
        .format_version = FSM_PAGE_FORMAT_VERSION,
        .flags = initialization.flags,
        .page_lsn = initialization.page_lsn,
        .checksum_crc32c = 0,
        .header_size = FSM_PAGE_TOTAL_HEADER_SIZE,
        .reserved16 = 0,
        .page_no = page_->Id().page_no,
    };
    const FsmPageHeader fsm_header{
        .first_heap_page_no = first_heap_page_no,
        .entry_count = initialization.entry_count,
        .reserved16 = 0,
        .reserved32 = 0,
    };
    if (!InitializedRangeFits(fsm_header)) {
        return {.error = FsmPageInitializeError::HEAP_PAGE_RANGE_OVERFLOW};
    }
    std::array<std::byte, COMMON_PAGE_HEADER_ENCODED_SIZE> encoded_common_header{};
    std::array<std::byte, FSM_PAGE_HEADER_ENCODED_SIZE> encoded_fsm_header{};
    if (!EncodeCommonPageHeader(encoded_common_header, common_header) ||
        !EncodeFsmPageHeader(encoded_fsm_header, fsm_header)) {
        return {.error = FsmPageInitializeError::ENCODING_FAILED};
    }

    auto page_bytes = page_->Bytes();
    std::ranges::fill(page_bytes, std::byte{0});
    std::ranges::copy(encoded_common_header, page_bytes.begin());
    std::ranges::copy(encoded_fsm_header,
                      page_bytes.begin() + static_cast<std::ptrdiff_t>(FSM_PAGE_HEADER_OFFSET));
    return {};
}

std::optional<FsmPageHeader> FsmPage::Header() const noexcept {
    return DecodeFsmPageHeader(
        page_->Bytes().subspan(FSM_PAGE_HEADER_OFFSET, FSM_PAGE_HEADER_ENCODED_SIZE));
}

FsmPageValidationResult FsmPage::Validate() const noexcept {
    const auto common_header = page_->DecodeHeader();
    if (!common_header.has_value()) {
        return ValidationFailure(FsmPageValidationError::COMMON_HEADER_DECODE_FAILED, std::nullopt);
    }
    if (common_header->page_type != PageType::FSM_DATA) {
        return ValidationFailure(FsmPageValidationError::WRONG_PAGE_TYPE, common_header);
    }
    if (common_header->page_no != page_->Id().page_no) {
        return ValidationFailure(FsmPageValidationError::WRONG_PAGE_NUMBER, common_header);
    }
    if (common_header->header_size != FSM_PAGE_TOTAL_HEADER_SIZE) {
        return ValidationFailure(FsmPageValidationError::WRONG_HEADER_SIZE, common_header);
    }
    if (common_header->format_version != FSM_PAGE_FORMAT_VERSION) {
        return ValidationFailure(FsmPageValidationError::UNSUPPORTED_FORMAT_VERSION, common_header);
    }
    if (common_header->reserved16 != 0) {
        return ValidationFailure(FsmPageValidationError::NONZERO_COMMON_RESERVED, common_header);
    }

    const auto fsm_header = Header();
    if (!fsm_header.has_value()) {
        return ValidationFailure(FsmPageValidationError::FSM_HEADER_DECODE_FAILED, common_header);
    }
    if (fsm_header->reserved16 != 0 || fsm_header->reserved32 != 0) {
        return ValidationFailure(
            FsmPageValidationError::NONZERO_FSM_RESERVED, common_header, fsm_header);
    }
    if (page_->Id().page_no == 0 || page_->Id().page_no == INVALID_PAGE_NO) {
        return ValidationFailure(
            FsmPageValidationError::INVALID_FSM_PAGE_NUMBER, common_header, fsm_header);
    }

    PageNo expected_first_heap_page_no = 0;
    if (!FirstHeapPageForFsmPage(page_->Id().page_no, expected_first_heap_page_no)) {
        return ValidationFailure(
            FsmPageValidationError::FIRST_HEAP_PAGE_OVERFLOW, common_header, fsm_header);
    }
    if (fsm_header->first_heap_page_no != expected_first_heap_page_no) {
        return ValidationFailure(
            FsmPageValidationError::FIRST_HEAP_PAGE_MISMATCH, common_header, fsm_header);
    }
    if (fsm_header->entry_count > FSM_PAGE_ENTRY_CAPACITY) {
        return ValidationFailure(
            FsmPageValidationError::ENTRY_COUNT_OUT_OF_RANGE, common_header, fsm_header);
    }
    if (!InitializedRangeFits(*fsm_header)) {
        return ValidationFailure(
            FsmPageValidationError::INITIALIZED_HEAP_RANGE_OVERFLOW, common_header, fsm_header);
    }

    const auto entries = page_->Bytes().subspan(FSM_PAGE_ENTRIES_OFFSET, FSM_PAGE_ENTRY_CAPACITY);
    for (std::size_t index = fsm_header->entry_count; index < entries.size(); ++index) {
        if (entries[index] != std::byte{0}) {
            return ValidationFailure(FsmPageValidationError::NONZERO_UNINITIALIZED_ENTRY,
                                     common_header,
                                     fsm_header,
                                     index);
        }
    }

    return {
        .common_header = common_header,
        .fsm_header = fsm_header,
        .error = FsmPageValidationError::NONE,
    };
}

FsmPageCategoryResult FsmPage::GetCategory(std::size_t entry_index) const noexcept {
    const auto validation = Validate();
    if (!validation || !validation.fsm_header.has_value()) {
        return {
            .category = std::nullopt,
            .error = FsmPageEntryError::PAGE_INVALID,
            .page_error = validation.error,
        };
    }
    if (entry_index >= validation.fsm_header->entry_count) {
        return {
            .category = std::nullopt,
            .error = FsmPageEntryError::ENTRY_OUT_OF_RANGE,
        };
    }

    return {
        .category =
            std::to_integer<std::uint8_t>(page_->Bytes()[FSM_PAGE_ENTRIES_OFFSET + entry_index]),
        .error = FsmPageEntryError::NONE,
    };
}

FsmPageUpdateResult FsmPage::SetCategory(std::size_t entry_index, std::uint8_t category) noexcept {
    const auto validation = Validate();
    if (!validation || !validation.fsm_header.has_value()) {
        return {
            .error = FsmPageEntryError::PAGE_INVALID,
            .page_error = validation.error,
        };
    }
    if (entry_index >= validation.fsm_header->entry_count) {
        return {.error = FsmPageEntryError::ENTRY_OUT_OF_RANGE};
    }

    page_->Bytes()[FSM_PAGE_ENTRIES_OFFSET + entry_index] = static_cast<std::byte>(category);
    return {};
}

} // namespace dblusblus
