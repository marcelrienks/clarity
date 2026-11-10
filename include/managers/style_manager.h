#pragma once

#include "definitions/configs.h"
#include "definitions/constants.h"
#include "definitions/styles.h"
#include "definitions/types.h"
#include "interfaces/i_config.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_style_manager.h"

#include <lvgl.h>
#include <memory>
#include <string>

#define MAIN_DEFAULT (static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT)
#define ITEMS_DEFAULT (static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_DEFAULT)
#define INDICATOR_DEFAULT (static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_DEFAULT)

/**
 * @class StyleManager
 * @brief Theme and LVGL style management service
 *
 * @details This service provides centralized theme management and LVGL style
 * allocation for the entire application. It implements efficient style sharing
 * to reduce memory usage and provides consistent theming across all components.
 *
 * @design_pattern Service with Factory Pattern - created by ManagerFactory
 * @theme_system Day/Night themes with customizable color schemes
 * @style_sharing Shared style objects reduce memory fragmentation
 * @memory_optimization Single style instances used by multiple components
 *
 * @supported_themes:
 * - Night Theme: Dark background with red accents (default)
 * - Day Theme: Light background with white accents
 * - Extensible: Easy to add new themes
 *
 * @color_categories:
 * - background: Screen/panel background color
 * - text: General text and labels
 * - primary: Primary accent color
 * - gauge_normal: Normal gauge/indicator color
 * - gauge_warning: Warning state color (orange)
 * - gauge_danger: Critical/danger state color (red)
 *
 * @shared_styles:
 * - gauge_main_style: Main gauge background and border
 * - gauge_indicator_style: Major tick marks and indicators
 * - gauge_items_style: Minor tick marks and scale items
 * - gauge_danger_section_style: Danger zone highlighting
 *
 * @style_lifecycle:
 * 1. Init(): Initialize with default theme
 * 2. SetTheme(): Change theme and refresh all styles
 * 3. ApplyThemeToScreen(): Apply theme to specific screens
 * 4. Get*Style(): Accessor methods for components
 *
 * @memory_efficiency:
 * - Shared style objects prevent duplication
 * - Lazy style initialization
 * - Automatic cleanup on theme changes
 *
 * @context This manager handles all theming and styling for the app.
 * Components get their styles from here to ensure consistency. The night
 * theme uses red accents while day theme uses white/neutral colors.
 */
class StyleManager : public IStyleManager, public IConfig
{
  public:
    // ========== Constructors and Destructor ==========
    explicit StyleManager(const char *theme);
    StyleManager(const StyleManager &) = delete;
    StyleManager &operator=(const StyleManager &) = delete;
    ~StyleManager() override;

    // ========== Static Methods ==========
    static StyleManager &Instance();

    // ========== Public Interface Methods ==========
    void InitializeStyles() override;

    void SetTheme(const char *theme) override;

    void ApplyThemeToScreen(lv_obj_t *screen) override;

    void ApplyCurrentTheme() override;

    const ThemeColors &GetColours(const std::string &theme) const;

    lv_style_t &GetBackgroundStyle() override
    {
        return background_style_;
    }

    lv_style_t &GetTextStyle() override
    {
        return text_style_;
    }

    lv_style_t &GetGaugeNormalStyle() override
    {
        return gauge_normal_style_;
    }

    lv_style_t &GetGaugeWarningStyle() override
    {
        return gauge_warning_style_;
    }

    lv_style_t &GetGaugeDangerStyle() override
    {
        return gauge_danger_style_;
    }

    lv_style_t &GetGaugeIndicatorStyle() override
    {
        return gauge_indicator_style_;
    }

    lv_style_t &GetGaugeItemsStyle() override
    {
        return gauge_items_style_;
    }

    lv_style_t &GetGaugeMainStyle() override
    {
        return gauge_main_style_;
    }

    lv_style_t &GetGaugeDangerSectionStyle() override
    {
        return gauge_danger_section_style_;
    }

    const std::string &GetCurrentTheme() const override;

    const ThemeColors &GetThemeColors() const override
    {
        return GetColours(GetCurrentTheme());
    }

    bool IsInitialized() const override
    {
        return initialized_;
    }

    void SwitchTheme(const char *theme_name);

    void SetConfigurationManager(IConfigurationManager *configuration_manager);

    void LoadConfiguration();

    // IConfig implementation (instance method for backward compatibility)
    void RegisterConfig(IConfigurationManager *configuration_manager) override;

    // Static schema registration for self-registering pattern
    static void RegisterConfigSchema(IConfigurationManager *configuration_manager);

    // Live config update methods
    void RefreshConfig();
    void RegisterConfigCallback();

    // ========== Configuration Constants ==========
    static constexpr const char *CONFIG_SECTION = config_constants::sections::STYLE_MANAGER;
    static constexpr const char *CONFIG_THEME = config_constants::keys::STYLE_MANAGER_THEME;
    static constexpr const char *CONFIG_BRIGHTNESS = config_constants::keys::STYLE_MANAGER_BRIGHTNESS;

  private:
    // ========== Private Methods ==========
    void ResetStyles();

    // ========== Configuration Items (inline definitions) ==========
    inline static config::ConfigItem theme_config{
        config_constants::items::THEME, ui_strings::config_labels::THEME, std::string(themes::DAY),
        config::ConfigMetadata(config_constants::options::THEMES, config::ConfigItemType::Selection)};
    inline static config::ConfigItem brightness_config{
        config_constants::items::BRIGHTNESS, ui_strings::config_labels::BRIGHTNESS,
        config_constants::defaults::DEFAULT_BRIGHTNESS,
        config::ConfigMetadata(config_constants::options::BRIGHTNESS_LEVELS, "%", config::ConfigItemType::Selection)};

    // ========== Private Data Members ==========
    std::string theme_ = themes::NIGHT;
    bool initialized_ = false;

    // Core Style Objects
    lv_style_t background_style_{};    // Style for backgrounds
    lv_style_t text_style_{};          // Style for general text
    lv_style_t gauge_normal_style_{};  // Style for gauges in normal range
    lv_style_t gauge_warning_style_{}; // Style for gauges in warning range
    lv_style_t gauge_danger_style_{};  // Style for gauges in danger range

    // Shared gauge component styles (replaces per-component allocation)
    lv_style_t gauge_indicator_style_{};      // Style for gauge major ticks
    lv_style_t gauge_items_style_{};          // Style for gauge minor ticks
    lv_style_t gauge_main_style_{};           // Style for gauge main part
    lv_style_t gauge_danger_section_style_{}; // Style for danger zone sections

    // Theme color references from styles.h
    const ThemeColors &day_theme_colours_ = theme_definitions::DAY_THEME;
    const ThemeColors &night_theme_colours_ = theme_definitions::NIGHT_THEME;
    const ThemeColors &error_theme_colours_ = theme_definitions::ERROR_THEME;

    // Direct preference reading support
    IConfigurationManager *configuration_manager_ = nullptr;
};