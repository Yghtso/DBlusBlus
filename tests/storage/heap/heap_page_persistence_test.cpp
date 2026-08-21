#include "storage/disk/disk_manager.h"
#include "storage/file/file_superblock.h"
#include "storage/file/page_file.h"
#include "storage/heap/heap_page.h"
#include "storage/page/page.h"

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

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{
            '/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u',  's',
            '-', 'h', 'e', 'a', 'p', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
        };
        if (char* created = ::mkdtemp(path_template.data()); created != nullptr) {
            path_ = created;
        }
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;

    [[nodiscard]] bool valid() const noexcept {
        return !path_.empty();
    }

    [[nodiscard]] std::filesystem::path File(std::string_view name) const {
        return path_ / name;
    }

  private:
    std::filesystem::path path_;
};

[[nodiscard]] std::span<std::byte> SlotBytes(Page& page, SlotId slot_id) {
    return page.Bytes().subspan(
        HEAP_PAGE_SLOT_DIRECTORY_OFFSET +
            (static_cast<std::size_t>(slot_id) * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE),
        HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE);
}

TEST(HeapPageDeadTransitionIntegrationTest, PersistsDeadStateWithoutRemovingTupleBytes) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
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
        if (!page_file.page_file.has_value()) {
            ADD_FAILURE() << "heap page file creation failed";
            return;
        }
        const auto allocation = page_file.page_file->AllocatePage();
        if (!allocation.page_id.has_value()) {
            ADD_FAILURE() << "heap page allocation failed";
            return;
        }
        page_id = *allocation.page_id;

        Page page{page_id};
        HeapPage heap_page{page};
        ASSERT_TRUE(heap_page.Initialize());
        constexpr std::array first{std::byte{0x01}, std::byte{0x02}};
        constexpr std::array second{std::byte{0x10}, std::byte{0x00}, std::byte{0xFF}};
        constexpr std::array third{std::byte{0x20}};
        ASSERT_TRUE(heap_page.Insert(first));
        ASSERT_TRUE(heap_page.Insert(second));
        ASSERT_TRUE(heap_page.Insert(third));
        ASSERT_TRUE(heap_page.MarkDead(1));
        ASSERT_TRUE(manager.WritePage(page_id, page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    DiskManager reopened_manager;
    auto reopened = PageFile::Open(reopened_manager, path, file_id, FileKind::HEAP, object_id);
    if (!reopened.page_file.has_value()) {
        ADD_FAILURE() << "heap page file reopen failed";
        return;
    }
    Page read{page_id};
    ASSERT_TRUE(reopened_manager.ReadPage(page_id, read.Bytes()));
    HeapPage read_heap_page{read};
    ASSERT_TRUE(read_heap_page.Validate());

    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(read, 1));
    if (!dead_slot.entry.has_value()) {
        ADD_FAILURE() << "persisted DEAD slot did not decode";
        return;
    }
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_FALSE(read_heap_page.TupleBytes(1).has_value());
    constexpr std::array expected_dead_bytes{std::byte{0x10}, std::byte{0x00}, std::byte{0xFF}};
    EXPECT_TRUE(std::ranges::equal(
        read.Bytes().subspan(dead_slot.entry->tuple_offset, dead_slot.entry->tuple_length),
        expected_dead_bytes));

    const auto first = read_heap_page.TupleBytes(0);
    const auto third = read_heap_page.TupleBytes(2);
    if (!first.has_value() || !third.has_value()) {
        ADD_FAILURE() << "unaffected persisted NORMAL tuple was not retrievable";
        return;
    }
    constexpr std::array expected_first{std::byte{0x01}, std::byte{0x02}};
    constexpr std::array expected_third{std::byte{0x20}};
    EXPECT_TRUE(std::ranges::equal(*first, expected_first));
    EXPECT_TRUE(std::ranges::equal(*third, expected_third));
}

TEST(HeapPageCompactionIntegrationTest, PersistsCompactedGeometryStatesAndPayloads) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
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
        if (!page_file.page_file.has_value()) {
            ADD_FAILURE() << "heap page file creation failed";
            return;
        }
        const auto allocation = page_file.page_file->AllocatePage();
        if (!allocation.page_id.has_value()) {
            ADD_FAILURE() << "heap page allocation failed";
            return;
        }
        page_id = *allocation.page_id;

        Page page{page_id};
        HeapPage heap_page{page};
        ASSERT_TRUE(heap_page.Initialize());
        constexpr std::array first{std::byte{0x01}, std::byte{0x02}};
        constexpr std::array second{
            std::byte{0x10}, std::byte{0x11}, std::byte{0x12}, std::byte{0x13}};
        constexpr std::array third{std::byte{0x20}, std::byte{0x21}, std::byte{0x22}};
        ASSERT_TRUE(heap_page.Insert(first));
        ASSERT_TRUE(heap_page.Insert(second));
        ASSERT_TRUE(heap_page.Insert(third));
        ASSERT_TRUE(heap_page.MarkDead(1));
        ASSERT_TRUE(heap_page.Compact());
        ASSERT_TRUE(manager.WritePage(page_id, page.Bytes()));
        ASSERT_TRUE(manager.SyncFile(file_id));
    }

    DiskManager reopened_manager;
    auto reopened = PageFile::Open(reopened_manager, path, file_id, FileKind::HEAP, object_id);
    if (!reopened.page_file.has_value()) {
        ADD_FAILURE() << "heap page file reopen failed";
        return;
    }
    Page read{page_id};
    ASSERT_TRUE(reopened_manager.ReadPage(page_id, read.Bytes()));
    HeapPage heap_page{read};
    ASSERT_TRUE(heap_page.Validate());

    const auto header = heap_page.Header();
    if (!header.has_value()) {
        ADD_FAILURE() << "persisted compacted header did not decode";
        return;
    }
    EXPECT_EQ(header->slot_count, std::uint16_t{3});
    EXPECT_EQ(header->lower, HEAP_PAGE_TOTAL_HEADER_SIZE + (3 * HEAP_PAGE_SLOT_ENTRY_ENCODED_SIZE));
    EXPECT_EQ(header->upper, PAGE_SIZE - 5);
    EXPECT_EQ(header->free_slot_head, INVALID_SLOT_ID);

    const auto first_slot = DecodeHeapSlotEntry(SlotBytes(read, 0));
    const auto dead_slot = DecodeHeapSlotEntry(SlotBytes(read, 1));
    const auto third_slot = DecodeHeapSlotEntry(SlotBytes(read, 2));
    if (!first_slot.entry.has_value() || !dead_slot.entry.has_value() ||
        !third_slot.entry.has_value()) {
        ADD_FAILURE() << "persisted compacted slot did not decode";
        return;
    }
    EXPECT_EQ(first_slot.entry->tuple_offset, PAGE_SIZE - 2);
    EXPECT_EQ(dead_slot.entry->state, HeapSlotState::DEAD);
    EXPECT_EQ(dead_slot.entry->tuple_offset, std::uint16_t{0});
    EXPECT_EQ(dead_slot.entry->tuple_length, std::uint16_t{0});
    EXPECT_EQ(third_slot.entry->tuple_offset, PAGE_SIZE - 5);
    EXPECT_FALSE(heap_page.TupleBytes(1).has_value());

    const auto first = heap_page.TupleBytes(0);
    const auto third = heap_page.TupleBytes(2);
    if (!first.has_value() || !third.has_value()) {
        ADD_FAILURE() << "persisted live tuple was not retrievable";
        return;
    }
    constexpr std::array expected_first{std::byte{0x01}, std::byte{0x02}};
    constexpr std::array expected_third{std::byte{0x20}, std::byte{0x21}, std::byte{0x22}};
    EXPECT_TRUE(std::ranges::equal(*first, expected_first));
    EXPECT_TRUE(std::ranges::equal(*third, expected_third));
}

TEST(HeapPageIntegrationTest, PersistsAllocatedBlankHeapPageThroughDiskManager) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
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
    if (!page_file.page_file.has_value()) {
        ADD_FAILURE() << "heap page file creation unexpectedly failed";
        return;
    }
    const auto allocation = page_file.page_file->AllocatePage();
    if (!allocation.page_id.has_value()) {
        ADD_FAILURE() << "heap page allocation unexpectedly failed";
        return;
    }

    Page written{*allocation.page_id};
    HeapPage written_heap_page{written};
    ASSERT_TRUE(written_heap_page.Initialize());
    constexpr std::array first{std::byte{0x01}, std::byte{0x00}, std::byte{0xFF}};
    constexpr std::array second{std::byte{0x10}, std::byte{0x20}};
    ASSERT_TRUE(written_heap_page.Insert(first));
    ASSERT_TRUE(written_heap_page.Insert(second));
    ASSERT_TRUE(manager.WritePage(written.Id(), written.Bytes()));

    Page read{*allocation.page_id};
    ASSERT_TRUE(manager.ReadPage(read.Id(), read.Bytes()));
    HeapPage read_heap_page{read};
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
