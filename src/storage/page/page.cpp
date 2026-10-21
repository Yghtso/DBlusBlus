#include "storage/page/page.h"

#include "common/assert.h"

#include <cstddef>
#include <optional>

namespace dblusblus::storage {

Page::Page(PageId page_id) noexcept : page_id_(page_id) {}

PageId Page::Id() const noexcept {
    return page_id_;
}

std::span<std::byte, PAGE_SIZE> Page::Bytes() noexcept {
    return bytes_;
}

std::span<const std::byte, PAGE_SIZE> Page::Bytes() const noexcept {
    return bytes_;
}

void Page::Initialize(const CommonPageHeader& header) noexcept {
    bytes_.fill(std::byte{0});
    WriteHeader(header);
}

std::optional<CommonPageHeader> Page::DecodeHeader() const noexcept {
    return DecodeCommonPageHeader(Bytes());
}

void Page::WriteHeader(const CommonPageHeader& header) noexcept {
    // A whole page always has room for the common header.
    const bool encoded = EncodeCommonPageHeader(Bytes(), header);
    DBLUS_CHECK(encoded, "common page header must fit in a page");
}

PageHeaderValidationResult Page::ValidateHeader(PageType expected_type) const noexcept {
    auto header = DecodeHeader();
    if (!header.has_value()) {
        return Unexpected{PageHeaderValidationError::DECODE_FAILED};
    }
    if (header->page_type != expected_type) {
        return Unexpected{PageHeaderValidationError::UNEXPECTED_PAGE_TYPE};
    }
    if (header->page_no != page_id_.page_no) {
        return Unexpected{PageHeaderValidationError::UNEXPECTED_PAGE_NUMBER};
    }
    return *header;
}

} // namespace dblusblus::storage
