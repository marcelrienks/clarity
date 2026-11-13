#pragma once

/**
 * @file definitions/styles.h
 * @brief Centralized style utilities and color definitions for the Clarity automotive gauge system
 *
 * @details This header contains style-related utilities including:
 * - Theme color structure definitions
 * - Error level color mapping functions
 * - Centralized color constants and style helpers
 *
 * @organization:
 * - ThemeColors: Color scheme structure for different themes
 * - Style Utilities: Helper functions for color mapping and styling
 *
 * @note This file centralizes style utilities for consistent theming across components
 * @note For compile-time constants, see constants.h; for mutable types, see types.h
 */

// System/Library Includes
#include <lvgl.h>

// Project Includes
#include "definitions/constants.h"
#include "definitions/types.h"

//=============================================================================
// THEME COLOR DEFINITIONS
// Color schemes and theme-specific color structures
//=============================================================================

/**
 * @struct ThemeColors
 * @brief Color scheme structure for theme management
 *
 * @details Defines all colors used by a specific theme including background,
 * text, gauges, and status indicators. Used by StyleManager to provide
 * consistent theming across all components.
 *
 * @color_categories:
 * - background: Screen/panel background color
 * - text: General text and labels
 * - primary: Primary accent color
 * - gauge colors: Normal, warning, and danger state colors
 * - needle colors: Normal and danger needle colors
 * - key colors: Key presence status indicators
 */
struct ThemeColors
{
    lv_color_t background;    ///< Background color
    lv_color_t text;          ///< Text color
    lv_color_t primary;       ///< Primary accent color
    lv_color_t gaugeNormal;   ///< Normal gauge color
    lv_color_t gaugeWarning;  ///< Warning gauge color
    lv_color_t gaugeDanger;   ///< Danger gauge color
    lv_color_t gaugeTicks;    ///< Gauge tick marks (soft off-white)
    lv_color_t needleNormal;  ///< Normal needle color (bright white)
    lv_color_t needleDanger;  ///< Danger needle color (bright red/orange)
    lv_color_t keyPresent;    ///< Normal key present color (pure white)
    lv_color_t keyNotPresent; ///< Normal Key not present color (bright red)
    lv_color_t lockEngaged;   ///< Lock engaged color (red)
};

//=============================================================================
// PREDEFINED THEME COLOR SCHEMES
// Complete theme definitions for all supported visual themes
//=============================================================================

namespace theme_definitions
{
/**
 * @brief Day theme colors - Light background with white accents
 * @details Provides a bright, readable interface for daytime use
 */
inline const ThemeColors DAY_THEME = {
    .background = lv_color_hex(0x121212),    ///< Dark gray background for readability
    .text = lv_color_hex(0xEEEEEE),          ///< Light gray text
    .primary = lv_color_hex(0xEEEEEE),       ///< Light gray primary accent
    .gaugeNormal = lv_color_hex(0xEEEEEE),   ///< Light gray for normal gauges
    .gaugeWarning = lv_color_hex(0xFB8C00),  ///< Orange for warning states
    .gaugeDanger = lv_color_hex(0xB00020),   ///< Deep red for danger states
    .gaugeTicks = lv_color_hex(0xF0F0E8),    ///< Soft off-white for tick marks
    .needleNormal = lv_color_hex(0xFFFFFF),  ///< Pure white for normal needles
    .needleDanger = lv_color_hex(0xDC143C),  ///< Crimson red for danger needles
    .keyPresent = lv_color_hex(0x006400),    ///< Deep green for key present
    .keyNotPresent = lv_color_hex(0xDC143C), ///< Crimson red for key not present
    .lockEngaged = lv_color_hex(0xDC143C)    ///< Crimson red for lock engaged
};

/**
 * @brief Night theme colors - Black background with red accents
 * @details Optimized for nighttime driving with minimal eye strain
 */
inline const ThemeColors NIGHT_THEME = {
    .background = lv_color_hex(0x000000),    ///< Solid black background
    .text = lv_color_hex(0xB00020),          ///< Deep red text for night vision
    .primary = lv_color_hex(0xB00020),       ///< Deep red primary accent
    .gaugeNormal = lv_color_hex(0xB00020),   ///< Red icons for night mode
    .gaugeWarning = lv_color_hex(0xFB8C00),  ///< Orange for warning states
    .gaugeDanger = lv_color_hex(0xB00020),   ///< Deep red for danger states
    .gaugeTicks = lv_color_hex(0xB00020),    ///< Red tick marks for night mode
    .needleNormal = lv_color_hex(0xFFFFFF),  ///< Pure white for normal needles
    .needleDanger = lv_color_hex(0xDC143C),  ///< Crimson red for danger needles
    .keyPresent = lv_color_hex(0x006400),    ///< Deep green for key present
    .keyNotPresent = lv_color_hex(0xDC143C), ///< Crimson red for key not present
    .lockEngaged = lv_color_hex(0xDC143C)    ///< Crimson red for lock engaged
};

/**
 * @brief Error theme colors - High contrast for error display
 * @details Maximum readability and attention-grabbing colors for error states
 */
inline const ThemeColors ERROR_THEME = {
    .background = lv_color_hex(0x000000),    ///< Black background for high contrast
    .text = lv_color_hex(0xFFFFFF),          ///< White text for maximum readability
    .primary = lv_color_hex(0xFF0000),       ///< Bright red for error emphasis
    .gaugeNormal = lv_color_hex(0xFFFFFF),   ///< White for general elements
    .gaugeWarning = lv_color_hex(0xFFFF00),  ///< Yellow for warning states
    .gaugeDanger = lv_color_hex(0xFF0000),   ///< Red for critical errors
    .gaugeTicks = lv_color_hex(0xFFFFFF),    ///< White tick marks
    .needleNormal = lv_color_hex(0xFFFFFF),  ///< White needles
    .needleDanger = lv_color_hex(0xFF0000),  ///< Red for danger needles
    .keyPresent = lv_color_hex(0xFFFFFF),    ///< White (not used in error panel)
    .keyNotPresent = lv_color_hex(0xFFFFFF), ///< White (not used in error panel)
    .lockEngaged = lv_color_hex(0xFF0000)    ///< Bright red for lock engaged
};
} // namespace theme_definitions

//=============================================================================
// STYLE UTILITY FUNCTIONS
// Helper functions for color mapping and style operations
//=============================================================================

namespace style_utils
{
inline lv_color_t GetErrorColor(ErrorLevel level)
{
    switch (level)
    {
        case ErrorLevel::CRITICAL:
            return lv_color_hex(0xFF0000); // Red
        case ErrorLevel::ERROR:
            return lv_color_hex(0xFFFF00); // Yellow
        default:                           // WARNING and anything unknown
            return lv_color_hex(0xFFFFFF); // White
    }
}
} // namespace style_utils

//=============================================================================
// COLOR CONSTANTS
// Static color values for UI components and themes
//=============================================================================

namespace colors
{
// Night theme colors
static constexpr uint32_t NIGHT_BACKGROUND = 0x1A0000;      // Very dark red
static constexpr uint32_t NIGHT_TITLE_TEXT = 0xFF6666;      // Light red
static constexpr uint32_t NIGHT_HINT_TEXT = 0x993333;       // Darker red
static constexpr uint32_t NIGHT_SELECTED_BG = 0x4D1F1F;     // Dark red background
static constexpr uint32_t NIGHT_SELECTED_BORDER = 0x993333; // Red border
static constexpr uint32_t NIGHT_SELECTED_ITEM = 0xFF0000;   // Bright red
static constexpr uint32_t NIGHT_BASE_COLOR = 0xB00020;      // Deep red base

// Day theme colors
static constexpr uint32_t DAY_TITLE_TEXT = 0xCCCCCC;      // Light gray
static constexpr uint32_t DAY_HINT_TEXT = 0x888888;       // Gray
static constexpr uint32_t DAY_SELECTED_BG = 0x555555;     // Dark gray background
static constexpr uint32_t DAY_SELECTED_BORDER = 0x888888; // Gray border
static constexpr uint32_t DAY_SELECTED_ITEM = 0xFFFFFF;   // White
static constexpr uint32_t DAY_BASE_COLOR = 0xEEEEEE;      // Light gray base
static constexpr uint32_t DAY_FALLBACK = 0x888888;        // Gray fallback

// Common colors
static constexpr uint32_t WHITE = 0xFFFFFF; // Pure white
static constexpr uint32_t BLACK = 0x000000; // Pure black

// Component-specific colors
static constexpr uint32_t PIVOT_CIRCLE_CENTER = 0x505050; // Medium gray for pivot center
static constexpr uint32_t PIVOT_CIRCLE_EDGE = 0x2A2A2A;   // Dark gray for pivot edge
static constexpr uint32_t PIVOT_CIRCLE_BORDER = 0x1A1A1A; // Very dark border
static constexpr uint32_t PIVOT_CIRCLE_SHADOW = 0x000000; // Black shadow
static constexpr uint32_t NEEDLE_HIGHLIGHT = 0xFFFFFF;    // Pure white needle highlight
static constexpr uint32_t PIVOT_HIGHLIGHT = 0x707070;     // Light gray pivot highlight
} // namespace colors