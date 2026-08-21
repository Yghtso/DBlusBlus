#include "storage/disk/disk_manager.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utility>

namespace dblusblus {
namespace {

constexpr mode_t PAGE_FILE_MODE = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

[[nodiscard]] DiskError MakeError(DiskErrorCode code,
                                  DiskOperation operation,
                                  FileId file_id,
                                  const std::filesystem::path& path,
                                  std::string_view message,
                                  int system_error = 0,
                                  PageNo page_no = INVALID_PAGE_NO) {
    return DiskError{
        .code = code,
        .operation = operation,
        .file_id = file_id,
        .page_no = page_no,
        .path = path,
        .system_error = system_error,
        .message = std::string{message},
    };
}

[[nodiscard]] DiskStatus SuccessStatus() noexcept {
    return DiskStatus{.success = true, .error = {}};
}

[[nodiscard]] DiskStatus FailureStatus(DiskError error) {
    return DiskStatus{.success = false, .error = std::move(error)};
}

template <typename Value>
[[nodiscard]] DiskResult<Value> SuccessResult(Value value) noexcept {
    return DiskResult<Value>{.success = true, .value = value, .error = {}};
}

template <typename Value>
[[nodiscard]] DiskResult<Value> FailureResult(DiskError&& error) {
    return DiskResult<Value>{.success = false, .value = {}, .error = std::move(error)};
}

[[nodiscard]] int OpenWithRetry(const std::filesystem::path& path, int flags, mode_t mode) {
    int descriptor = -1;
    do {
        descriptor = ::open(path.c_str(), flags, mode);
    } while (descriptor < 0 && errno == EINTR);
    return descriptor;
}

[[nodiscard]] int FstatWithRetry(int descriptor, struct stat& status) noexcept {
    int result = -1;
    do {
        result = ::fstat(descriptor, &status);
    } while (result < 0 && errno == EINTR);
    return result;
}

[[nodiscard]] int FtruncateWithRetry(int descriptor, off_t size) noexcept {
    int result = -1;
    do {
        result = ::ftruncate(descriptor, size);
    } while (result < 0 && errno == EINTR);
    return result;
}

[[nodiscard]] int FdatasyncWithRetry(int descriptor) noexcept {
    int result = -1;
    do {
        result = ::fdatasync(descriptor);
    } while (result < 0 && errno == EINTR);
    return result;
}

[[nodiscard]] int CloseDescriptor(int descriptor) noexcept {
    if (::close(descriptor) == 0) {
        return 0;
    }
    return errno;
}

[[nodiscard]] DiskResult<off_t>
PageOffset(PageId page_id, const std::filesystem::path& path, DiskOperation operation) {
    constexpr auto max_offset = static_cast<std::uint64_t>(std::numeric_limits<off_t>::max());
    constexpr auto max_full_page_no = (max_offset / PAGE_SIZE) - 1U;
    if (page_id.page_no > max_full_page_no) {
        return FailureResult<off_t>(MakeError(DiskErrorCode::OFFSET_OVERFLOW,
                                              operation,
                                              page_id.file_id,
                                              path,
                                              "page offset exceeds POSIX off_t range",
                                              0,
                                              page_id.page_no));
    }
    return SuccessResult(static_cast<off_t>(page_id.page_no * PAGE_SIZE));
}

} // namespace

DiskManager::FileEntry::FileEntry(int descriptor_value, std::filesystem::path file_path)
    : descriptor(descriptor_value), path(std::move(file_path)) {}

DiskManager::FileEntry::~FileEntry() {
    if (descriptor >= 0) {
        static_cast<void>(CloseDescriptor(descriptor));
    }
}

DiskManager::FileEntry::FileEntry(FileEntry&& other) noexcept
    : descriptor(std::exchange(other.descriptor, -1)), path(std::move(other.path)) {}

DiskManager::FileEntry& DiskManager::FileEntry::operator=(FileEntry&& other) noexcept {
    if (this != &other) {
        if (descriptor >= 0) {
            static_cast<void>(CloseDescriptor(descriptor));
        }
        descriptor = std::exchange(other.descriptor, -1);
        path = std::move(other.path);
    }
    return *this;
}

DiskManager::~DiskManager() = default;

DiskStatus DiskManager::CreateFile(FileId file_id, const std::filesystem::path& path) {
    if (file_id == INVALID_FILE_ID) {
        return FailureStatus(MakeError(DiskErrorCode::INVALID_FILE_IDENTIFIER,
                                       DiskOperation::CREATE_FILE,
                                       file_id,
                                       path,
                                       "cannot register INVALID_FILE_ID"));
    }
    if (files_.contains(file_id)) {
        return FailureStatus(MakeError(DiskErrorCode::DUPLICATE_FILE_ID,
                                       DiskOperation::CREATE_FILE,
                                       file_id,
                                       path,
                                       "FileId is already registered"));
    }

    const int descriptor =
        OpenWithRetry(path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, PAGE_FILE_MODE);
    if (descriptor < 0) {
        const int system_error = errno;
        return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                       DiskOperation::CREATE_FILE,
                                       file_id,
                                       path,
                                       "open(O_CREAT|O_EXCL) failed",
                                       system_error));
    }

    files_.emplace(file_id, FileEntry{descriptor, path});
    return SuccessStatus();
}

DiskStatus DiskManager::OpenFile(FileId file_id, const std::filesystem::path& path) {
    if (file_id == INVALID_FILE_ID) {
        return FailureStatus(MakeError(DiskErrorCode::INVALID_FILE_IDENTIFIER,
                                       DiskOperation::OPEN_FILE,
                                       file_id,
                                       path,
                                       "cannot register INVALID_FILE_ID"));
    }
    if (files_.contains(file_id)) {
        return FailureStatus(MakeError(DiskErrorCode::DUPLICATE_FILE_ID,
                                       DiskOperation::OPEN_FILE,
                                       file_id,
                                       path,
                                       "FileId is already registered"));
    }

    const int descriptor = OpenWithRetry(path, O_RDWR | O_CLOEXEC, PAGE_FILE_MODE);
    if (descriptor < 0) {
        const int system_error = errno;
        return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                       DiskOperation::OPEN_FILE,
                                       file_id,
                                       path,
                                       "open existing file failed",
                                       system_error));
    }

    struct stat status{};
    if (FstatWithRetry(descriptor, status) < 0) {
        const int system_error = errno;
        static_cast<void>(CloseDescriptor(descriptor));
        return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                       DiskOperation::OPEN_FILE,
                                       file_id,
                                       path,
                                       "fstat failed while opening file",
                                       system_error));
    }
    if (status.st_size < 0 || status.st_size % static_cast<off_t>(PAGE_SIZE) != 0) {
        static_cast<void>(CloseDescriptor(descriptor));
        return FailureStatus(MakeError(DiskErrorCode::FILE_SIZE_NOT_PAGE_ALIGNED,
                                       DiskOperation::OPEN_FILE,
                                       file_id,
                                       path,
                                       "page file size is not a multiple of PAGE_SIZE"));
    }

    files_.emplace(file_id, FileEntry{descriptor, path});
    return SuccessStatus();
}

DiskStatus DiskManager::CloseFile(FileId file_id) {
    const auto iterator = files_.find(file_id);
    if (iterator == files_.end()) {
        return FailureStatus(MakeError(DiskErrorCode::FILE_NOT_REGISTERED,
                                       DiskOperation::CLOSE_FILE,
                                       file_id,
                                       {},
                                       "FileId is not registered"));
    }
    const int descriptor = iterator->second.descriptor;
    iterator->second.descriptor = -1;
    const int close_error = CloseDescriptor(descriptor);
    const auto path = iterator->second.path;
    files_.erase(iterator);
    if (close_error != 0) {
        return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                       DiskOperation::CLOSE_FILE,
                                       file_id,
                                       path,
                                       "close failed",
                                       close_error));
    }
    return SuccessStatus();
}

const DiskManager::FileEntry* DiskManager::FindFile(FileId file_id) const noexcept {
    const auto iterator = files_.find(file_id);
    return iterator == files_.end() ? nullptr : &iterator->second;
}

DiskResult<std::uint64_t> DiskManager::FileSize(FileId file_id) const {
    const FileEntry* file = FindFile(file_id);
    if (file == nullptr) {
        return FailureResult<std::uint64_t>(MakeError(DiskErrorCode::FILE_NOT_REGISTERED,
                                                      DiskOperation::FILE_SIZE,
                                                      file_id,
                                                      {},
                                                      "FileId is not registered"));
    }

    struct stat status{};
    if (FstatWithRetry(file->descriptor, status) < 0) {
        const int system_error = errno;
        return FailureResult<std::uint64_t>(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                                      DiskOperation::FILE_SIZE,
                                                      file_id,
                                                      file->path,
                                                      "fstat failed",
                                                      system_error));
    }
    if (status.st_size < 0 || status.st_size % static_cast<off_t>(PAGE_SIZE) != 0) {
        return FailureResult<std::uint64_t>(
            MakeError(DiskErrorCode::FILE_SIZE_NOT_PAGE_ALIGNED,
                      DiskOperation::FILE_SIZE,
                      file_id,
                      file->path,
                      "page file size is not a multiple of PAGE_SIZE"));
    }

    return SuccessResult(static_cast<std::uint64_t>(status.st_size));
}

DiskResult<PageNo> DiskManager::PageCount(FileId file_id) const {
    auto size = FileSize(file_id);
    if (!size) {
        return FailureResult<PageNo>(std::move(size.error));
    }
    return SuccessResult(static_cast<PageNo>(size.value / PAGE_SIZE));
}

DiskStatus DiskManager::ReadPage(PageId page_id, std::span<std::byte, PAGE_SIZE> output) const {
    const FileEntry* file = FindFile(page_id.file_id);
    if (file == nullptr) {
        return FailureStatus(MakeError(DiskErrorCode::FILE_NOT_REGISTERED,
                                       DiskOperation::READ_PAGE,
                                       page_id.file_id,
                                       {},
                                       "FileId is not registered",
                                       0,
                                       page_id.page_no));
    }
    const auto offset = PageOffset(page_id, file->path, DiskOperation::READ_PAGE);
    if (!offset) {
        return FailureStatus(offset.error);
    }

    std::array<std::byte, PAGE_SIZE> page{};
    std::size_t transferred = 0;
    while (transferred < PAGE_SIZE) {
        const auto result = ::pread(file->descriptor,
                                    page.data() + transferred,
                                    PAGE_SIZE - transferred,
                                    offset.value + static_cast<off_t>(transferred));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int system_error = errno;
            return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                           DiskOperation::READ_PAGE,
                                           page_id.file_id,
                                           file->path,
                                           "pread failed",
                                           system_error,
                                           page_id.page_no));
        }
        if (result == 0) {
            const auto code =
                transferred == 0 ? DiskErrorCode::PAGE_NOT_FOUND : DiskErrorCode::SHORT_READ;
            return FailureStatus(
                MakeError(code,
                          DiskOperation::READ_PAGE,
                          page_id.file_id,
                          file->path,
                          transferred == 0 ? "page does not exist" : "pread reached EOF mid-page",
                          0,
                          page_id.page_no));
        }
        transferred += static_cast<std::size_t>(result);
    }

    std::ranges::copy(page, output.begin());
    return SuccessStatus();
}

DiskStatus DiskManager::WritePage(PageId page_id,
                                  std::span<const std::byte, PAGE_SIZE> data) const {
    const FileEntry* file = FindFile(page_id.file_id);
    if (file == nullptr) {
        return FailureStatus(MakeError(DiskErrorCode::FILE_NOT_REGISTERED,
                                       DiskOperation::WRITE_PAGE,
                                       page_id.file_id,
                                       {},
                                       "FileId is not registered",
                                       0,
                                       page_id.page_no));
    }
    const auto offset = PageOffset(page_id, file->path, DiskOperation::WRITE_PAGE);
    if (!offset) {
        return FailureStatus(offset.error);
    }
    const auto page_count = PageCount(page_id.file_id);
    if (!page_count) {
        auto error = page_count.error;
        error.operation = DiskOperation::WRITE_PAGE;
        error.page_no = page_id.page_no;
        return FailureStatus(std::move(error));
    }
    if (page_id.page_no >= page_count.value) {
        return FailureStatus(MakeError(DiskErrorCode::PAGE_NOT_FOUND,
                                       DiskOperation::WRITE_PAGE,
                                       page_id.file_id,
                                       file->path,
                                       "page does not exist; extend the file first",
                                       0,
                                       page_id.page_no));
    }

    std::size_t transferred = 0;
    while (transferred < PAGE_SIZE) {
        const auto result = ::pwrite(file->descriptor,
                                     data.data() + transferred,
                                     PAGE_SIZE - transferred,
                                     offset.value + static_cast<off_t>(transferred));
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            const int system_error = errno;
            return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                           DiskOperation::WRITE_PAGE,
                                           page_id.file_id,
                                           file->path,
                                           "pwrite failed",
                                           system_error,
                                           page_id.page_no));
        }
        if (result == 0) {
            return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                           DiskOperation::WRITE_PAGE,
                                           page_id.file_id,
                                           file->path,
                                           "pwrite made no progress",
                                           EIO,
                                           page_id.page_no));
        }
        transferred += static_cast<std::size_t>(result);
    }
    return SuccessStatus();
}

DiskResult<PageNo> DiskManager::ExtendFile(FileId file_id) {
    const std::scoped_lock extension_lock{extension_mutex_};
    const FileEntry* file = FindFile(file_id);
    if (file == nullptr) {
        return FailureResult<PageNo>(MakeError(DiskErrorCode::FILE_NOT_REGISTERED,
                                               DiskOperation::EXTEND_FILE,
                                               file_id,
                                               {},
                                               "FileId is not registered"));
    }

    const auto page_count = PageCount(file_id);
    if (!page_count) {
        auto error = page_count.error;
        error.operation = DiskOperation::EXTEND_FILE;
        return FailureResult<PageNo>(std::move(error));
    }
    if (page_count.value >
        (static_cast<PageNo>(std::numeric_limits<off_t>::max()) / PAGE_SIZE) - 1U) {
        return FailureResult<PageNo>(MakeError(DiskErrorCode::OFFSET_OVERFLOW,
                                               DiskOperation::EXTEND_FILE,
                                               file_id,
                                               file->path,
                                               "extended file size exceeds POSIX off_t range"));
    }

    const auto new_size = static_cast<off_t>((page_count.value + 1U) * PAGE_SIZE);
    if (FtruncateWithRetry(file->descriptor, new_size) < 0) {
        const int system_error = errno;
        return FailureResult<PageNo>(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                               DiskOperation::EXTEND_FILE,
                                               file_id,
                                               file->path,
                                               "ftruncate failed",
                                               system_error));
    }
    return SuccessResult(page_count.value);
}

DiskStatus DiskManager::SyncFile(FileId file_id) const {
    const FileEntry* file = FindFile(file_id);
    if (file == nullptr) {
        return FailureStatus(MakeError(DiskErrorCode::FILE_NOT_REGISTERED,
                                       DiskOperation::SYNC_FILE,
                                       file_id,
                                       {},
                                       "FileId is not registered"));
    }
    if (FdatasyncWithRetry(file->descriptor) < 0) {
        const int system_error = errno;
        return FailureStatus(MakeError(DiskErrorCode::SYSTEM_ERROR,
                                       DiskOperation::SYNC_FILE,
                                       file_id,
                                       file->path,
                                       "fdatasync failed",
                                       system_error));
    }
    return SuccessStatus();
}

} // namespace dblusblus
