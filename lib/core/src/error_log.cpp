#include "clarity/core/error_log.h"

#include <cstdarg>
#include <cstdio>

namespace clarity::core
{
namespace
{
const ErrorInfo EMPTY_INFO{};
} // namespace

bool ErrorLog::Report(uint32_t now, ErrorLevel level, Fault fault, const char *source, const char *fmt, ...)
{
    Expire(now);

    ErrorInfo incoming;
    incoming.level = level;
    incoming.fault = fault;
    incoming.source = source;
    incoming.timestamp = now;
    if (fmt != nullptr)
    {
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(incoming.message, ErrorInfo::MESSAGE_SIZE, fmt, args);
        va_end(args);
    }

    if (count_ == CAPACITY)
    {
        // Victim: lowest severity, then oldest. The first match in report order is the oldest of its level.
        size_t victim = 0;
        for (size_t i = 1; i < count_; ++i)
        {
            if (ring_[i].level < ring_[victim].level)
            {
                victim = i;
            }
        }
        if (incoming.level < ring_[victim].level)
        {
            return false;
        }
        RemoveAt(victim);
    }

    ring_[count_++] = incoming;
    ++revision_;
    return true;
}

const ErrorInfo &ErrorLog::At(size_t index) const noexcept
{
    return index < count_ ? ring_[index] : EMPTY_INFO;
}

void ErrorLog::DismissAt(size_t index) noexcept
{
    if (index >= count_)
    {
        return;
    }
    if (ring_[index].level == ErrorLevel::Critical)
    {
        ring_[index].acknowledged = true;
    }
    else
    {
        RemoveAt(index);
    }
    ++revision_;
}

void ErrorLog::Clear() noexcept
{
    if (count_ == 0)
    {
        return;
    }
    count_ = 0;
    ++revision_;
}

void ErrorLog::Expire(uint32_t now) noexcept
{
    bool removed = false;
    size_t i = 0;
    while (i < count_)
    {
        if (ring_[i].level == ErrorLevel::Warning &&
            static_cast<uint32_t>(now - ring_[i].timestamp) > WARNING_EXPIRY_MS)
        {
            RemoveAt(i);
            removed = true;
        }
        else
        {
            ++i;
        }
    }
    if (removed)
    {
        ++revision_;
    }
}

void ErrorLog::RemoveAt(size_t index) noexcept
{
    for (size_t i = index + 1; i < count_; ++i)
    {
        ring_[i - 1] = ring_[i];
    }
    --count_;
}
} // namespace clarity::core
