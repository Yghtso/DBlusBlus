#include "storage/page/page.h"

#include <cstddef>
#include <optional>

namespace dblusblus {

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

bool Page::Initialize(const CommonPageHeader& header) noexcept {
    bytes_.fill(std::byte{0});
    return WriteHeader(header);
}

std::optional<CommonPageHeader> Page::DecodeHeader() const noexcept {
    return DecodeCommonPageHeader(Bytes());
}

bool Page::WriteHeader(const CommonPageHeader& header) noexcept {
    return EncodeCommonPageHeader(Bytes(), header);
}

PageHeaderValidationResult Page::ValidateHeader(PageType expected_type) const noexcept {
    auto header = DecodeHeader();
    if (!header.has_value()) {
        return {.header = std::nullopt, .error = PageHeaderValidationError::DECODE_FAILED};
    }
    if (header->page_type != expected_type) {
        return {.header = header, .error = PageHeaderValidationError::UNEXPECTED_PAGE_TYPE};
    }
    if (header->page_no != page_id_.page_no) {
        return {.header = header, .error = PageHeaderValidationError::UNEXPECTED_PAGE_NUMBER};
    }
    return {.header = header, .error = PageHeaderValidationError::NONE};
}

} // namespace dblusblus
