#include "storage/disk/disk_manager.h"

#include <algorithm>
#include <array>
#include <barrier>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <limits>
#include <span>
#include <string_view>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace dblusblus {
namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{
            '/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u',  's',
            '-', 'd', 'i', 's', 'k', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
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

[[nodiscard]] std::array<std::byte, PAGE_SIZE> PageFilledWith(std::byte value) {
    std::array<std::byte, PAGE_SIZE> page{};
    page.fill(value);
    return page;
}

[[nodiscard]] bool TruncatePath(const std::filesystem::path& path, std::uint64_t size) {
    int result = -1;
    do {
        result = ::truncate(path.c_str(), static_cast<off_t>(size));
    } while (result < 0 && errno == EINTR);
    return result == 0;
}

TEST(DiskManagerTest, CreatesOpensAndRegistersFilesWithoutTruncatingExistingData) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto first_path = temporary_directory.File("first.pages");
    const auto second_path = temporary_directory.File("second.pages");
    DiskManager manager;

    ASSERT_TRUE(manager.CreateFile(FileId{1}, first_path));
    const auto size = manager.FileSize(FileId{1});
    const auto count = manager.PageCount(FileId{1});
    ASSERT_TRUE(size);
    ASSERT_TRUE(count);
    EXPECT_EQ(size.value, std::uint64_t{0});
    EXPECT_EQ(count.value, PageNo{0});

    const auto duplicate = manager.CreateFile(FileId{1}, second_path);
    EXPECT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error.code, DiskErrorCode::DUPLICATE_FILE_ID);
    EXPECT_FALSE(std::filesystem::exists(second_path));

    const auto existing = manager.CreateFile(FileId{2}, first_path);
    EXPECT_FALSE(existing);
    EXPECT_EQ(existing.error.code, DiskErrorCode::SYSTEM_ERROR);
    EXPECT_EQ(existing.error.operation, DiskOperation::CREATE_FILE);
    EXPECT_EQ(existing.error.file_id, FileId{2});
    EXPECT_EQ(existing.error.path, first_path);
    EXPECT_EQ(existing.error.system_error, EEXIST);
    EXPECT_FALSE(existing.error.message.empty());

    ASSERT_TRUE(manager.CloseFile(FileId{1}));
    ASSERT_TRUE(manager.OpenFile(FileId{1}, first_path));
    EXPECT_TRUE(manager.CloseFile(FileId{1}));
}

TEST(DiskManagerTest, ExtendsByExactlyOnePageAndReturnsMonotonicPageNumbers) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("extend.pages");
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(FileId{7}, path));

    for (PageNo expected = 0; expected < 3; ++expected) {
        const auto allocated = manager.ExtendFile(FileId{7});
        ASSERT_TRUE(allocated);
        EXPECT_EQ(allocated.value, expected);

        const auto size = manager.FileSize(FileId{7});
        ASSERT_TRUE(size);
        EXPECT_EQ(size.value, (expected + 1U) * PAGE_SIZE);
        EXPECT_EQ(std::filesystem::file_size(path), (expected + 1U) * PAGE_SIZE);
    }
}

TEST(DiskManagerTest, ConcurrentExtensionsAllocateUniqueContiguousPageNumbers) {
    constexpr std::size_t WORKER_COUNT = 8;
    constexpr std::size_t EXTENSIONS_PER_WORKER = 64;
    constexpr std::size_t EXTENSION_COUNT = WORKER_COUNT * EXTENSIONS_PER_WORKER;

    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("concurrent-extend.pages");
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(FileId{8}, path));

    const auto initial_page_count = manager.PageCount(FileId{8});
    ASSERT_TRUE(initial_page_count);

    std::barrier start_barrier{static_cast<std::ptrdiff_t>(WORKER_COUNT + 1U)};
    std::array<std::uint8_t, EXTENSION_COUNT> successes{};
    std::array<PageNo, EXTENSION_COUNT> allocated_page_numbers{};
    std::array<std::thread, WORKER_COUNT> workers;

    for (std::size_t worker_index = 0; worker_index < WORKER_COUNT; ++worker_index) {
        workers[worker_index] = std::thread{[&, worker_index] {
            start_barrier.arrive_and_wait();
            for (std::size_t call_index = 0; call_index < EXTENSIONS_PER_WORKER; ++call_index) {
                const std::size_t result_index =
                    (worker_index * EXTENSIONS_PER_WORKER) + call_index;
                const auto allocated = manager.ExtendFile(FileId{8});
                successes[result_index] = static_cast<std::uint8_t>(allocated.success);
                if (allocated) {
                    allocated_page_numbers[result_index] = allocated.value;
                }
            }
        }};
    }

    start_barrier.arrive_and_wait();
    for (auto& worker : workers) {
        worker.join();
    }

    const auto successful_extensions =
        std::ranges::count(successes, static_cast<std::uint8_t>(true));
    ASSERT_EQ(successful_extensions, EXTENSION_COUNT);

    std::ranges::sort(allocated_page_numbers);
    EXPECT_EQ(std::ranges::adjacent_find(allocated_page_numbers), allocated_page_numbers.end());
    for (std::size_t index = 0; index < EXTENSION_COUNT; ++index) {
        EXPECT_EQ(allocated_page_numbers[index], initial_page_count.value + index);
    }

    const PageNo expected_page_count = initial_page_count.value + EXTENSION_COUNT;
    const std::uint64_t expected_file_size = expected_page_count * PAGE_SIZE;
    const auto final_size = manager.FileSize(FileId{8});
    ASSERT_TRUE(final_size);
    EXPECT_EQ(final_size.value, expected_file_size);
    EXPECT_EQ(std::filesystem::file_size(path), expected_file_size);
    EXPECT_EQ(final_size.value % PAGE_SIZE, std::uint64_t{0});
}

TEST(DiskManagerTest, MapsFileIdsAndReadsWritesPagesPositionally) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto first_path = temporary_directory.File("one.pages");
    const auto second_path = temporary_directory.File("two.pages");
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(FileId{11}, first_path));
    ASSERT_TRUE(manager.CreateFile(FileId{12}, second_path));
    ASSERT_TRUE(manager.ExtendFile(FileId{11}));
    ASSERT_TRUE(manager.ExtendFile(FileId{11}));
    ASSERT_TRUE(manager.ExtendFile(FileId{12}));

    const auto first_page = PageFilledWith(std::byte{0x11});
    const auto second_page = PageFilledWith(std::byte{0x22});
    const auto other_file_page = PageFilledWith(std::byte{0x33});

    ASSERT_TRUE(manager.WritePage(PageId{.file_id = 11, .page_no = 1}, second_page));
    ASSERT_TRUE(manager.WritePage(PageId{.file_id = 12, .page_no = 0}, other_file_page));
    ASSERT_TRUE(manager.WritePage(PageId{.file_id = 11, .page_no = 0}, first_page));

    std::array<std::byte, PAGE_SIZE> output{};
    ASSERT_TRUE(manager.ReadPage(PageId{.file_id = 11, .page_no = 0}, output));
    EXPECT_EQ(output, first_page);
    ASSERT_TRUE(manager.ReadPage(PageId{.file_id = 12, .page_no = 0}, output));
    EXPECT_EQ(output, other_file_page);
    ASSERT_TRUE(manager.ReadPage(PageId{.file_id = 11, .page_no = 1}, output));
    EXPECT_EQ(output, second_page);
}

TEST(DiskManagerTest, RejectsMissingPagesWithoutChangingReadDestination) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(FileId{21}, temporary_directory.File("missing.pages")));
    ASSERT_TRUE(manager.ExtendFile(FileId{21}));

    auto output = PageFilledWith(std::byte{0xA5});
    const auto original = output;
    const auto read = manager.ReadPage(PageId{.file_id = 21, .page_no = 1}, output);
    EXPECT_FALSE(read);
    EXPECT_EQ(read.error.code, DiskErrorCode::PAGE_NOT_FOUND);
    EXPECT_EQ(read.error.operation, DiskOperation::READ_PAGE);
    EXPECT_EQ(read.error.page_no, PageNo{1});
    EXPECT_EQ(output, original);

    const auto write = manager.WritePage(PageId{.file_id = 21, .page_no = 1}, output);
    EXPECT_FALSE(write);
    EXPECT_EQ(write.error.code, DiskErrorCode::PAGE_NOT_FOUND);
    const auto size = manager.FileSize(FileId{21});
    ASSERT_TRUE(size);
    EXPECT_EQ(size.value, PAGE_SIZE);
}

TEST(DiskManagerTest, RejectsMisalignedFilesAndReportsMidPageShortReads) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("misaligned.pages");
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(FileId{31}, path));
    ASSERT_TRUE(manager.ExtendFile(FileId{31}));
    ASSERT_TRUE(manager.ExtendFile(FileId{31}));
    ASSERT_TRUE(TruncatePath(path, PAGE_SIZE + 100U));

    const auto size = manager.FileSize(FileId{31});
    EXPECT_FALSE(size);
    EXPECT_EQ(size.error.code, DiskErrorCode::FILE_SIZE_NOT_PAGE_ALIGNED);

    auto output = PageFilledWith(std::byte{0xA5});
    const auto original = output;
    const auto read = manager.ReadPage(PageId{.file_id = 31, .page_no = 1}, output);
    EXPECT_FALSE(read);
    EXPECT_EQ(read.error.code, DiskErrorCode::SHORT_READ);
    EXPECT_EQ(output, original);

    ASSERT_TRUE(manager.CloseFile(FileId{31}));
    const auto open = manager.OpenFile(FileId{31}, path);
    EXPECT_FALSE(open);
    EXPECT_EQ(open.error.code, DiskErrorCode::FILE_SIZE_NOT_PAGE_ALIGNED);
}

TEST(DiskManagerTest, RejectsUnregisteredAndOverflowingPageIdsWithContext) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("overflow.pages");
    DiskManager manager;
    auto output = PageFilledWith(std::byte{0xA5});

    const auto unregistered = manager.ReadPage(PageId{.file_id = 99, .page_no = 4}, output);
    EXPECT_FALSE(unregistered);
    EXPECT_EQ(unregistered.error.code, DiskErrorCode::FILE_NOT_REGISTERED);
    EXPECT_EQ(unregistered.error.operation, DiskOperation::READ_PAGE);
    EXPECT_EQ(unregistered.error.file_id, FileId{99});
    EXPECT_EQ(unregistered.error.page_no, PageNo{4});
    EXPECT_FALSE(unregistered.error.message.empty());

    ASSERT_TRUE(manager.CreateFile(FileId{41}, path));
    const auto overflow = manager.ReadPage(
        PageId{.file_id = 41, .page_no = std::numeric_limits<PageNo>::max()}, output);
    EXPECT_FALSE(overflow);
    EXPECT_EQ(overflow.error.code, DiskErrorCode::OFFSET_OVERFLOW);
    EXPECT_EQ(overflow.error.path, path);
    EXPECT_EQ(overflow.error.page_no, std::numeric_limits<PageNo>::max());
}

TEST(DiskManagerTest, ReopeningPreservesBytesAndSyncsValidFiles) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("reopen.pages");
    const auto expected = PageFilledWith(std::byte{0x5A});

    {
        DiskManager manager;
        ASSERT_TRUE(manager.CreateFile(FileId{51}, path));
        ASSERT_TRUE(manager.ExtendFile(FileId{51}));
        ASSERT_TRUE(manager.WritePage(PageId{.file_id = 51, .page_no = 0}, expected));
        EXPECT_TRUE(manager.SyncFile(FileId{51}));
    }

    DiskManager reopened;
    ASSERT_TRUE(reopened.OpenFile(FileId{51}, path));
    std::array<std::byte, PAGE_SIZE> actual{};
    ASSERT_TRUE(reopened.ReadPage(PageId{.file_id = 51, .page_no = 0}, actual));
    EXPECT_EQ(actual, expected);
}

TEST(DiskManagerTest, RejectsInvalidFileIdsAndMissingPaths) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto missing_path = temporary_directory.File("does-not-exist.pages");
    DiskManager manager;

    const auto invalid = manager.CreateFile(INVALID_FILE_ID, missing_path);
    EXPECT_FALSE(invalid);
    EXPECT_EQ(invalid.error.code, DiskErrorCode::INVALID_FILE_IDENTIFIER);

    const auto missing = manager.OpenFile(FileId{61}, missing_path);
    EXPECT_FALSE(missing);
    EXPECT_EQ(missing.error.code, DiskErrorCode::SYSTEM_ERROR);
    EXPECT_EQ(missing.error.operation, DiskOperation::OPEN_FILE);
    EXPECT_EQ(missing.error.system_error, ENOENT);
    EXPECT_EQ(missing.error.path, missing_path);
    EXPECT_FALSE(missing.error.message.empty());
}

} // namespace
} // namespace dblusblus
