#pragma once

#include "common/result.h"
#include "common/types.h"
#include "storage/page/page_header.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace dblusblus {

enum class PageHeaderValidationError : std::uint8_t {
    DECODE_FAILED,
    UNEXPECTED_PAGE_TYPE,
    UNEXPECTED_PAGE_NUMBER,
};

using PageHeaderValidationResult = Result<CommonPageHeader, PageHeaderValidationError>;

class Page {
  public:
    using ByteStorage = std::array<std::byte, PAGE_SIZE>;

    Page() = default;
    explicit Page(PageId page_id) noexcept;

    [[nodiscard]] PageId Id() const noexcept;

    [[nodiscard]] std::span<std::byte, PAGE_SIZE> Bytes() noexcept;
    [[nodiscard]] std::span<const std::byte, PAGE_SIZE> Bytes() const noexcept;

    // Zero-fills the page and writes header.
    void Initialize(const CommonPageHeader& header) noexcept;
    [[nodiscard]] std::optional<CommonPageHeader> DecodeHeader() const noexcept;
    void WriteHeader(const CommonPageHeader& header) noexcept;
    [[nodiscard]] PageHeaderValidationResult ValidateHeader(PageType expected_type) const noexcept;

  private:
    PageId page_id_{};
    ByteStorage bytes_{};
};

static_assert(sizeof(Page::ByteStorage) == PAGE_SIZE);

} // namespace dblusblus
