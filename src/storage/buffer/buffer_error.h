#pragma once

#include "common/result.h"
#include "common/types.h"
#include "storage/disk/disk_manager.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace dblusblus {

// BufferPool error categories (ARCHITECTURE.md §7.12.7). Categories that depend on the WAL
// (WAL_RESERVATION_OR_APPEND_FAILURE, WAL_DURABILITY_FAILURE, STORAGE_NONCONTINUABLE) and file
// formats the pool does not dispatch (UNSUPPORTED_FILE_FORMAT) are absent until those owners exist.
enum class BufferErrorCode : std::uint8_t {
    FILE_OR_PAGE_NOT_FOUND,
    RAW_IO_FAILURE,
    CORRUPT_PAGE,
    UNSUPPORTED_PAGE_FORMAT,
    NO_REPLACEABLE_FRAME,
    FILE_RETIRED_OR_CLOSING,
    PIN_COUNT_OVERFLOW,
    BUFFERPOOL_QUIESCING,
    INTERNAL_INVARIANT_FAILURE,
};

enum class BufferOperation : std::uint8_t {
    REGISTER_FILE,
    RETIRE_FILE,
    FETCH_PAGE,
    NEW_PAGE,
    FLUSH_PAGE,
    SHUTDOWN,
};

struct BufferError {
    BufferErrorCode code{BufferErrorCode::INTERNAL_INVARIANT_FAILURE};
    BufferOperation operation{BufferOperation::FETCH_PAGE};
    PageId page_id{};
    // Populated when the failure originated in the raw I/O layer.
    std::optional<DiskError> disk_error;
    std::string message;
};

using BufferStatus = Status<BufferError>;

template <typename T>
using BufferResult = Result<T, BufferError>;

} // namespace dblusblus
