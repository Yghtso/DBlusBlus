#include "common/assert.h"

#include <cstdio>
#include <cstdlib>

namespace dblusblus::detail {

void CheckFailed(const char* condition,
                 const char* message,
                 std::source_location location) noexcept {
    std::fprintf(stderr,
                 "DBlusBlus internal check failed: %s\n  %s\n  at %s:%u in %s\n",
                 condition,
                 message,
                 location.file_name(),
                 static_cast<unsigned>(location.line()),
                 location.function_name());
    std::fflush(stderr);
    std::abort();
}

} // namespace dblusblus::detail
