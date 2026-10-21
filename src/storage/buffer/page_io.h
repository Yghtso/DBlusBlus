#pragma once

#include "common/types.h"
#include "storage/disk/disk_manager.h"
#include "storage/page/page_header.h"

#include <cstddef>
#include <span>

namespace dblusblus {

// The raw page-file operations BufferPool needs. The production implementation forwards to
// DiskManager; tests substitute a wrapper that injects transfer and synchronization faults.
// Implementations must allow concurrent calls for different pages.
class PageIo {
  public:
    PageIo() = default;
    virtual ~PageIo() = default;

    PageIo(const PageIo&) = delete;
    PageIo& operator=(const PageIo&) = delete;
    PageIo(PageIo&&) = delete;
    PageIo& operator=(PageIo&&) = delete;

    [[nodiscard]] virtual DiskStatus ReadPage(PageId page_id,
                                              std::span<std::byte, PAGE_SIZE> output) = 0;
    [[nodiscard]] virtual DiskStatus WritePage(PageId page_id,
                                               std::span<const std::byte, PAGE_SIZE> data) = 0;
    [[nodiscard]] virtual DiskStatus SyncFile(FileId file_id) = 0;
    [[nodiscard]] virtual DiskResult<PageNo> PageCount(FileId file_id) = 0;
    [[nodiscard]] virtual DiskResult<PageNo> ExtendFile(FileId file_id) = 0;
};

class DiskPageIo final : public PageIo {
  public:
    // Non-owning; the DiskManager must outlive this object.
    explicit DiskPageIo(DiskManager& disk_manager) noexcept : disk_manager_(&disk_manager) {}

    [[nodiscard]] DiskStatus ReadPage(PageId page_id,
                                      std::span<std::byte, PAGE_SIZE> output) override {
        return disk_manager_->ReadPage(page_id, output);
    }
    [[nodiscard]] DiskStatus WritePage(PageId page_id,
                                       std::span<const std::byte, PAGE_SIZE> data) override {
        return disk_manager_->WritePage(page_id, data);
    }
    [[nodiscard]] DiskStatus SyncFile(FileId file_id) override {
        return disk_manager_->SyncFile(file_id);
    }
    [[nodiscard]] DiskResult<PageNo> PageCount(FileId file_id) override {
        return disk_manager_->PageCount(file_id);
    }
    [[nodiscard]] DiskResult<PageNo> ExtendFile(FileId file_id) override {
        return disk_manager_->ExtendFile(file_id);
    }

  private:
    DiskManager* disk_manager_;
};

} // namespace dblusblus
