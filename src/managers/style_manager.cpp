#include "managers/style_manager.h"
#include "definitions/constants.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <cstring>
#include <esp32-hal-log.h>

// Self-registration at program startup
static bool style_manager_registered = []()
{
    ConfigurationManager::AddSchema(StyleManager::RegisterConfigSchema);
    return true;
}();

// Static instance for singleton pattern
static StyleManager *style_instance_ptr = nullptr;

// ========== Constructors and Destructor ==========

/**
 * @brief Construct StyleManager with specified initial theme
 * @details Sets up singleton pointer but does not initialize LVGL styles yet
 */
StyleManager::StyleManager(const char *theme) : theme_(theme), base_theme_(theme), initialized_(false)
{
    log_v("StyleManager() constructor called");

    // Set singleton instance for interrupt architecture
    style_instance_ptr = this;
}

/**
 * @brief Destructor - clean up LVGL styles and singleton pointer
 */
StyleManager::~StyleManager()
{
    log_v("~StyleManager() destructor called");
    ResetStyles();

    // Clear singleton instance if we are the current instance
    if (style_instance_ptr == this)
    {
        style_instance_ptr = nullptr;
    }
}

// ========== Static Methods ==========

/**
 * @brief Singleton access for interrupt architecture
 * @details Must be initialized by ManagerFactory before use
 */
StyleManager &StyleManager::Instance()
{
    if (!style_instance_ptr)
    {
        log_e("StyleManager::Instance() called before initialization");
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "Instance() called before initialization - UI will not function");
        // In embedded systems, we need a valid instance
        // This should be set during ManagerFactory initialization
    }
    return *style_instance_ptr;
}

// ========== Public Interface Methods ==========

/**
 * @brief Initialize all LVGL style objects and apply theme
 * @details Must be called after LVGL initialization, idempotent
 */
void StyleManager::InitializeStyles()
{
    log_v("InitializeStyles() called");
    if (initialized_)
    {
        return; // Already initialized - idempotent operation
    }

    // Initialize LVGL style objects (must be done after LVGL init)
    lv_style_init(&background_style_);
    lv_style_init(&text_style_);
    lv_style_init(&gauge_normal_style_);
    lv_style_init(&gauge_warning_style_);
    lv_style_init(&gauge_danger_style_);

    // Initialize shared gauge component styles for memory efficiency
    lv_style_init(&gauge_indicator_style_);
    lv_style_init(&gauge_items_style_);
    lv_style_init(&gauge_main_style_);
    lv_style_init(&gauge_danger_section_style_);

    // Mark as initialized before applying theme (since SetTheme checks this flag)
    initialized_ = true;

    if (configuration_manager_)
    {
        RegisterConfig(configuration_manager_);
    }

    // Copy first: SetTheme() reassigns theme_
    const std::string initial_theme = theme_;
    SetTheme(initial_theme.c_str());
}

/**
 * @brief Apply a specified theme to the styles
 * @param theme the theme to be applied
 */
void StyleManager::SetTheme(const char *theme)
{
    log_t("SetTheme() called");
    // Handle invalid theme gracefully
    if (!theme || strlen(theme) == 0)
    {
        return;
    }
    // Track the applied theme so ApplyCurrentTheme() compares against what is on screen.
    // The error theme is temporary: base_theme_ keeps the theme the lights signal asked for.
    theme_ = theme;
    if (strcmp(theme, themes::ERROR) != 0)
    {
        base_theme_ = theme;
    }
    log_t("StyleManager: Theme applied: %s", theme);

    // Select the current theme colors
    ThemeColors colours = GetColours(theme);

    // Background style
    lv_style_set_bg_color(&background_style_, colours.background);
    lv_style_set_bg_opa(&background_style_, LV_OPA_COVER);

    // Text style
    lv_style_set_text_color(&text_style_, colours.text);
    lv_style_set_text_opa(&text_style_, LV_OPA_COVER);

    // Gauge styles
    lv_style_set_line_color(&gauge_normal_style_, colours.gaugeNormal);
    lv_style_set_line_color(&gauge_warning_style_, colours.gaugeWarning);
    lv_style_set_line_color(&gauge_danger_style_, colours.gaugeDanger);

    // Configure shared gauge component styles
    // Indicator style (Major ticks)
    lv_style_set_length(&gauge_indicator_style_, 25);
    lv_style_set_line_width(&gauge_indicator_style_, 7);
    lv_style_set_line_color(&gauge_indicator_style_, colours.gaugeTicks);

    // Items style (Minor ticks)
    lv_style_set_length(&gauge_items_style_, 18);
    lv_style_set_line_width(&gauge_items_style_, 2);
    lv_style_set_line_color(&gauge_items_style_, colours.gaugeTicks);

    // Main style (Arc)
    lv_style_set_arc_width(&gauge_main_style_, 0);

    // Danger section style
    lv_style_set_line_width(&gauge_danger_section_style_, 5);
    lv_style_set_line_color(&gauge_danger_section_style_, colours.gaugeDanger);

    lv_obj_t *current_screen = lv_scr_act();
    if (current_screen != nullptr)
    {
        ApplyThemeToScreen(current_screen);
        // Force complete refresh of all child objects recursively
        lv_obj_refresh_style(current_screen, LV_PART_ANY, LV_STYLE_PROP_ANY);
        // Force LVGL to refresh the display
        lv_obj_invalidate(current_screen);
    }
}

/**
 * @brief Apply the current theme to a specific screen
 * @param screen the screen to which the theme will be applied
 */
void StyleManager::ApplyThemeToScreen(lv_obj_t *screen)
{
    log_v("ApplyThemeToScreen() called");

    // Safety checks
    if (!screen)
    {
        log_e("Cannot apply theme to null screen. Display will not function!");
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "Cannot apply theme to null screen - display will not function");
        return;
    }

    if (!initialized_)
    {
        log_e("StyleManager not initialized, cannot apply theme to screen. Application will not display correctly!");
        ErrorManager::Instance().ReportCriticalError("StyleManager", "Not initialized - cannot apply theme to screen");
        return;
    }

    // Only apply the background style to screens
    // Other styles should be applied to specific components that need them
    lv_obj_add_style(screen, &background_style_, MAIN_DEFAULT);

    // Don't apply all styles to the same screen - this can cause conflicts
    // Components should apply their own specific styles as needed
}

/**
 * @brief Re-apply the theme the dash-light signal asked for
 * @details The theme is driven only by the lights signal (Day or Night), with the error panel temporarily
 * overriding it. This puts the signal's theme back once the error panel is gone.
 */
void StyleManager::ApplyCurrentTheme()
{
    log_i("StyleManager::ApplyCurrentTheme() called");

    if (theme_ != base_theme_)
    {
        log_t("StyleManager: Theme changed from %s to %s - applying new theme", theme_.c_str(), base_theme_.c_str());
        // Copy first: SetTheme() reassigns base_theme_
        const std::string target = base_theme_;
        SetTheme(target.c_str());
    }
    else
    {
        log_v("Theme unchanged (%s) - no update needed", theme_.c_str());
    }
}

/**
 * @brief Get the colours scheme for the supplied theme
 * @param theme the theme to retrieve the colour scheme for
 * @return the colour scheme for the specified theme
 */
/**
 * @brief Resolve theme name to corresponding color scheme
 * @details Returns day theme as fallback for unknown themes
 */
const ThemeColors &StyleManager::GetColours(const std::string &theme) const
{
    log_v("GetColours() called");
    if (theme.empty())
    {
        return day_theme_colours_; // Default fallback
    }

    // Map theme names to color schemes
    if (theme == themes::NIGHT)
    {
        return night_theme_colours_;
    }
    if (theme == themes::ERROR)
    {
        return error_theme_colours_;
    }

    return day_theme_colours_; // Fallback for unknown themes
}

/**
 * @brief Get the theme that is applied now
 */
const std::string &StyleManager::GetCurrentTheme() const
{
    return theme_;
}

/**
 * @brief Inject preference service dependency for theme persistence
 * @param configurationManager Pointer to preference service instance
 */
void StyleManager::SetConfigurationManager(IConfigurationManager *configuration_manager)
{
    log_v("SetConfigurationManager() called");
    configuration_manager_ = configuration_manager;
}

// ========== Private Methods ==========

/**
 * @brief Reset all LVGL style objects to default state
 * @details Called during destruction to clean up LVGL resources
 */
void StyleManager::ResetStyles()
{
    log_v("ResetStyles() called");

    // Reset all core style objects to free LVGL resources
    lv_style_reset(&background_style_);
    lv_style_reset(&text_style_);
    lv_style_reset(&gauge_normal_style_);
    lv_style_reset(&gauge_warning_style_);
    lv_style_reset(&gauge_danger_style_);

    // Reset shared gauge component styles
    lv_style_reset(&gauge_indicator_style_);
    lv_style_reset(&gauge_items_style_);
    lv_style_reset(&gauge_main_style_);
    lv_style_reset(&gauge_danger_section_style_);
}

/**
 * @brief Static method to register configuration schema without instance
 * @param configurationManager Service to register schema with
 *
 * Called automatically at program startup through ConfigRegistry.
 * Registers the StyleManager configuration schema without
 * requiring a manager instance to exist.
 */
void StyleManager::RegisterConfigSchema(IConfigurationManager *configuration_manager)
{
    if (!configuration_manager)
    {
        log_e("StyleManager::RegisterConfigSchema: ConfigurationManager is null - style config registration failed!");
        ErrorManager::Instance().ReportCriticalError("StyleManager", "ConfigManager null - style config failed");
        return;
    }

    // Check if already registered to prevent duplicates
    if (configuration_manager->IsSchemaRegistered(CONFIG_SECTION))
    {
        return;
    }

    using namespace config;

    ConfigSection section(config_constants::sections::STYLE_MANAGER, CONFIG_SECTION,
                          config_constants::section_names::STYLE_MANAGER);

    section.AddItem(brightness_config);

    configuration_manager->RegisterConfigSection(section);
    log_i("StyleManager configuration schema registered (static)");
}

/**
 * @brief Instance method for backward compatibility during migration
 * @param configurationManager Service to register schema with
 *
 * This method maintains backward compatibility during migration.
 * New code path uses static RegisterConfigSchema instead.
 * Can be removed once all components are migrated.
 */
void StyleManager::RegisterConfig(IConfigurationManager *configuration_manager)
{
    // During migration, just delegate to static method
    RegisterConfigSchema(configuration_manager);
}
