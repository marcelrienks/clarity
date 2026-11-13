#pragma once

/**
 * @file definitions/state_types.h
 * @brief Runtime state structures and behavior types
 *
 * @details This header contains data structures for system state tracking,
 * error management, triggers, and action handlers in the Clarity automotive
 * gauge system. Combines runtime state structures with behavior types for
 * cohesive event-driven architecture.
 *
 * @note Part of the modularized types system - include via types.h for backward compatibility
 */

#include "definitions/constants.h"
#include "definitions/enums.h"
#include <cstring>
#include <string>

// Forward declarations
class BaseSensor;

//=============================================================================
// RUNTIME STATE STRUCTURES
// System status, hardware state, and error tracking data
//=============================================================================

/**
 * @brief State-based Trigger for GPIO monitoring with dual functions
 */
struct Trigger
{
    const char *id;           ///< Static string identifier
    Priority priority;        ///< Processing priority (CRITICAL > IMPORTANT > NORMAL)
    TriggerType type;         ///< PANEL, STYLE, or FUNCTION
    void (*activateFunc)();   ///< Function called when trigger activates
    void (*deactivateFunc)(); ///< Function called when trigger deactivates
    class BaseSensor *sensor; ///< Associated sensor for state monitoring
    bool isActive;            ///< Current activation state

    // Execution methods as documented in interrupt-architecture.md
    void ExecuteActivate()
    {
        if (activateFunc)
        {
            activateFunc();
            isActive = true; // Only activation sets active flag
        }
    }

    void ExecuteDeactivate()
    {
        if (deactivateFunc)
        {
            deactivateFunc();
            isActive = false; // Clear active flag on deactivation
        }
    }
};

/// @struct ErrorInfo
/**
 * @brief Complete error information structure
 */
struct ErrorInfo
{
    ErrorLevel level;                                               ///< Severity level of the error
    const char *source;                                             ///< Component/manager that reported the error
    char message[data_constants::error_info::MAX_MESSAGE_LENGTH]{}; ///< Fixed-size error message buffer (optimized for
                                                                    ///< embedded)
    unsigned long timestamp;                                        ///< millis() timestamp when error occurred
    bool acknowledged;                                              ///< Whether user has acknowledged the error

    // Constructor to maintain compatibility with std::string API
    ErrorInfo() : level(ErrorLevel::WARNING), source(nullptr), timestamp(0), acknowledged(false)
    {
        message[0] = '\0';
    }

    // Helper method to set message safely
    void SetMessage(const std::string &msg)
    {
        strncpy(message, msg.c_str(), data_constants::error_info::MAX_MESSAGE_LENGTH - 1);
        message[data_constants::error_info::MAX_MESSAGE_LENGTH - 1] = '\0'; // Ensure null termination
    }
};

//=============================================================================
// BEHAVIOR TYPES
// Action handlers, triggers, and function wrappers with mutable state
//=============================================================================

/// @struct Action
/**
 * @brief Event-based Action for button processing
 */
struct Action
{
    const char *id;        ///< Static string identifier
    void (*executeFunc)(); ///< Function to execute on button press
    bool hasTriggered;     ///< Whether this action has been triggered
    ActionPress pressType; ///< SHORT or LONG press type

    // Execution method as documented in interrupt-architecture.md
    void Execute()
    {
        if (executeFunc && hasTriggered)
        {
            executeFunc();
            hasTriggered = false; // Clear trigger flag after execution
        }
    }
};