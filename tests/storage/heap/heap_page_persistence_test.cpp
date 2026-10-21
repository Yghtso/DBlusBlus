#include "storage/disk/disk_manager.h"
#include "storage/file/file_superblock.h"
#include "storage/file/page_file.h"
#include "storage/heap/heap_page.h"
#include "storage/heap/heap_test_support.h"
#include "storage/heap/heap_test_tuples.h"
#include "storage/page/page.h"
#include "support/temporary_directory.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <span>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace dblusblus {
namespace {

using heap_test::SlotBytes;

using test::TemporaryDirectory;

using heap_test::MakeTestTuple;

TEST(HeapPageDeadTransitionIntegrationTest, PersistsDeadStateWithoutRemovingTupleBytes) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.Valid());
    const auto path = temporary_directory.File("heap-dead.pages");
    constexpr FileId file_id = 42;
    constexpr std::uint64_t object_id = 7002;
    PageId page_id{};

    {
        DiskManager manager;
        auto page_file = PageFile::Create(manager,
                                          path,
                                          FileSuperblock{
                                              .file_kind = FileKind::HEAP,
                                              .file_id = file_id,
                                              .object_id = object_id,
                                              .creation_epoch = 9002,
                                          });
        if (!page_file.Ok()) {
            ADD_FAILURE() << "heap page file creation failed";
            return;
        }
        const auto allocation = page_file->AllocatePage();
        if (!allocation.Ok()) {
            ADD_FAILURE() << "heap page allocation failed";
            return;
        }
        page_id = *allocation;

        Page page{page_id};
        HeapPage heap_page{page, heap_test::TestSchemaHistory()};
        ASSERT_TRUE(heap_page.Initialize());
        ASSERT_TRUE(heap_page.Insert(MakeTestTuple(60, std::byte{0x01})));
        ASSERT_TRUE(heap_page.Insert(MakeTestTuple(61, std::byte{0x10})));
        ASSERT_TRUE(heap_page.Insert(MakeTestTuple(62, std::byte{0x20})));
        ASSERT_TRUE(heap_page.MarkDead(1));
        ASSERT_TRUE(manager.WritePage(page_id, page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    DiskManager reopened_manager;
    auto reopened = PageFile::Open(reopened_manager, path, file_id, FileKind::HEAP, object_id);
    if (!reopened.Ok()) {
        ADD_FAILURE() << "heap page file reopen failed";
        return;
    }
    Page read{page_id};
    ASSERT_TRUE(reopened_manager.ReadPage(page_id, read.Bytes()));
    HeapPage read_heap_page{read, heap_test::TestSchemaHistory()};
    ASSERT_TRUE(read_heap_page.Validate());

    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(read, 1));
    if (!dead_slot.Ok()) {
        ADD_FAILURE() << "persisted DEAD slot did not decode";
        return;
    }
    EXPECT_EQ(dead_slot->state, HeapSlotState::DEAD);
    EXPECT_FALSE(read_heap_page.TupleBytes(1).has_value());
    const auto expected_dead_bytes = MakeTestTuple(61, std::byte{0x10});
    EXPECT_TRUE(
        std::ranges::equal(read.Bytes().subspan(dead_slot->tuple_offset, dead_slot->tuple_length),
                           expected_dead_bytes));

    const auto first = read_heap_page.TupleBytes(0);
    const auto third = read_heap_page.TupleBytes(2);
    if (!first.has_value() || !third.has_value()) {
        ADD_FAILURE() << "unaffected persisted NORMAL tuple was not retrievable";
        return;
    }
    const auto expected_first = MakeTestTuple(60, std::byte{0x01});
    const auto expected_third = MakeTestTuple(62, std::byte{0x20});
    EXPECT_TRUE(std::ranges::equal(*first, expected_first));
    EXPECT_TRUE(std::ranges::equal(*third, expected_third));
}

TEST(HeapPageCompactionIntegrationTest, PersistsCompactedGeometryStatesAndPayloads) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.Valid());
    const auto path = temporary_directory.File("heap-compact.pages");
    constexpr FileId file_id = 52;
    constexpr std::uint64_t object_id = 7003;
    PageId page_id{};

    {
        DiskManager manager;
        auto page_file = PageFile::Create(manager,
                                          path,
                                          FileSuperblock{
                                              .file_kind = FileKind::HEAP,
                                              .file_id = file_id,
                                              .object_id = object_id,
                                              .creation_epoch = 9003,
                                          });
        if (!page_file.Ok()) {
            ADD_FAILURE() << "heap page file creation failed";
            return;
        }
        const auto allocation = page_file->AllocatePage();
        if (!allocation.Ok()) {
            ADD_FAILURE() << "heap page allocation failed";
            return;
        }
        page_id = *allocation;

        Page page{page_id};
        HeapPage heap_page{page, heap_test::TestSchemaHistory()};
        ASSERT_TRUE(heap_page.Initialize());
        ASSERT_TRUE(heap_page.Insert(MakeTestTuple(70, std::byte{0x01})));
        ASSERT_TRUE(heap_page.Insert(MakeTestTuple(80, std::byte{0x10})));
        ASSERT_TRUE(heap_page.Insert(MakeTestTuple(90, std::byte{0x20})));
        ASSERT_TRUE(heap_page.MarkDead(1));
        ASSERT_TRUE(heap_page.Compact());
        ASSERT_TRUE(manager.WritePage(page_id, page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    DiskManager reopened_manager;
    auto reopened = PageFile::Open(reopened_manager, path, file_id, FileKind::HEAP, object_id);
    if (!reopened.Ok()) {
        ADD_FAILURE() << "heap page file reopen failed";
        return;
    }
    Page read{page_id};
    ASSERT_TRUE(reopened_manager.ReadPage(page_id, read.Bytes()));
    HeapPage heap_page{read, heap_test::TestSchemaHistory()};
    ASSERT_TRUE(heap_page.Validate());

    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "persisted compacted header did not decode";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{3});
    EXPECT_EQ(header->lower, HEAP_PAGE_TOTAL_HEADER_SIZE + (3 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    EXPECT_EQ(header->upper, PAGE_SIZE - 70 - 90);
    EXPECT_EQ(header->free_slot_head, INVALID_SLOT_ID);

    const auto first_slot = DecodeHeapSlotEntry(SlotBytes(read, 0));
    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(read, 1));
    const auto third_slot = DecodeHeapSlotEntry(SlotBytes(read, 2));
    if (!first_slot.Ok() || !dead_slot.Ok() || !third_slot.Ok()) {
        ADD_FAILURE() << "persisted compacted slot did not decode";
        return;
    }
    EXPECT_EQ(first_slot->tuple_offset, PAGE_SIZE - 70);
    EXPECT_EQ(dead_slot->state, HeapSlotState::DEAD);
    EXPECT_EQ(dead_slot->tuple_offset, std::uint16_t{0});
    EXPECT_EQ(dead_slot->tuple_length, std::uint16_t{0});
    EXPECT_EQ(third_slot->tuple_offset, PAGE_SIZE - 70 - 90);
    EXPECT_FALSE(heap_page.TupleBytes(1).has_value());

    const auto first = heap_page.TupleBytes(0);
    const auto third = heap_page.TupleBytes(2);
    if (!first.has_value() || !third.has_value()) {
        ADD_FAILURE() << "persisted live tuple was not retrievable";
        return;
    }
    const auto expected_first = MakeTestTuple(70, std::byte{0x01});
    const auto expected_third = MakeTestTuple(90, std::byte{0x20});
    EXPECT_TRUE(std::ranges::equal(*first, expected_first));
    EXPECT_TRUE(std::ranges::equal(*third, expected_third));
}

TEST(HeapPageIntegrationTest, PersistsAllocatedBlankHeapPageThroughDiskManager) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.Valid());
    const auto path = temporary_directory.File("heap.pages");
    constexpr FileId file_id = 41;
    DiskManager manager;
    auto page_file = PageFile::Create(manager,
                                      path,
                                      FileSuperblock{
                                          .file_kind = FileKind::HEAP,
                                          .file_id = file_id,
                                          .object_id = 7001,
                                          .creation_epoch = 9001,
                                      });
    if (!page_file.Ok()) {
        ADD_FAILURE() << "heap page file creation unexpectedly failed";
        return;
    }
    const auto allocation = page_file->AllocatePage();
    if (!allocation.Ok()) {
        ADD_FAILURE() << "heap page allocation unexpectedly failed";
        return;
    }

    Page written{*allocation};
    HeapPage written_heap_page{written, heap_test::TestSchemaHistory()};
    ASSERT_TRUE(written_heap_page.Initialize());
    const auto first = MakeTestTuple(64, std::byte{0xFF});
    const auto second = MakeTestTuple(65, std::byte{0x10});
    ASSERT_TRUE(written_heap_page.Insert(first));
    ASSERT_TRUE(written_heap_page.Insert(second));
    ASSERT_TRUE(manager.WritePage(written.Id(), written.Bytes()));

    Page read{*allocation};
    ASSERT_TRUE(manager.ReadPage(read.Id(), read.Bytes()));
    HeapPage read_heap_page{read, heap_test::TestSchemaHistory()};
    const auto validation = read_heap_page.Validate();
    EXPECT_TRUE(validation);
    EXPECT_TRUE(std::ranges::equal(read.Bytes(), written.Bytes()));
    const auto stored_first = read_heap_page.TupleBytes(0);
    const auto stored_second = read_heap_page.TupleBytes(1);
    if (!stored_first.has_value() || !stored_second.has_value()) {
        ADD_FAILURE() << "persisted tuples were not retrievable";
        return;
    }
    EXPECT_TRUE(std::ranges::equal(*stored_first, first));
    EXPECT_TRUE(std::ranges::equal(*stored_second, second));
}

} // namespace
} // namespace dblusblus
