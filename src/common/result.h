#pragma once

#include "common/assert.h"

#include <concepts>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>

namespace dblusblus {

// Wraps an error value so it can be returned from a function whose return type is Result.
//
//     return Unexpected{HeapSlotEntryDecodeError::INVALID_STATE};
template <typename E>
class Unexpected {
  public:
    constexpr explicit Unexpected(E error) noexcept(std::is_nothrow_move_constructible_v<E>)
        : error_(std::move(error)) {}

    [[nodiscard]] constexpr const E& Error() const& noexcept {
        return error_;
    }
    [[nodiscard]] constexpr E&& Error() && noexcept {
        return std::move(error_);
    }

  private:
    E error_;
};

template <typename E>
Unexpected(E) -> Unexpected<E>;

// The outcome of an operation that either produces a T or fails with an E. It is the
// project's stand-in for C++23 std::expected and follows the project naming style.
//
// Success converts implicitly from a T; failure is returned as Unexpected{error}. Value() and
// Error() abort through DBLUS_CHECK when called on the wrong alternative: callers test Ok() (or
// the result itself) first.
template <typename T, typename E>
class [[nodiscard]] Result {
  public:
    using ValueType = T;
    using ErrorType = E;

    // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    constexpr Result(T value) noexcept(std::is_nothrow_move_constructible_v<T>)
        : storage_(std::in_place_index<0>, std::move(value)) {}

    template <typename Other>
        requires std::constructible_from<E, Other>
    // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    constexpr Result(Unexpected<Other> error)
        : storage_(std::in_place_index<1>, std::move(error).Error()) {}

    [[nodiscard]] constexpr bool Ok() const noexcept {
        return storage_.index() == 0;
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return Ok();
    }

    [[nodiscard]] constexpr const T& Value() const& noexcept {
        DBLUS_CHECK(Ok(), "Result::Value() called on an error result");
        return *std::get_if<0>(&storage_);
    }
    [[nodiscard]] constexpr T& Value() & noexcept {
        DBLUS_CHECK(Ok(), "Result::Value() called on an error result");
        return *std::get_if<0>(&storage_);
    }
    [[nodiscard]] constexpr T&& Value() && noexcept {
        DBLUS_CHECK(Ok(), "Result::Value() called on an error result");
        return std::move(*std::get_if<0>(&storage_));
    }

    [[nodiscard]] constexpr const E& Error() const& noexcept {
        DBLUS_CHECK(!Ok(), "Result::Error() called on a successful result");
        return *std::get_if<1>(&storage_);
    }
    [[nodiscard]] constexpr E&& Error() && noexcept {
        DBLUS_CHECK(!Ok(), "Result::Error() called on a successful result");
        return std::move(*std::get_if<1>(&storage_));
    }

    [[nodiscard]] constexpr const T& operator*() const& noexcept {
        return Value();
    }
    [[nodiscard]] constexpr T& operator*() & noexcept {
        return Value();
    }
    [[nodiscard]] constexpr T&& operator*() && noexcept {
        return std::move(*this).Value();
    }
    [[nodiscard]] constexpr const T* operator->() const noexcept {
        return &Value();
    }
    [[nodiscard]] constexpr T* operator->() noexcept {
        return &Value();
    }

    // Converts to std::optional, dropping the error.
    [[nodiscard]] constexpr std::optional<T> AsOptional() const& {
        return Ok() ? std::optional<T>{Value()} : std::nullopt;
    }

  private:
    std::variant<T, E> storage_;
};

// The outcome of an operation that produces nothing on success. A default-constructed Status is
// a success.
template <typename E>
class [[nodiscard]] Status {
  public:
    using ErrorType = E;

    constexpr Status() noexcept = default;

    template <typename Other>
        requires std::constructible_from<E, Other>
    // NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    constexpr Status(Unexpected<Other> error) : error_(std::move(error).Error()) {}

    [[nodiscard]] constexpr bool Ok() const noexcept {
        return !error_.has_value();
    }
    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return Ok();
    }

    [[nodiscard]] constexpr const E& Error() const& noexcept {
        DBLUS_CHECK(!Ok(), "Status::Error() called on a successful status");
        return *error_; // NOLINT(bugprone-unchecked-optional-access): checked above
    }
    [[nodiscard]] constexpr E&& Error() && noexcept {
        DBLUS_CHECK(!Ok(), "Status::Error() called on a successful status");
        return std::move(*error_); // NOLINT(bugprone-unchecked-optional-access): checked above
    }

  private:
    std::optional<E> error_;
};

} // namespace dblusblus
