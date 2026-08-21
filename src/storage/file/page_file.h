#ifndef DBLUSBLUS_STORAGE_PAGE_FILE_H_
#define DBLUSBLUS_STORAGE_PAGE_FILE_H_

#include "common/types.h"
#include "storage/disk/disk_manager.h"
#include "storage/file/file_superblock.h"

#include <cstdint>
#include <filesystem>
#include <optional>

namespace dblusblus {

enum class PageFileOperation : std::uint8_t {
    CREATE,
    OPEN,
    ALLOCATE_PAGE,
};

enum class PageFileErrorCode : std::uint8_t {
    NONE,
    DISK_ERROR,
    SUPERBLOCK_ENCODING_FAILED,
    SUPERBLOCK_INVALID,
    FILE_ID_MISMATCH,
    FILE_KIND_MISMATCH,
    OBJECT_ID_MISMATCH,
    UNEXPECTED_INITIAL_PAGE_COUNT,
    MISSING_SUPERBLOCK,
    UNEXPECTED_SUPERBLOCK_PAGE_NUMBER,
    DATA_PAGE_ZERO_ALLOCATED,
    NOT_OPEN,
};

struct PageFileError {
    PageFileErrorCode code{PageFileErrorCode::NONE};
    PageFileOperation operation{PageFileOperation::OPEN};
    DiskError disk_error{};
    FileSuperblockDecodeError superblock_error{FileSuperblockDecodeError::NONE};
    FileId expected_file_id{INVALID_FILE_ID};
    FileId actual_file_id{INVALID_FILE_ID};
    FileKind expected_file_kind{FileKind::HEAP};
    FileKind actual_file_kind{FileKind::HEAP};
    std::uint64_t expected_object_id{0};
    std::uint64_t actual_object_id{0};
    PageNo actual_page_count{0};
    PageNo actual_page_no{INVALID_PAGE_NO};
};

class PageFile;
struct PageFileResult;
struct PageAllocationResult;

// Move-only RAII controller for one FileId registration held by a caller-owned DiskManager.
//
// PageFile does not own the DiskManager. The DiskManager passed to Create or Open must outlive the
// returned PageFile. While that PageFile is alive, external code must not close, unregister, or
// rebind its FileId; PageFile retains responsibility for that registration until destruction or a
// move.
//
// Destruction closes the managed FileId through DiskManager::CloseFile; it does not delete the
// underlying file. Moving transfers this cleanup responsibility. Move assignment first releases
// the destination's current registration, and the moved-from PageFile owns no registration, does
// nothing on destruction, and reports NOT_OPEN from AllocatePage.
class PageFile {
  public:
    ~PageFile();

    PageFile(const PageFile&) = delete;
    PageFile& operator=(const PageFile&) = delete;
    PageFile(PageFile&& other) noexcept;
    PageFile& operator=(PageFile&& other) noexcept;

    [[nodiscard]] static PageFileResult Create(DiskManager& disk_manager,
                                               const std::filesystem::path& path,
                                               const FileSuperblock& superblock);

    [[nodiscard]] static PageFileResult
    Open(DiskManager& disk_manager,
         const std::filesystem::path& path,
         FileId expected_file_id,
         FileKind expected_file_kind,
         std::optional<std::uint64_t> expected_object_id = std::nullopt);

    [[nodiscard]] const FileSuperblock& Superblock() const noexcept;
    [[nodiscard]] PageAllocationResult AllocatePage();

  private:
    PageFile(DiskManager& disk_manager, FileSuperblock superblock) noexcept;
    void Release() noexcept;

    // Non-owning; null only after move/release. See the class lifetime contract above.
    DiskManager* disk_manager_{nullptr};
    FileSuperblock superblock_{};
};

struct PageFileResult {
    std::optional<PageFile> page_file;
    PageFileError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return page_file.has_value();
    }
};

struct PageAllocationResult {
    std::optional<PageId> page_id;
    PageFileError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return page_id.has_value();
    }
};

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_PAGE_FILE_H_
