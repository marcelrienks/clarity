#pragma once

#include <cstdint>

namespace clarity::core
{
/// Every screen the device can show
enum class PanelId : uint8_t
{
    Splash,
    Oil,
    Config,
    Error,
    Key,
    Lock
};

/// Colour themes. Error is applied only while the error panel is shown
enum class ThemeId : uint8_t
{
    Day,
    Night,
    Error
};

/// What the key panel shows
enum class KeyState : uint8_t
{
    Inactive,
    Present,
    NotPresent
};

/// Ordered by severity: a higher value is more severe
enum class ErrorLevel : uint8_t
{
    Warning,
    Error,
    Critical
};

/// A completed button press, delivered once
enum class ButtonEvent : uint8_t
{
    None,
    Short,
    Long
};

/// Machine-readable cause of a user-visible fault
enum class Fault : uint16_t
{
    NvsWriteFailed,
    SensorImplausible,
    PanelBuildFailed,
    DebugTest
};

constexpr const char *ToString(PanelId id)
{
    switch (id)
    {
    case PanelId::Splash:
        return "Splash";
    case PanelId::Oil:
        return "Oil";
    case PanelId::Config:
        return "Config";
    case PanelId::Error:
        return "Error";
    case PanelId::Key:
        return "Key";
    case PanelId::Lock:
        return "Lock";
    }
    return "Unknown";
}

constexpr const char *ToString(ThemeId id)
{
    switch (id)
    {
    case ThemeId::Day:
        return "Day";
    case ThemeId::Night:
        return "Night";
    case ThemeId::Error:
        return "Error";
    }
    return "Unknown";
}

constexpr const char *ToString(KeyState state)
{
    switch (state)
    {
    case KeyState::Inactive:
        return "Inactive";
    case KeyState::Present:
        return "Present";
    case KeyState::NotPresent:
        return "NotPresent";
    }
    return "Unknown";
}

constexpr const char *ToString(ErrorLevel level)
{
    switch (level)
    {
    case ErrorLevel::Warning:
        return "Warning";
    case ErrorLevel::Error:
        return "Error";
    case ErrorLevel::Critical:
        return "Critical";
    }
    return "Unknown";
}

constexpr const char *ToString(ButtonEvent event)
{
    switch (event)
    {
    case ButtonEvent::None:
        return "None";
    case ButtonEvent::Short:
        return "Short";
    case ButtonEvent::Long:
        return "Long";
    }
    return "Unknown";
}

constexpr const char *ToString(Fault fault)
{
    switch (fault)
    {
    case Fault::NvsWriteFailed:
        return "NvsWriteFailed";
    case Fault::SensorImplausible:
        return "SensorImplausible";
    case Fault::PanelBuildFailed:
        return "PanelBuildFailed";
    case Fault::DebugTest:
        return "DebugTest";
    }
    return "Unknown";
}
} // namespace clarity::core
