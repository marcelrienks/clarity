#pragma once

/**
 * @file definitions/state_types.h
 * @brief Runtime state structures
 *
 * @details This header contains data structures for system state tracking
 * and error management in the Clarity automotive gauge system.
 *
 * @note Part of the modularized types system - include via types.h for backward compatibility
 */

#include "definitions/constants.h"
#include "definitions/enums.h"
#include <cstring>
#include <string>

//=============================================================================
// RUNTIME STATE STRUCTURES
// System status, hardware state, and error tracking data
//=============================================================================

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
