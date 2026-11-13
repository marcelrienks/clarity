#pragma once

/**
 * @file definitions/enums.h
 * @brief System-wide enums and fundamental type definitions
 *
 * @details This header contains all enumeration types used throughout the
 * Clarity automotive gauge system for categorizing and defining discrete
 * states and types.
 *
 * @note Part of the modularized types system - include via types.h for backward compatibility
 */

//=============================================================================
// ENUMS AND TYPE DEFINITIONS
// System-wide enums and fundamental type definitions
//=============================================================================

/**
 * @brief Priority levels for triggers and system events
 */
enum class Priority
{
    LOW_PRIORITY = 0,
    NORMAL = 1,
    IMPORTANT = 2,
    CRITICAL = 3
};

/**
 * @brief Types of triggers in the interrupt system
 */
enum class TriggerType
{
    PANEL, // Panel-related triggers
    STYLE, // Style and theme triggers
    SYSTEM // System-level triggers
};

/**
 * @brief Error severity levels
 */
enum class ErrorLevel
{
    WARNING = 0,
    ERROR = 1,
    CRITICAL = 2
};

/**
 * @brief Button press action types
 */
enum class ActionPress
{
    SHORT,
    LONG
};

/**
 * @brief Button action types for action handler
 */
enum class ButtonAction
{
    NONE,
    SHORT_PRESS,
    LONG_PRESS
};

/**
 * @brief UI state for controlling system responsiveness
 */
enum class UIState
{
    IDLE,   // UI is idle and can accept new events
    BUSY,   // UI is busy (animations, loading, etc.)
    LOADING // UI is in loading state
};

/**
 * @brief Key state for automotive key presence detection
 */
enum class KeyState
{
    Present,
    NotPresent,
    Inactive
};

/**
 * @brief Oil sensor types for animations and callbacks
 */
enum class OilSensorTypes
{
    Pressure,
    Temperature
};

/**
 * @brief Convert UIState enum to string for logging
 */
inline const char *UIStateToString(UIState state)
{
    switch (state)
    {
        case UIState::IDLE:
            return "IDLE";
        case UIState::BUSY:
            return "BUSY";
        case UIState::LOADING:
            return "LOADING";
        default:
            return "UNKNOWN";
    }
}