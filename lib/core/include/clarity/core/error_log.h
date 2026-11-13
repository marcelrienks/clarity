#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "clarity/core/types.h"

#if defined(__GNUC__)
    #define CLARITY_PRINTF_LIKE(fmt_index, first_arg) __attribute__((format(printf, fmt_index, first_arg)))
#else
    #define CLARITY_PRINTF_LIKE(fmt_index, first_arg)
#endif

namespace clarity::core
{
struct ErrorInfo
{
    static constexpr size_t MESSAGE_SIZE = 128;

    ErrorLevel level = ErrorLevel::Warning;
    Fault fault = Fault::DebugTest;
    const char *source = "";         ///< String literal naming the reporter
    char message[MESSAGE_SIZE] = {}; ///< Formatted detail, always terminated
    uint32_t timestamp = 0;          ///< Caller's millisecond clock at report time
    bool acknowledged = false;       ///< Set by DismissAt() on a critical error, which stays listed
};

/// Bounded list of user-visible faults, in report order.
/// Time is passed in, so the log has no clock and is host-testable.
class ErrorLog
{
  public:
    static constexpr size_t CAPACITY = 10;
    static constexpr uint32_t WARNING_EXPIRY_MS = 10000;

    /// Adds an entry after expiring old warnings. When full, the least important entry is dropped:
    /// lowest severity, then oldest. The new entry takes part in that choice, so a warning is rejected
    /// by a log full of errors. Returns false in that case. printf-style detail; fmt may be null.
    bool Report(uint32_t now, ErrorLevel level, Fault fault, const char *source, const char *fmt = nullptr, ...)
        CLARITY_PRINTF_LIKE(6, 7);

    bool HasActive() const noexcept
    {
        return count_ > 0;
    }

    size_t Count() const noexcept
    {
        return count_;
    }

    /// An out-of-range index returns an empty entry
    const ErrorInfo &At(size_t index) const noexcept;

    /// The error panel's "viewed, next". Warnings and errors are removed; a critical error is marked
    /// acknowledged and stays until Clear(). Out-of-range indexes are ignored.
    void DismissAt(size_t index) noexcept;

    void Clear() noexcept;

    /// Removes warnings older than WARNING_EXPIRY_MS
    void Expire(uint32_t now) noexcept;

    /// Changes whenever the list contents change, so views redraw only when needed
    uint32_t Revision() const noexcept
    {
        return revision_;
    }

  private:
    void RemoveAt(size_t index) noexcept;

    std::array<ErrorInfo, CAPACITY> ring_{};
    uint8_t count_ = 0;
    uint32_t revision_ = 0;
};
} // namespace clarity::core
