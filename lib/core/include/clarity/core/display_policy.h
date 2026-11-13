#pragma once

#include <cstdint>

#include "clarity/core/types.h"

namespace clarity::core
{
/// Debounced vehicle lines, sampled once per loop
struct VehicleInputs
{
    bool key_present;
    bool key_not_present;
    bool lock;
    bool lights;
};

/// A panel that takes over the screen while its condition holds
enum class OverridePanel : uint8_t
{
    None,
    Error,
    Key,
    Lock
};

struct DisplayRequest
{
    OverridePanel panel;
    KeyState key; ///< Only meaningful when panel is Key; Inactive otherwise
};

/// What the screen should show, by priority: errors, then key present, key not present, lock.
/// None means the user panel (the restoration panel) is shown.
constexpr DisplayRequest ResolveDisplay(const VehicleInputs &in, bool errors_active) noexcept
{
    if (errors_active)
    {
        return {OverridePanel::Error, KeyState::Inactive};
    }
    if (in.key_present)
    {
        return {OverridePanel::Key, KeyState::Present};
    }
    if (in.key_not_present)
    {
        return {OverridePanel::Key, KeyState::NotPresent};
    }
    if (in.lock)
    {
        return {OverridePanel::Lock, KeyState::Inactive};
    }
    return {OverridePanel::None, KeyState::Inactive};
}

/// The theme to apply: Error while the error panel is requested, otherwise Night while the dash-light
/// signal is on and Day while it is off. The signal is wired to the car's dash lighting, so the user's
/// saved theme plays no part (Q-05).
constexpr ThemeId EffectiveTheme(const DisplayRequest &request, const VehicleInputs &in) noexcept
{
    if (request.panel == OverridePanel::Error)
    {
        return ThemeId::Error;
    }
    return in.lights ? ThemeId::Night : ThemeId::Day;
}
} // namespace clarity::core
