#pragma once

/**
 * @file definitions/types.h
 * @brief Core types and aggregated header for type definitions
 *
 * @details This header contains core fundamental types and serves as an aggregated
 * include for the modularized type headers. For backward compatibility and ease of use,
 * including this file provides access to all type definitions.
 *
 * The type system has been organized into domain-specific headers:
 * - enums.h: System-wide enums and fundamental type definitions
 * - ui_types.h: Component positioning and layout data structures
 * - state_types.h: Runtime state structures and behavior types
 *
 * @note For new code, consider including only the specific type headers needed
 * to reduce compilation dependencies.
 * @note For compile-time constants and immutable data, see constants.h
 */

#include <cstdint>
#include <string>
#include <variant>

//=============================================================================
// CORE TYPES
// Fundamental data types for sensor readings and dynamic data exchange
//=============================================================================

/**
 * @typedef Reading
 * @brief Variant type for sensor readings supporting multiple data types
 *
 * @details This variant can hold different types of sensor data:
 * - std::monostate: Uninitialized/invalid reading
 * - int32_t: Integer values (pressure, temperature, etc.)
 * - double: Floating-point values (precise measurements)
 * - std::string: Text/status readings
 * - bool: Boolean states (switches, alarms)
 *
 * @usage_examples:
 * - Oil pressure: int32_t (0-10 Bar)
 * - Oil temperature: int32_t (0-120°C)
 * - Key status: bool (true/false)
 * - Error messages: std::string
 */
using Reading = std::variant<std::monostate, int32_t, double, std::string, bool>;

// Include all modularized type headers
#include "definitions/enums.h"
#include "definitions/state_types.h"
#include "definitions/ui_types.h"
