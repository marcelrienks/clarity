#include "managers/style_manager.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include "definitions/constants.h"
#include "managers/configuration_manager.h"
#include <cstring>
#include <esp32-hal-log.h>

// Self-registration at program startup
static bool style_manager_registered = []() {
    ConfigurationManager::AddSchema(StyleManager::RegisterConfigSchema);
    return true;
}();

// Static instance for singleton pattern
static StyleManager* styleInstancePtr_ = nullptr;

// ========== Constructors and Destructor ==========

/**
 * @brief Construct StyleManager with specified initial theme
 * @details Sets up singleton pointer but does not initialize LVGL styles yet
 */
StyleManager::StyleManager(const char *theme) : theme_(theme), initialized_(false)
{
    log_v("StyleManager() constructor called");

    // Set singleton instance for interrupt architecture
    styleInstancePtr_ = this;
}

/**
 * @brief Destructor - clean up LVGL styles and singleton pointer
 */
StyleManager::~StyleManager()
{
    log_v("~StyleManager() destructor called");
    ResetStyles();

    // Clear singleton instance if we are the current instance
    if (styleInstancePtr_ == this) {
        styleInstancePtr_ = nullptr;
    }
}

// ========== Static Methods ==========

/**
 * @brief Singleton access for interrupt architecture
 * @details Must be initialized by ManagerFactory before use
 */
StyleManager& StyleManager::Instance() {
    if (!styleInstancePtr_) {
        log_e("StyleManager::Instance() called before initialization");
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "Instance() called before initialization - UI will not function");
        // In embedded systems, we need a valid instance
        // This should be set during ManagerFactory initialization
    }
    return *styleInstancePtr_;
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
    lv_style_init(&backgroundStyle_);
    lv_style_init(&textStyle_);
    lv_style_init(&gaugeNormalStyle_);
    lv_style_init(&gaugeWarningStyle_);
    lv_style_init(&gaugeDangerStyle_);

    // Initialize shared gauge component styles for memory efficiency
    lv_style_init(&gaugeIndicatorStyle_);
    lv_style_init(&gaugeItemsStyle_);
    lv_style_init(&gaugeMainStyle_);
    lv_style_init(&gaugeDangerSectionStyle_);

    // Mark as initialized before applying theme (since SetTheme checks this flag)
    initialized_ = true;

    // Apply theme from preferences
    if (configurationManager_) {
        LoadConfiguration();
        RegisterConfigCallback();
    } else {
        SetTheme(theme_.c_str());
    }
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
    // Track the applied theme so ApplyCurrentTheme() compares against what is on screen
    theme_ = theme;
    log_t("StyleManager: Theme applied: %s", theme);

    // Select the current theme colors
    ThemeColors colours = GetColours(theme);

    // Background style
    lv_style_set_bg_color(&backgroundStyle_, colours.background);
    lv_style_set_bg_opa(&backgroundStyle_, LV_OPA_COVER);

    // Text style
    lv_style_set_text_color(&textStyle_, colours.text);
    lv_style_set_text_opa(&textStyle_, LV_OPA_COVER);

    // Gauge styles
    lv_style_set_line_color(&gaugeNormalStyle_, colours.gaugeNormal);
    lv_style_set_line_color(&gaugeWarningStyle_, colours.gaugeWarning);
    lv_style_set_line_color(&gaugeDangerStyle_, colours.gaugeDanger);

    // Configure shared gauge component styles
    // Indicator style (Major ticks)
    lv_style_set_length(&gaugeIndicatorStyle_, 25);
    lv_style_set_line_width(&gaugeIndicatorStyle_, 7);
    lv_style_set_line_color(&gaugeIndicatorStyle_, colours.gaugeTicks);

    // Items style (Minor ticks)
    lv_style_set_length(&gaugeItemsStyle_, 18);
    lv_style_set_line_width(&gaugeItemsStyle_, 2);
    lv_style_set_line_color(&gaugeItemsStyle_, colours.gaugeTicks);

    // Main style (Arc)
    lv_style_set_arc_width(&gaugeMainStyle_, 0);

    // Danger section style
    lv_style_set_line_width(&gaugeDangerSectionStyle_, 5);
    lv_style_set_line_color(&gaugeDangerSectionStyle_, colours.gaugeDanger);

    lv_obj_t *current_screen = lv_scr_act();
    if (current_screen != nullptr) {
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
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "Not initialized - cannot apply theme to screen");
        return;
    }

    // Only apply the background style to screens
    // Other styles should be applied to specific components that need them
    lv_obj_add_style(screen, &backgroundStyle_, MAIN_DEFAULT);

    // Don't apply all styles to the same screen - this can cause conflicts
    // Components should apply their own specific styles as needed
}

/**
 * @brief Apply current theme from preferences and refresh styles
 */
void StyleManager::ApplyCurrentTheme()
{
    log_i("StyleManager::ApplyCurrentTheme() called");

    if (!configurationManager_) {
        log_e("StyleManager: No PreferenceService available - cannot read theme from preferences!");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "StyleManager",
                                            "No PreferenceService available - cannot persist theme");
        log_v("Current cached theme is: %s", theme_.c_str());
        return;
    }

    // Read theme using type-safe config system
    std::string preferenceTheme = Themes::DAY; // Default
    if (auto themeValue = configurationManager_->QueryConfig<std::string>(CONFIG_THEME)) {
        preferenceTheme = *themeValue;
    }
    log_v("Read theme from preferences: %s (cached: %s)", preferenceTheme.c_str(), theme_.c_str());

    // Apply the theme if it's different from cached value
    if (theme_ != preferenceTheme) {
        log_t("StyleManager: Theme changed from %s to %s - applying new theme", theme_.c_str(), preferenceTheme.c_str());
        SetTheme(preferenceTheme.c_str());
    } else {
        log_v("Theme unchanged (%s) - no update needed", preferenceTheme.c_str());
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
const ThemeColors &StyleManager::GetColours(const std::string& theme) const
{
    log_v("GetColours() called");
    if (theme.empty())
        return dayThemeColours_; // Default fallback

    // Map theme names to color schemes
    if (theme == Themes::NIGHT) return nightThemeColours_;
    if (theme == Themes::ERROR) return errorThemeColours_;

    return dayThemeColours_; // Fallback for unknown themes
}

/**
 * @brief Get current active theme name
 * @details Always reads from preferences when available for consistency
 */
const std::string& StyleManager::GetCurrentTheme() const
{
    // Always pull theme using type-safe config system for consistency
    if (configurationManager_) {
        static std::string currentTheme;
        if (auto themeValue = configurationManager_->QueryConfig<std::string>(CONFIG_THEME)) {
            currentTheme = *themeValue;
        } else {
            currentTheme = Themes::DAY;
        }
        return currentTheme;
    }

    // Fallback to cached value if preference service not available
    log_w("StyleManager: PreferenceService not available, using cached theme: %s", theme_.c_str());
    return theme_;
}

/**
 * @brief Direct theme switching interface for panel actions
 * @param themeName Name of theme to switch to immediately
 */
void StyleManager::SwitchTheme(const char* themeName)
{
    log_v("SwitchTheme() called");
    log_v("Direct theme switch requested to: %s", themeName);
    SetTheme(themeName);
}

/**
 * @brief Inject preference service dependency for theme persistence
 * @param configurationManager Pointer to preference service instance
 */
void StyleManager::SetConfigurationManager(IConfigurationManager* configurationManager)
{
    log_v("SetConfigurationManager() called");
    configurationManager_ = configurationManager;
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
    lv_style_reset(&backgroundStyle_);
    lv_style_reset(&textStyle_);
    lv_style_reset(&gaugeNormalStyle_);
    lv_style_reset(&gaugeWarningStyle_);
    lv_style_reset(&gaugeDangerStyle_);

    // Reset shared gauge component styles
    lv_style_reset(&gaugeIndicatorStyle_);
    lv_style_reset(&gaugeItemsStyle_);
    lv_style_reset(&gaugeMainStyle_);
    lv_style_reset(&gaugeDangerSectionStyle_);
}

/**
 * @brief Static method to register configuration schema without instance
 * @param configurationManager Service to register schema with
 *
 * Called automatically at program startup through ConfigRegistry.
 * Registers the StyleManager configuration schema without
 * requiring a manager instance to exist.
 */
void StyleManager::RegisterConfigSchema(IConfigurationManager* configurationManager)
{
    if (!configurationManager) {
        log_e("StyleManager::RegisterConfigSchema: ConfigurationManager is null - style config registration failed!");
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "ConfigManager null - style config failed");
        return;
    }

    // Check if already registered to prevent duplicates
    if (configurationManager->IsSchemaRegistered(CONFIG_SECTION)) {
        return;
    }

    using namespace Config;

    ConfigSection section(ConfigConstants::Sections::STYLE_MANAGER, CONFIG_SECTION, ConfigConstants::SectionNames::STYLE_MANAGER);

    section.AddItem(themeConfig_);
    section.AddItem(brightnessConfig_);

    configurationManager->RegisterConfigSection(section);
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
void StyleManager::RegisterConfig(IConfigurationManager* configurationManager)
{
    // During migration, just delegate to static method
    RegisterConfigSchema(configurationManager);
}

/**
 * @brief Load configuration from preference system
 */
void StyleManager::LoadConfiguration()
{
    if (!configurationManager_) {
        log_e("StyleManager::LoadConfiguration: ConfigurationManager is null - cannot load style configuration!");
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "ConfigManager null - using default styles");
        return;
    }

    // Register configuration first
    RegisterConfig(configurationManager_);

    // Load theme using type-safe config system
    if (auto themeValue = configurationManager_->QueryConfig<std::string>(CONFIG_THEME)) {
        SetTheme(themeValue->c_str());
    } else {
        SetTheme(Themes::DAY); // Default theme
    }

    log_i("Loaded style configuration: theme=%s", theme_.c_str());
}

/**
 * @brief Refresh configuration in response to live config changes
 */
void StyleManager::RefreshConfig()
{
    log_i("StyleManager: Refreshing config due to live update");
    LoadConfiguration();
}

/**
 * @brief Register callback for live configuration updates
 */
void StyleManager::RegisterConfigCallback()
{
    if (!configurationManager_) {
        log_e("StyleManager::RegisterConfigCallback: ConfigurationManager is null - cannot register config callbacks!");
        ErrorManager::Instance().ReportCriticalError("StyleManager",
                                                     "ConfigurationManager is null - style updates will not work");
        return;
    }

    // Register callback that calls RefreshConfig when our section changes
    auto callback = [this](const std::string& fullKey,
                          const std::optional<Config::ConfigValue>& oldValue,
                          const Config::ConfigValue& newValue) {
        RefreshConfig();
    };

    configurationManager_->RegisterChangeCallback(CONFIG_THEME, callback);
    log_i("StyleManager: Registered config callback for key: %s", CONFIG_THEME);
}