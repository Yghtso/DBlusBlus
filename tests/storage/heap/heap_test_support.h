#pragma once

#include "storage/heap/heap_page.h"
#include "storage/heap/heap_page_format.h"
#include "storage/heap/heap_test_tuples.h"
#include "storage/page/page.h"
#include "storage/page/page_header.h"

#include <algorithm>
#include <cstddef>
#include <gtest/gtest.h>
#include <span>

// Byte-level helpers shared by the HeapPage tests.
namespace dblusblus::heap_test {

[[nodiscard]] inline std::span<std::byte> HeapHeaderBytes(Page& page) {
    return page.Bytes().subspan(HEAP_PAGE_HEADER_OFFSET, HEAP_PAGE_HEADER_ENCODED_SIZE);
}

[[nodiscard]] inline std::span<std::byte> SlotBytes(Page& page, SlotId slot_id) {
    return page.Bytes().subspan(
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
            (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE),
        HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
}

[[nodiscard]] inline Page::ByteStorage CopyPageBytes(const Page& page) {
    Page::ByteStorage copy{};
    std::ranges::copy(page.Bytes(), copy.begin());
    return copy;
}

inline void WriteHeapHeader(Page& page, const HeapPageHeader& header) {
    ASSERT_TRUE(EncodeHeapPageHeader(HeapHeaderBytes(page), header));
}

inline void WriteCommonHeader(Page& page, const CommonPageHeader& header) {
    ASSERT_TRUE(page.WriteHeader(header));
}

[[nodiscard]] inline Page InitializedHeapPage(PageId page_id = {.file_id = 7, .page_no = 3}) {
    Page page{page_id};
    HeapPage heap_page{page, TestSchemaHistory()};
    if (!heap_page.Initialize()) {
        ADD_FAILURE() << "heap page initialization unexpectedly failed";
    }
    return page;
}

} // namespace dblusblus::heap_test
