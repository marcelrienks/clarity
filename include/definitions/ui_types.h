#pragma once

/**
 * @file definitions/ui_types.h
 * @brief Component positioning, layout data, and user interface calculations
 *
 * @details This header contains data structures for UI component positioning
 * and layout management in the Clarity automotive gauge system's LVGL-based
 * user interface.
 *
 * @note Part of the modularized types system - include via types.h for backward compatibility
 */

// System/Library Includes
#include <lvgl.h>

#include <cstdint>

//=============================================================================
// UI DATA STRUCTURES
// Component positioning, layout data, and user interface calculations
//=============================================================================

/// @struct ComponentLocation
/**
 * @brief UI component positioning and sizing parameters
 *
 * @details Comprehensive structure for defining component placement
 * within LVGL screens. Supports both absolute positioning (x,y) and
 * relative alignment with offsets.
 *
 * @positioning_modes:
 * - Absolute: Use x,y coordinates directly
 * - Relative: Use align with optional x_offset, y_offset
 * - Sizing: width,height or LV_SIZE_CONTENT for auto-sizing
 *
 * @usage_examples:
 * - ComponentLocation(100, 50): Absolute position at (100,50)
 * - ComponentLocation(LV_ALIGN_CENTER): Centered with auto-size
 * - ComponentLocation(LV_ALIGN_LEFT_MID, 10, 0): Left-aligned with 10px offset
 */
struct ComponentLocation
{
    lv_coord_t x = 0;                   ///< Absolute X coordinate
    lv_coord_t y = 0;                   ///< Absolute Y coordinate
    lv_align_t align = LV_ALIGN_CENTER; ///< LVGL alignment mode
    lv_coord_t x_offset = 0;            ///< X offset from alignment point
    lv_coord_t y_offset = 0;            ///< Y offset from alignment point
    int32_t rotation = 0;               ///< Rotation angle in degrees (optional)

    ComponentLocation() = default;

    /// Constructor for absolute positioning
    ComponentLocation(lv_coord_t x_in, lv_coord_t y_in) : x(x_in), y(y_in)
    {
    }

    /// Constructor for relative alignment with optional offsets
    ComponentLocation(lv_align_t align_in, lv_coord_t x_offset_in = 0, lv_coord_t y_offset_in = 0)
        : align(align_in), x_offset(x_offset_in), y_offset(y_offset_in)
    {
    }

    /// Constructor for rotation start point of scales
    ComponentLocation(int32_t rotation_in) : rotation(rotation_in)
    {
    }
};