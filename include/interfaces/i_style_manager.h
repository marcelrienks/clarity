#pragma once

// System/Library Includes
#include <lvgl.h>

// Project Includes
#include "definitions/styles.h"
#include "definitions/types.h"

/**
 * @interface IStyleManager
 * @brief Interface for style management and theme operations
 *
 * @details This interface abstracts theme management functionality,
 * providing access to LVGL styles and theme operations. Implementations
 * should handle initialization, theme switching, and style application
 * to screen objects.
 *
 * @design_pattern Interface Segregation - Focused on style operations only
 * @testability Enables mocking for unit tests
 * @dependency_injection Replaces direct StyleManager singleton access
 */
class IStyleManager
{
  public:
    virtual ~IStyleManager() = default;

    // Core Functionality Methods

    virtual void InitializeStyles() = 0;

    virtual bool IsInitialized() const = 0;

    virtual void ApplyThemeToScreen(lv_obj_t *screen) = 0;

    virtual void SetTheme(const char *theme) = 0;

    virtual void ApplyCurrentTheme() = 0;

    // Style Accessor Methods

    virtual lv_style_t &GetBackgroundStyle() = 0;

    virtual lv_style_t &GetTextStyle() = 0;

    virtual lv_style_t &GetGaugeNormalStyle() = 0;

    virtual lv_style_t &GetGaugeWarningStyle() = 0;

    virtual lv_style_t &GetGaugeDangerStyle() = 0;

    virtual lv_style_t &GetGaugeIndicatorStyle() = 0;

    virtual lv_style_t &GetGaugeItemsStyle() = 0;

    virtual lv_style_t &GetGaugeMainStyle() = 0;

    virtual lv_style_t &GetGaugeDangerSectionStyle() = 0;

    // Theme Information Methods

    virtual const std::string &GetCurrentTheme() const = 0;

    // Color Access Methods

    virtual const ThemeColors &GetThemeColors() const = 0;
};