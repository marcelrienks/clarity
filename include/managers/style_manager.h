#pragma once

#include "interfaces/i_style_manager.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_config.h"
#include "definitions/styles.h"
#include "definitions/types.h"
#include "definitions/constants.h"
#include "definitions/configs.h"

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
    ~StyleManager();

    // ========== Static Methods ==========
    static StyleManager& Instance();

    // ========== Public Interface Methods ==========
    void InitializeStyles() override;

    void SetTheme(const char *theme) override;

    void ApplyThemeToScreen(lv_obj_t *screen) override;

    void ApplyCurrentTheme() override;

    const ThemeColors &GetColours(const std::string& theme) const;

    lv_style_t &GetBackgroundStyle() override
    {
        return backgroundStyle_;
    }

    lv_style_t &GetTextStyle() override
    {
        return textStyle_;
    }

    lv_style_t &GetGaugeNormalStyle() override
    {
        return gaugeNormalStyle_;
    }

    lv_style_t &GetGaugeWarningStyle() override
    {
        return gaugeWarningStyle_;
    }

    lv_style_t &GetGaugeDangerStyle() override
    {
        return gaugeDangerStyle_;
    }

    lv_style_t &GetGaugeIndicatorStyle() override
    {
        return gaugeIndicatorStyle_;
    }

    lv_style_t &GetGaugeItemsStyle() override
    {
        return gaugeItemsStyle_;
    }

    lv_style_t &GetGaugeMainStyle() override
    {
        return gaugeMainStyle_;
    }

    lv_style_t &GetGaugeDangerSectionStyle() override
    {
        return gaugeDangerSectionStyle_;
    }

    const std::string& GetCurrentTheme() const override;

    const ThemeColors &GetThemeColors() const override
    {
        return GetColours(GetCurrentTheme());
    }

    bool IsInitialized() const override
    {
        return initialized_;
    }

    void SwitchTheme(const char* themeName);

    void SetConfigurationManager(IConfigurationManager* configurationManager);

    void LoadConfiguration();

    // IConfig implementation (instance method for backward compatibility)
    void RegisterConfig(IConfigurationManager* configurationManager) override;

    // Static schema registration for self-registering pattern
    static void RegisterConfigSchema(IConfigurationManager* configurationManager);

    // Live config update methods
    void RefreshConfig();
    void RegisterConfigCallback();

    // ========== Configuration Constants ==========
    static constexpr const char* CONFIG_SECTION = ConfigConstants::Sections::STYLE_MANAGER;
    static constexpr const char* CONFIG_THEME = ConfigConstants::Keys::STYLE_MANAGER_THEME;
    static constexpr const char* CONFIG_BRIGHTNESS = ConfigConstants::Keys::STYLE_MANAGER_BRIGHTNESS;

private:
    // ========== Private Methods ==========
    void ResetStyles();

    // ========== Configuration Items (inline definitions) ==========
    inline static Config::ConfigItem themeConfig_{ConfigConstants::Items::THEME, UIStrings::ConfigLabels::THEME,
                                                   std::string(Themes::DAY),
                                                   Config::ConfigMetadata(ConfigConstants::Options::THEMES, Config::ConfigItemType::Selection)};
    inline static Config::ConfigItem brightnessConfig_{ConfigConstants::Items::BRIGHTNESS, UIStrings::ConfigLabels::BRIGHTNESS,
                                                        ConfigConstants::Defaults::DEFAULT_BRIGHTNESS,
                                                        Config::ConfigMetadata(ConfigConstants::Options::BRIGHTNESS_LEVELS, "%", Config::ConfigItemType::Selection)};

    // ========== Private Data Members ==========
    std::string theme_ = Themes::NIGHT;
    bool initialized_ = false;

    // Core Style Objects
    lv_style_t backgroundStyle_;   // Style for backgrounds
    lv_style_t textStyle_;         // Style for general text
    lv_style_t gaugeNormalStyle_;  // Style for gauges in normal range
    lv_style_t gaugeWarningStyle_; // Style for gauges in warning range
    lv_style_t gaugeDangerStyle_;  // Style for gauges in danger range

    // Shared gauge component styles (replaces per-component allocation)
    lv_style_t gaugeIndicatorStyle_;     // Style for gauge major ticks
    lv_style_t gaugeItemsStyle_;         // Style for gauge minor ticks
    lv_style_t gaugeMainStyle_;          // Style for gauge main part
    lv_style_t gaugeDangerSectionStyle_; // Style for danger zone sections

    // Theme color references from styles.h
    const ThemeColors& dayThemeColours_ = ThemeDefinitions::DAY_THEME;
    const ThemeColors& nightThemeColours_ = ThemeDefinitions::NIGHT_THEME;
    const ThemeColors& errorThemeColours_ = ThemeDefinitions::ERROR_THEME;

    // Direct preference reading support
    IConfigurationManager* configurationManager_ = nullptr;
};