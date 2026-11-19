#pragma once

#include <cstddef>
#include <cstdint>

namespace clarity::core
{
/// Longest namespace or key the NVS backend accepts (NVS limit: 15 characters, not counting the terminator).
inline constexpr size_t KEY_MAX_LENGTH = 15;

/// True when a namespace or key is non-empty and fits the NVS limit. Usable in static_assert.
constexpr bool FitsKeyLimit(const char *s) noexcept
{
    size_t n = 0;
    while (s[n] != '\0')
    {
        ++n;
        if (n > KEY_MAX_LENGTH)
        {
            return false;
        }
    }
    return n > 0;
}

/// Typed, namespaced persistent storage. The only storage interface in core.
///
/// Entries are typed like NVS entries: reading a key as another type than it was written as fails.
/// Get* returns false (and leaves `out` untouched) when the key is missing, holds another type,
/// or the namespace or key is invalid. Put* returns false when the write was refused or failed.
/// Callers fall back to defaults on a failed read and report a failed write.
class KeyValueStore
{
  public:
    virtual ~KeyValueStore() = default;

    /// Copies the string into `out` (terminated). False when it does not fit in `capacity` bytes.
    virtual bool GetString(const char *ns, const char *key, char *out, size_t capacity) const = 0;
    virtual bool PutString(const char *ns, const char *key, const char *value) = 0;

    virtual bool GetInt32(const char *ns, const char *key, int32_t &out) const = 0;
    virtual bool PutInt32(const char *ns, const char *key, int32_t value) = 0;

    virtual bool GetBool(const char *ns, const char *key, bool &out) const = 0;
    virtual bool PutBool(const char *ns, const char *key, bool value) = 0;

    virtual bool GetFloat(const char *ns, const char *key, float &out) const = 0;
    virtual bool PutFloat(const char *ns, const char *key, float value) = 0;

    /// True when the key existed and was removed
    virtual bool Remove(const char *ns, const char *key) = 0;
};
} // namespace clarity::core
