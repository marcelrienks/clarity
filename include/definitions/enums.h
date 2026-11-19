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
 * @brief Error severity levels
 */
enum class ErrorLevel
{
    WARNING = 0,
    ERROR = 1,
    CRITICAL = 2
};

/**
 * @brief Oil sensor types for animations and callbacks
 */
enum class OilSensorTypes
{
    Pressure,
    Temperature
};
