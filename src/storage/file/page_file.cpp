#include "storage/file/page_file.h"

#include "storage/page/page.h"

#include <cstdint>
#include <optional>
#include <utility>

namespace dblusblus {
namespace {

[[nodiscard]] PageFileError DiskFailure(PageFileOperation operation, DiskError disk_error) {
    return PageFileError{
        .code = PageFileErrorCode::DISK_ERROR,
        .operation = operation,
        .disk_error = std::move(disk_error),
    };
}

[[nodiscard]] PageFileResult Failure(PageFileError error) {
    return PageFileResult{.page_file = std::nullopt, .error = std::move(error)};
}

[[nodiscard]] PageAllocationResult AllocationFailure(PageFileError error) {
    return PageAllocationResult{.page_id = std::nullopt, .error = std::move(error)};
}

void CleanupRegistration(DiskManager& disk_manager, FileId file_id) {
    static_cast<void>(disk_manager.CloseFile(file_id));
}

} // namespace

PageFile::PageFile(DiskManager& disk_manager, FileSuperblock superblock) noexcept
    : disk_manager_(&disk_manager), superblock_(superblock) {}

PageFile::~PageFile() {
    Release();
}

PageFile::PageFile(PageFile&& other) noexcept
    : disk_manager_(std::exchange(other.disk_manager_, nullptr)), superblock_(other.superblock_) {}

PageFile& PageFile::operator=(PageFile&& other) noexcept {
    if (this != &other) {
        Release();
        disk_manager_ = std::exchange(other.disk_manager_, nullptr);
        superblock_ = other.superblock_;
    }
    return *this;
}

PageFileResult PageFile::Create(DiskManager& disk_manager,
                                const std::filesystem::path& path,
                                const FileSuperblock& superblock) {
    auto create = disk_manager.CreateFile(superblock.file_id, path);
    if (!create) {
        return Failure(DiskFailure(PageFileOperation::CREATE, std::move(create.error)));
    }

    auto page_count = disk_manager.PageCount(superblock.file_id);
    if (!page_count) {
        auto error = DiskFailure(PageFileOperation::CREATE, std::move(page_count.error));
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }
    if (page_count.value != 0) {
        PageFileError error{
            .code = PageFileErrorCode::UNEXPECTED_INITIAL_PAGE_COUNT,
            .operation = PageFileOperation::CREATE,
            .actual_page_count = page_count.value,
        };
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }

    auto extended = disk_manager.ExtendFile(superblock.file_id);
    if (!extended) {
        auto error = DiskFailure(PageFileOperation::CREATE, std::move(extended.error));
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }
    if (extended.value != 0) {
        PageFileError error{
            .code = PageFileErrorCode::UNEXPECTED_SUPERBLOCK_PAGE_NUMBER,
            .operation = PageFileOperation::CREATE,
            .actual_page_no = extended.value,
        };
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }

    Page superblock_page{PageId{.file_id = superblock.file_id, .page_no = 0}};
    if (!EncodeFileSuperblock(superblock_page.Bytes(), superblock)) {
        PageFileError error{
            .code = PageFileErrorCode::SUPERBLOCK_ENCODING_FAILED,
            .operation = PageFileOperation::CREATE,
        };
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }

    auto write = disk_manager.WritePage(superblock_page.Id(), superblock_page.Bytes());
    if (!write) {
        auto error = DiskFailure(PageFileOperation::CREATE, std::move(write.error));
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }

    auto sync = disk_manager.SyncFile(superblock.file_id);
    if (!sync) {
        auto error = DiskFailure(PageFileOperation::CREATE, std::move(sync.error));
        CleanupRegistration(disk_manager, superblock.file_id);
        return Failure(std::move(error));
    }

    PageFile page_file{disk_manager, superblock};
    return PageFileResult{
        .page_file = std::optional<PageFile>{std::move(page_file)},
        .error = {},
    };
}

PageFileResult PageFile::Open(DiskManager& disk_manager,
                              const std::filesystem::path& path,
                              FileId expected_file_id,
                              FileKind expected_file_kind,
                              std::optional<std::uint64_t> expected_object_id) {
    auto open = disk_manager.OpenFile(expected_file_id, path);
    if (!open) {
        return Failure(DiskFailure(PageFileOperation::OPEN, std::move(open.error)));
    }

    auto page_count = disk_manager.PageCount(expected_file_id);
    if (!page_count) {
        auto error = DiskFailure(PageFileOperation::OPEN, std::move(page_count.error));
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }
    if (page_count.value == 0) {
        PageFileError error{
            .code = PageFileErrorCode::MISSING_SUPERBLOCK,
            .operation = PageFileOperation::OPEN,
            .actual_page_count = 0,
        };
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }

    Page superblock_page{PageId{.file_id = expected_file_id, .page_no = 0}};
    auto read = disk_manager.ReadPage(superblock_page.Id(), superblock_page.Bytes());
    if (!read) {
        auto error = DiskFailure(PageFileOperation::OPEN, std::move(read.error));
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }

    auto decoded = DecodeFileSuperblock(superblock_page.Bytes());
    if (!decoded.superblock.has_value()) {
        PageFileError error{
            .code = PageFileErrorCode::SUPERBLOCK_INVALID,
            .operation = PageFileOperation::OPEN,
            .superblock_error = decoded.error,
        };
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }

    const FileSuperblock superblock = *decoded.superblock;
    if (superblock.file_id != expected_file_id) {
        PageFileError error{
            .code = PageFileErrorCode::FILE_ID_MISMATCH,
            .operation = PageFileOperation::OPEN,
            .expected_file_id = expected_file_id,
            .actual_file_id = superblock.file_id,
        };
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }
    if (superblock.file_kind != expected_file_kind) {
        PageFileError error{
            .code = PageFileErrorCode::FILE_KIND_MISMATCH,
            .operation = PageFileOperation::OPEN,
            .expected_file_kind = expected_file_kind,
            .actual_file_kind = superblock.file_kind,
        };
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }
    if (expected_object_id.has_value() && superblock.object_id != *expected_object_id) {
        PageFileError error{
            .code = PageFileErrorCode::OBJECT_ID_MISMATCH,
            .operation = PageFileOperation::OPEN,
            .expected_object_id = *expected_object_id,
            .actual_object_id = superblock.object_id,
        };
        CleanupRegistration(disk_manager, expected_file_id);
        return Failure(std::move(error));
    }

    PageFile page_file{disk_manager, superblock};
    return PageFileResult{
        .page_file = std::optional<PageFile>{std::move(page_file)},
        .error = {},
    };
}

const FileSuperblock& PageFile::Superblock() const noexcept {
    return superblock_;
}

PageAllocationResult PageFile::AllocatePage() {
    if (disk_manager_ == nullptr) {
        return AllocationFailure(PageFileError{
            .code = PageFileErrorCode::NOT_OPEN,
            .operation = PageFileOperation::ALLOCATE_PAGE,
        });
    }

    auto extended = disk_manager_->ExtendFile(superblock_.file_id);
    if (!extended) {
        return AllocationFailure(
            DiskFailure(PageFileOperation::ALLOCATE_PAGE, std::move(extended.error)));
    }
    if (extended.value == 0) {
        return AllocationFailure(PageFileError{
            .code = PageFileErrorCode::DATA_PAGE_ZERO_ALLOCATED,
            .operation = PageFileOperation::ALLOCATE_PAGE,
            .actual_page_no = extended.value,
        });
    }

    return PageAllocationResult{
        .page_id = PageId{.file_id = superblock_.file_id, .page_no = extended.value},
        .error = {},
    };
}

void PageFile::Release() noexcept {
    if (disk_manager_ != nullptr) {
        static_cast<void>(disk_manager_->CloseFile(superblock_.file_id));
        disk_manager_ = nullptr;
    }
}

} // namespace dblusblus
