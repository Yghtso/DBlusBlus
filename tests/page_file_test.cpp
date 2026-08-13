#include "common/file_superblock.h"
#include "common/page_header.h"
#include "storage/disk_manager.h"
#include "storage/page.h"
#include "storage/page_file.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace dblusblus {
namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        std::array path_template{
            '/', 't', 'm', 'p', '/', 'd', 'b', 'l', 'u', 's', 'b', 'l', 'u', 's', '-',  'p',
            'a', 'g', 'e', 'f', 'i', 'l', 'e', '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0',
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

[[nodiscard]] FileSuperblock MakeSuperblock(FileId file_id,
                                            FileKind file_kind,
                                            std::uint64_t object_id,
                                            std::uint64_t creation_epoch) {
    return FileSuperblock{
        .file_kind = file_kind,
        .file_id = file_id,
        .flags = 0xA5F00F5AU,
        .page_lsn = INVALID_LSN,
        .object_id = object_id,
        .creation_epoch = creation_epoch,
    };
}

[[nodiscard]] bool TruncatePath(const std::filesystem::path& path, std::uint64_t size) {
    int result = -1;
    do {
        result = ::truncate(path.c_str(), static_cast<off_t>(size));
    } while (result < 0 && errno == EINTR);
    return result == 0;
}

TEST(PageFileTest, CreatesOnePageFileWithValidDurableSuperblock) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("heap.pages");
    const auto expected = MakeSuperblock(FileId{101}, FileKind::HEAP, 7001, 9001);
    DiskManager manager;

    auto created = PageFile::Create(manager, path, expected);
    if (!created.page_file.has_value()) {
        ADD_FAILURE() << "page file creation unexpectedly failed";
        return;
    }

    EXPECT_EQ(created.page_file->Superblock(), expected);
    const auto page_count = manager.PageCount(expected.file_id);
    const auto file_size = manager.FileSize(expected.file_id);
    ASSERT_TRUE(page_count);
    ASSERT_TRUE(file_size);
    EXPECT_EQ(page_count.value, PageNo{1});
    EXPECT_EQ(file_size.value, PAGE_SIZE);
    EXPECT_EQ(std::filesystem::file_size(path), PAGE_SIZE);

    Page page_zero{PageId{.file_id = expected.file_id, .page_no = 0}};
    ASSERT_TRUE(manager.ReadPage(page_zero.Id(), page_zero.Bytes()));
    const auto decoded = DecodeFileSuperblock(page_zero.Bytes());
    if (!decoded.superblock.has_value()) {
        ADD_FAILURE() << "created superblock did not validate";
        return;
    }
    EXPECT_EQ(*decoded.superblock, expected);

    const auto common_header = page_zero.DecodeHeader();
    if (!common_header.has_value()) {
        ADD_FAILURE() << "created superblock common header did not decode";
        return;
    }
    EXPECT_EQ(common_header->page_type, PageType::SUPERBLOCK);
    EXPECT_EQ(common_header->format_version, FILE_SUPERBLOCK_FORMAT_VERSION);
    EXPECT_EQ(common_header->header_size, FILE_SUPERBLOCK_HEADER_SIZE);
    EXPECT_EQ(common_header->page_no, PageNo{0});
    EXPECT_NE(common_header->checksum_crc32c, std::uint32_t{0});
}

TEST(PageFileTest, ReopensValidatedFileAndRetainsPageCountAfterAllocations) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("reopen.pages");
    const auto expected = MakeSuperblock(FileId{102}, FileKind::BTREE, 7102, 9102);
    DiskManager manager;

    {
        auto created = PageFile::Create(manager, path, expected);
        if (!created.page_file.has_value()) {
            ADD_FAILURE() << "page file creation unexpectedly failed";
            return;
        }
        ASSERT_TRUE(created.page_file->AllocatePage());
        ASSERT_TRUE(created.page_file->AllocatePage());
    }

    auto opened =
        PageFile::Open(manager, path, expected.file_id, expected.file_kind, expected.object_id);
    if (!opened.page_file.has_value()) {
        ADD_FAILURE() << "page file reopen unexpectedly failed";
        return;
    }
    EXPECT_EQ(opened.page_file->Superblock(), expected);
    const auto page_count = manager.PageCount(expected.file_id);
    ASSERT_TRUE(page_count);
    EXPECT_EQ(page_count.value, PageNo{3});
}

TEST(PageFileTest, RejectsIdentityMismatchesWithExpectedAndActualContext) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("identity.pages");
    const auto expected = MakeSuperblock(FileId{103}, FileKind::FSM, 7103, 9103);
    DiskManager manager;
    {
        auto created = PageFile::Create(manager, path, expected);
        ASSERT_TRUE(created);
    }

    const auto wrong_file_id =
        PageFile::Open(manager, path, FileId{203}, expected.file_kind, expected.object_id);
    EXPECT_FALSE(wrong_file_id);
    EXPECT_EQ(wrong_file_id.error.code, PageFileErrorCode::FILE_ID_MISMATCH);
    EXPECT_EQ(wrong_file_id.error.expected_file_id, FileId{203});
    EXPECT_EQ(wrong_file_id.error.actual_file_id, expected.file_id);
    ASSERT_TRUE(manager.OpenFile(FileId{203}, path));
    ASSERT_TRUE(manager.CloseFile(FileId{203}));

    const auto wrong_kind =
        PageFile::Open(manager, path, expected.file_id, FileKind::HEAP, expected.object_id);
    EXPECT_FALSE(wrong_kind);
    EXPECT_EQ(wrong_kind.error.code, PageFileErrorCode::FILE_KIND_MISMATCH);
    EXPECT_EQ(wrong_kind.error.expected_file_kind, FileKind::HEAP);
    EXPECT_EQ(wrong_kind.error.actual_file_kind, expected.file_kind);

    const auto wrong_object =
        PageFile::Open(manager, path, expected.file_id, expected.file_kind, 9999);
    EXPECT_FALSE(wrong_object);
    EXPECT_EQ(wrong_object.error.code, PageFileErrorCode::OBJECT_ID_MISMATCH);
    EXPECT_EQ(wrong_object.error.expected_object_id, std::uint64_t{9999});
    EXPECT_EQ(wrong_object.error.actual_object_id, expected.object_id);

    auto unchecked_object = PageFile::Open(manager, path, expected.file_id, expected.file_kind);
    EXPECT_TRUE(unchecked_object);
}

TEST(PageFileTest, RejectsCorruptAndMissingSuperblocksAndCleansRegistration) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto corrupt_path = temporary_directory.File("corrupt.pages");
    const auto empty_path = temporary_directory.File("empty.pages");
    const auto expected = MakeSuperblock(FileId{104}, FileKind::CATALOG, 7104, 9104);
    DiskManager manager;
    {
        auto created = PageFile::Create(manager, corrupt_path, expected);
        ASSERT_TRUE(created);
    }

    ASSERT_TRUE(manager.OpenFile(expected.file_id, corrupt_path));
    Page page_zero{PageId{.file_id = expected.file_id, .page_no = 0}};
    ASSERT_TRUE(manager.ReadPage(page_zero.Id(), page_zero.Bytes()));
    page_zero.Bytes().back() ^= std::byte{0x01};
    ASSERT_TRUE(manager.WritePage(page_zero.Id(), page_zero.Bytes()));
    ASSERT_TRUE(manager.CloseFile(expected.file_id));

    const auto corrupt =
        PageFile::Open(manager, corrupt_path, expected.file_id, expected.file_kind);
    EXPECT_FALSE(corrupt);
    EXPECT_EQ(corrupt.error.code, PageFileErrorCode::SUPERBLOCK_INVALID);
    EXPECT_EQ(corrupt.error.superblock_error, FileSuperblockDecodeError::CHECKSUM_MISMATCH);
    ASSERT_TRUE(manager.OpenFile(expected.file_id, corrupt_path));
    ASSERT_TRUE(manager.CloseFile(expected.file_id));

    ASSERT_TRUE(manager.CreateFile(FileId{105}, empty_path));
    ASSERT_TRUE(manager.CloseFile(FileId{105}));
    const auto empty = PageFile::Open(manager, empty_path, FileId{105}, FileKind::HEAP);
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.error.code, PageFileErrorCode::MISSING_SUPERBLOCK);
    EXPECT_EQ(empty.error.actual_page_count, PageNo{0});
    ASSERT_TRUE(manager.OpenFile(FileId{105}, empty_path));
    EXPECT_TRUE(manager.CloseFile(FileId{105}));
}

TEST(PageFileTest, PropagatesMisalignedFileErrorFromDiskManager) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("misaligned.pages");
    DiskManager manager;
    ASSERT_TRUE(manager.CreateFile(FileId{106}, path));
    ASSERT_TRUE(manager.CloseFile(FileId{106}));
    ASSERT_TRUE(TruncatePath(path, PAGE_SIZE + 1U));

    const auto opened = PageFile::Open(manager, path, FileId{106}, FileKind::HEAP);
    EXPECT_FALSE(opened);
    EXPECT_EQ(opened.error.code, PageFileErrorCode::DISK_ERROR);
    EXPECT_EQ(opened.error.operation, PageFileOperation::OPEN);
    EXPECT_EQ(opened.error.disk_error.code, DiskErrorCode::FILE_SIZE_NOT_PAGE_ALIGNED);
    EXPECT_EQ(opened.error.disk_error.path, path);
}

TEST(PageFileTest, AllocatesAppendOnlyZeroedPagesBeginningAtOne) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("allocate.pages");
    const auto superblock = MakeSuperblock(FileId{107}, FileKind::HEAP, 7107, 9107);
    DiskManager manager;
    auto created = PageFile::Create(manager, path, superblock);
    if (!created.page_file.has_value()) {
        ADD_FAILURE() << "page file creation unexpectedly failed";
        return;
    }

    for (PageNo expected_page_no = 1; expected_page_no <= 3; ++expected_page_no) {
        const auto allocation = created.page_file->AllocatePage();
        if (!allocation.page_id.has_value()) {
            ADD_FAILURE() << "ordinary page allocation unexpectedly failed";
            return;
        }
        EXPECT_EQ(*allocation.page_id,
                  (PageId{.file_id = superblock.file_id, .page_no = expected_page_no}));

        const auto page_count = manager.PageCount(superblock.file_id);
        ASSERT_TRUE(page_count);
        EXPECT_EQ(page_count.value, expected_page_no + 1U);
        EXPECT_EQ(std::filesystem::file_size(path), (expected_page_no + 1U) * PAGE_SIZE);

        Page allocated_page{*allocation.page_id};
        ASSERT_TRUE(manager.ReadPage(allocated_page.Id(), allocated_page.Bytes()));
        EXPECT_TRUE(std::ranges::all_of(allocated_page.Bytes(),
                                        [](std::byte value) { return value == std::byte{0}; }));
    }
}

TEST(PageFileTest, NeverReturnsPageZeroAsAnOrdinaryAllocation) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("page-zero.pages");
    const auto superblock = MakeSuperblock(FileId{108}, FileKind::HEAP, 7108, 9108);
    DiskManager manager;
    auto created = PageFile::Create(manager, path, superblock);
    if (!created.page_file.has_value()) {
        ADD_FAILURE() << "page file creation unexpectedly failed";
        return;
    }
    ASSERT_TRUE(TruncatePath(path, 0));

    const auto allocation = created.page_file->AllocatePage();
    EXPECT_FALSE(allocation);
    EXPECT_EQ(allocation.error.code, PageFileErrorCode::DATA_PAGE_ZERO_ALLOCATED);
    EXPECT_EQ(allocation.error.actual_page_no, PageNo{0});
}

TEST(PageFileTest, KeepsIndependentFileAllocationSequences) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    DiskManager manager;
    auto first = PageFile::Create(manager,
                                  temporary_directory.File("first.pages"),
                                  MakeSuperblock(FileId{109}, FileKind::HEAP, 7109, 9109));
    auto second = PageFile::Create(manager,
                                   temporary_directory.File("second.pages"),
                                   MakeSuperblock(FileId{110}, FileKind::FSM, 7110, 9110));
    if (!first.page_file.has_value() || !second.page_file.has_value()) {
        ADD_FAILURE() << "independent page file creation unexpectedly failed";
        return;
    }

    const auto first_page_one = first.page_file->AllocatePage();
    const auto second_page_one = second.page_file->AllocatePage();
    const auto first_page_two = first.page_file->AllocatePage();
    if (!first_page_one.page_id.has_value() || !second_page_one.page_id.has_value() ||
        !first_page_two.page_id.has_value()) {
        ADD_FAILURE() << "independent page allocation unexpectedly failed";
        return;
    }
    EXPECT_EQ(*first_page_one.page_id, (PageId{.file_id = 109, .page_no = 1}));
    EXPECT_EQ(*second_page_one.page_id, (PageId{.file_id = 110, .page_no = 1}));
    EXPECT_EQ(*first_page_two.page_id, (PageId{.file_id = 109, .page_no = 2}));
}

TEST(PageFileTest, SupportsEveryLockedRandomAccessFileKind) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    constexpr std::array file_kinds{
        FileKind::HEAP,
        FileKind::BTREE,
        FileKind::FSM,
        FileKind::CATALOG,
    };
    DiskManager manager;

    for (std::size_t index = 0; index < file_kinds.size(); ++index) {
        const auto file_id = static_cast<FileId>(120U + index);
        const auto object_id = std::uint64_t{7200} + index;
        const auto path = temporary_directory.File("kind-" + std::to_string(index) + ".pages");
        {
            auto created = PageFile::Create(
                manager, path, MakeSuperblock(file_id, file_kinds[index], object_id, 9200 + index));
            EXPECT_TRUE(created);
        }

        auto opened = PageFile::Open(manager, path, file_id, file_kinds[index], object_id);
        EXPECT_TRUE(opened);
    }
}

TEST(PageFileTest, PreservesDiskCreationFailuresWithoutTruncatingOrConflicting) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto first_path = temporary_directory.File("existing.pages");
    const auto duplicate_path = temporary_directory.File("duplicate.pages");
    DiskManager manager;
    auto first = PageFile::Create(
        manager, first_path, MakeSuperblock(FileId{130}, FileKind::HEAP, 7130, 9130));
    if (!first.page_file.has_value()) {
        ADD_FAILURE() << "initial page file creation unexpectedly failed";
        return;
    }

    const auto duplicate = PageFile::Create(
        manager, duplicate_path, MakeSuperblock(FileId{130}, FileKind::FSM, 8130, 9131));
    EXPECT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error.code, PageFileErrorCode::DISK_ERROR);
    EXPECT_EQ(duplicate.error.disk_error.code, DiskErrorCode::DUPLICATE_FILE_ID);
    EXPECT_FALSE(std::filesystem::exists(duplicate_path));

    const auto existing = PageFile::Create(
        manager, first_path, MakeSuperblock(FileId{131}, FileKind::HEAP, 7131, 9131));
    EXPECT_FALSE(existing);
    EXPECT_EQ(existing.error.code, PageFileErrorCode::DISK_ERROR);
    EXPECT_EQ(existing.error.disk_error.code, DiskErrorCode::SYSTEM_ERROR);
    EXPECT_EQ(existing.error.disk_error.system_error, EEXIST);
    EXPECT_EQ(std::filesystem::file_size(first_path), PAGE_SIZE);
}

TEST(PageFileTest, LeavesFailedPostCreationFileButCleansRegistration) {
    TemporaryDirectory temporary_directory;
    ASSERT_TRUE(temporary_directory.valid());
    const auto path = temporary_directory.File("partial.pages");
    auto invalid = MakeSuperblock(FileId{132}, FileKind::HEAP, 7132, 9132);
    invalid.file_kind = static_cast<FileKind>(0);
    DiskManager manager;

    const auto created = PageFile::Create(manager, path, invalid);
    EXPECT_FALSE(created);
    EXPECT_EQ(created.error.code, PageFileErrorCode::SUPERBLOCK_ENCODING_FAILED);
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_EQ(std::filesystem::file_size(path), PAGE_SIZE);

    ASSERT_TRUE(manager.OpenFile(invalid.file_id, path));
    Page page_zero{PageId{.file_id = invalid.file_id, .page_no = 0}};
    ASSERT_TRUE(manager.ReadPage(page_zero.Id(), page_zero.Bytes()));
    EXPECT_TRUE(std::ranges::all_of(page_zero.Bytes(),
                                    [](std::byte value) { return value == std::byte{0}; }));
    EXPECT_TRUE(manager.CloseFile(invalid.file_id));
}

} // namespace
} // namespace dblusblus
