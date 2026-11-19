#pragma once

/**
 * @file logging.h
 * @brief Extended logging utilities for the Clarity automotive gauge system
 *
 * @details This header provides additional logging macros that extend the
 * ESP32's built-in logging system (log_v, log_d, log_i, log_w, log_e).
 *
 * Additional log levels:
 * - log_t: Test logs with [T] prefix - independent of CORE_DEBUG_LEVEL
 *
 * Test Logging Concept:
 * - log_t() outputs directly to Serial with [T] prefix
 * - Controlled by TEST_LOGS flag, not CORE_DEBUG_LEVEL
 * - Allows CORE_DEBUG_LEVEL=0 while still showing test logs
 *
 * Build Configurations:
 * - debug-local: CORE_DEBUG_LEVEL=5 + TEST_LOGS (full logging)
 * - test-wokwi: CORE_DEBUG_LEVEL=0 + TEST_LOGS (test logs only)
 * - release: CORE_DEBUG_LEVEL=0 (no logs)
 *
 * @usage:
 * log_t("Panel loaded successfully");
 * log_t("Operation completed in %d ms", duration);
 *
 * @note Follows project logging standards from docs/patterns.md
 */

// Include ESP32 logging system
#include <esp32-hal-log.h>

//=============================================================================
// TEST LOGGING
// Direct serial output with [T] prefix, independent of CORE_DEBUG_LEVEL
//=============================================================================

/**
 * @brief Test log with [T] prefix - bypasses ESP32 log levels
 *
 * @details Direct serial output for test automation and debugging.
 * Unlike standard ESP32 log functions, log_t() is controlled by the
 * TEST_LOGS flag and outputs directly to Serial, bypassing CORE_DEBUG_LEVEL.
 *
 * This allows:
 * - CORE_DEBUG_LEVEL=0 with TEST_LOGS for clean test output
 * - CORE_DEBUG_LEVEL=5 with TEST_LOGS for full debug + test logs
 * - CORE_DEBUG_LEVEL=0 without TEST_LOGS for silent release
 *
 * Features duplicate suppression:
 * - Suppresses duplicate messages silently
 * - Every 25th duplicate is logged with count
 * - Shows final count when a new message arrives
 * - Resets when a new message is encountered
 *
 * @usage_examples:
 * - State changes: log_t("Sensor state changed: OFF -> ON");
 * - Panel loading: log_t("KeyPanel loaded successfully");
 * - Action execution: log_t("HandleShortPress() called");
 * - Timing: log_t("Operation completed in %d ms", duration);
 *
 * @note Output format: [T] message
 * @note Controlled by TEST_LOGS flag, not CORE_DEBUG_LEVEL
 * @note Essential for Wokwi test automation
 */
#ifdef TEST_LOGS
// Function declaration for the implementation
void log_t_impl(const char *format, ...);
    // Test logs enabled - uses implementation with duplicate suppression
    #define log_t(format, ...) log_t_impl(format, ##__VA_ARGS__)
#else
    // Test logs disabled - no output
    #define log_t(format, ...) ((void)0)
#endif

// Note: Standard ESP32 log functions (log_e, log_w, log_i, log_d, log_v)
// are controlled by CORE_DEBUG_LEVEL and remain unchanged.
// Only log_t() is controlled by the TEST_LOGS flag.

//=============================================================================
// USAGE EXAMPLES
//=============================================================================

/*
 * Basic usage:
 *   log_t("Panel loaded successfully");
 *   log_t("Operation completed in %d ms", duration);
 *   log_t("Sensor state changed: %s -> %s", old_state, newState);
 *
 * Output format (with TEST_LOGS enabled):
 *   [T] Panel loaded successfully
 *   [T] Operation completed in 150 ms
 *   [T] Sensor state changed: OFF -> ON
 *
 * Output format (with TEST_LOGS disabled):
 *   (no output)
 *
 * With timestamp option enabled:
 *   [   5432][T] Panel loaded successfully
 *   [   5582][T] Operation completed in 150 ms
 *
 * Build configurations:
 *   debug-local: CORE_DEBUG_LEVEL=5, TEST_LOGS=1 (all logs)
 *   test-wokwi:  CORE_DEBUG_LEVEL=0, TEST_LOGS=1 (test logs only)
 *   release:     CORE_DEBUG_LEVEL=0 (no logs)
 */