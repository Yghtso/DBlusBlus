#ifndef DBLUSBLUS_STORAGE_DISK_MANAGER_H_
#define DBLUSBLUS_STORAGE_DISK_MANAGER_H_

#include "common/types.h"
#include "storage/page/page_header.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

namespace dblusblus {

enum class DiskOperation : std::uint8_t {
    CREATE_FILE,
    OPEN_FILE,
    CLOSE_FILE,
    LOOKUP_FILE,
    FILE_SIZE,
    READ_PAGE,
    WRITE_PAGE,
    EXTEND_FILE,
    SYNC_FILE,
};

enum class DiskErrorCode : std::uint8_t {
    NONE,
    INVALID_FILE_IDENTIFIER,
    DUPLICATE_FILE_ID,
    FILE_NOT_REGISTERED,
    FILE_SIZE_NOT_PAGE_ALIGNED,
    OFFSET_OVERFLOW,
    PAGE_NOT_FOUND,
    SHORT_READ,
    SYSTEM_ERROR,
};

struct DiskError {
    DiskErrorCode code{DiskErrorCode::NONE};
    DiskOperation operation{DiskOperation::LOOKUP_FILE};
    FileId file_id{INVALID_FILE_ID};
    PageNo page_no{INVALID_PAGE_NO};
    std::filesystem::path path;
    int system_error{0};
    std::string message;
};

struct DiskStatus {
    bool success{false};
    DiskError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

template <typename Value>
struct DiskResult {
    bool success{false};
    Value value{};
    DiskError error;

    [[nodiscard]] explicit operator bool() const noexcept {
        return success;
    }
};

class DiskManager {
  public:
    DiskManager() = default;
    ~DiskManager();

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;
    DiskManager(DiskManager&&) = delete;
    DiskManager& operator=(DiskManager&&) = delete;

    [[nodiscard]] DiskStatus CreateFile(FileId file_id, const std::filesystem::path& path);
    [[nodiscard]] DiskStatus OpenFile(FileId file_id, const std::filesystem::path& path);
    [[nodiscard]] DiskStatus CloseFile(FileId file_id);

    [[nodiscard]] DiskResult<std::uint64_t> FileSize(FileId file_id) const;
    [[nodiscard]] DiskResult<PageNo> PageCount(FileId file_id) const;

    [[nodiscard]] DiskStatus ReadPage(PageId page_id, std::span<std::byte, PAGE_SIZE> output) const;
    [[nodiscard]] DiskStatus WritePage(PageId page_id,
                                       std::span<const std::byte, PAGE_SIZE> data) const;

    [[nodiscard]] DiskResult<PageNo> ExtendFile(FileId file_id);
    [[nodiscard]] DiskStatus SyncFile(FileId file_id) const;

  private:
    struct FileEntry {
        int descriptor{-1};
        std::filesystem::path path;

        FileEntry(int descriptor_value, std::filesystem::path file_path);
        ~FileEntry();

        FileEntry(const FileEntry&) = delete;
        FileEntry& operator=(const FileEntry&) = delete;
        FileEntry(FileEntry&& other) noexcept;
        FileEntry& operator=(FileEntry&& other) noexcept;
    };

    [[nodiscard]] const FileEntry* FindFile(FileId file_id) const noexcept;

    // File registration and closure are lifecycle operations and must not race active I/O.
    std::unordered_map<FileId, FileEntry> files_;
    std::mutex extension_mutex_;
};

} // namespace dblusblus

#endif // DBLUSBLUS_STORAGE_DISK_MANAGER_H_
