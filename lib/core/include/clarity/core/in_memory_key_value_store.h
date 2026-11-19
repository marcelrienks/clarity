#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <variant>

#include "clarity/core/key_value_store.h"

namespace clarity::core
{
/// Heap-backed KeyValueStore for host tests. Mimics the NVS rules that matter to callers:
/// namespaces and keys over 15 characters are refused (D-05e), values are typed, and a Put of
/// another type replaces the old entry. SetFailWrites() simulates a failing flash.
class InMemoryKeyValueStore final : public KeyValueStore
{
  public:
    bool GetString(const char *ns, const char *key, char *out, size_t capacity) const override
    {
        const auto *v = Find<std::string>(ns, key);
        if (v == nullptr || out == nullptr || v->size() + 1 > capacity)
        {
            return false;
        }
        std::memcpy(out, v->c_str(), v->size() + 1);
        return true;
    }

    bool PutString(const char *ns, const char *key, const char *value) override
    {
        return Store(ns, key, std::string(value != nullptr ? value : ""));
    }

    bool GetInt32(const char *ns, const char *key, int32_t &out) const override
    {
        return Read(ns, key, out);
    }

    bool PutInt32(const char *ns, const char *key, int32_t value) override
    {
        return Store(ns, key, value);
    }

    bool GetBool(const char *ns, const char *key, bool &out) const override
    {
        return Read(ns, key, out);
    }

    bool PutBool(const char *ns, const char *key, bool value) override
    {
        return Store(ns, key, value);
    }

    bool GetFloat(const char *ns, const char *key, float &out) const override
    {
        return Read(ns, key, out);
    }

    bool PutFloat(const char *ns, const char *key, float value) override
    {
        return Store(ns, key, value);
    }

    bool Remove(const char *ns, const char *key) override
    {
        if (!Valid(ns, key) || fail_writes_)
        {
            return false;
        }
        return entries_.erase(Id(ns, key)) > 0;
    }

    /// While true every Put and Remove fails and changes nothing
    void SetFailWrites(bool fail) noexcept
    {
        fail_writes_ = fail;
    }

    /// Number of Put calls that stored a value
    size_t WriteCount() const noexcept
    {
        return writes_;
    }

    size_t Size() const noexcept
    {
        return entries_.size();
    }

  private:
    using Value = std::variant<std::string, int32_t, bool, float>;
    using Id = std::pair<std::string, std::string>;

    static bool Valid(const char *ns, const char *key) noexcept
    {
        return ns != nullptr && key != nullptr && FitsKeyLimit(ns) && FitsKeyLimit(key);
    }

    template <typename T> const T *Find(const char *ns, const char *key) const
    {
        if (!Valid(ns, key))
        {
            return nullptr;
        }
        const auto it = entries_.find(Id(ns, key));
        return it == entries_.end() ? nullptr : std::get_if<T>(&it->second);
    }

    template <typename T> bool Read(const char *ns, const char *key, T &out) const
    {
        const T *v = Find<T>(ns, key);
        if (v == nullptr)
        {
            return false;
        }
        out = *v;
        return true;
    }

    template <typename T> bool Store(const char *ns, const char *key, T value)
    {
        if (!Valid(ns, key) || fail_writes_)
        {
            return false;
        }
        entries_[Id(ns, key)] = Value(std::move(value));
        ++writes_;
        return true;
    }

    std::map<Id, Value> entries_;
    size_t writes_ = 0;
    bool fail_writes_ = false;
};
} // namespace clarity::core
