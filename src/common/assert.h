#pragma once

#include <source_location>

namespace dblusblus::detail {

// Reports a failed internal check to stderr and aborts. Never returns.
[[noreturn]] void
CheckFailed(const char* condition, const char* message, std::source_location location) noexcept;

} // namespace dblusblus::detail

// Internal invariants: conditions that are true unless DBlusBlus itself has a bug. They are not
// for I/O failures, corrupt persistent data, or bad caller input that can happen at runtime;
// those use explicit error results.
//
// DBLUS_CHECK is always on. Use it where continuing after a broken invariant could damage
// persistent data or hide the bug.
#define DBLUS_CHECK(condition, message)                                                            \
    ((condition) ? static_cast<void>(0)                                                            \
                 : ::dblusblus::detail::CheckFailed(                                               \
                       #condition, (message), std::source_location::current()))

// DBLUS_ASSERT is checked in debug builds only. In release builds the condition is not
// evaluated, but it still has to compile and its variables still count as used.
#ifdef NDEBUG
#define DBLUS_ASSERT(condition, message) static_cast<void>(sizeof(!(condition)))
#else
#define DBLUS_ASSERT(condition, message) DBLUS_CHECK(condition, message)
#endif
