#pragma once

#include "common/result.h"

#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <string_view>

// Test accessors that record a failure instead of aborting when a value is missing. They return
// nullptr in that case, so callers write `if (value == nullptr) { return; }`.
namespace dblusblus::test {

template <typename T>
[[nodiscard]] const T* RequireValue(const std::optional<T>& optional,
                                    std::string_view description) {
    if (!optional.has_value()) {
        ADD_FAILURE() << description << " unexpectedly missing";
        return nullptr;
    }
    return std::addressof(*optional);
}

template <typename T>
[[nodiscard]] T* RequireValue(std::optional<T>& optional, std::string_view description) {
    if (!optional.has_value()) {
        ADD_FAILURE() << description << " unexpectedly missing";
        return nullptr;
    }
    return std::addressof(*optional);
}

template <typename T, typename E>
[[nodiscard]] const T* RequireValue(const Result<T, E>& result, std::string_view description) {
    if (!result.Ok()) {
        ADD_FAILURE() << description << " unexpectedly failed";
        return nullptr;
    }
    return std::addressof(result.Value());
}

template <typename T, typename E>
[[nodiscard]] T* RequireValue(Result<T, E>& result, std::string_view description) {
    if (!result.Ok()) {
        ADD_FAILURE() << description << " unexpectedly failed";
        return nullptr;
    }
    return std::addressof(result.Value());
}

// The error code of a result, or no value when it succeeded. For results whose success is a
// legitimate outcome of the test, where calling Error() directly would abort.
template <typename R>
[[nodiscard]] auto ErrorCodeOf(const R& result) -> std::optional<decltype(result.Error().code)> {
    if (result.Ok()) {
        return std::nullopt;
    }
    return result.Error().code;
}

// Deleted so a temporary result cannot hand out a dangling pointer.
template <typename T, typename E>
const T* RequireValue(const Result<T, E>&& result, std::string_view description) = delete;
template <typename T>
const T* RequireValue(const std::optional<T>&& optional, std::string_view description) = delete;

} // namespace dblusblus::test
