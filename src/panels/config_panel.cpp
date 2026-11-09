#include "panels/config_panel.h"
#include "managers/error_manager.h"
#include "managers/style_manager.h"
#include "definitions/types.h"
#include "utilities/logging.h"
#include "definitions/constants.h"
#include "definitions/styles.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cctype>
#include <unordered_map>

/**
 * @brief Constructs configuration panel with required service dependencies
 * @param gpio GPIO provider for hardware interaction
 * @param display Display provider for screen management
 * @param styleManager Style service for theme management
 *
 * Creates configuration panel with stack-allocated configuration component.
 * Initializes menu system for automotive settings management including
 * theme selection, calibration, and system configuration options.
 */
ConfigPanel::ConfigPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *styleManager,
                         IPanelManager *panelManager, IConfigurationManager *configurationManager)
    : gpioProvider_(gpio), displayProvider_(display), styleManager_(styleManager), panelManager_(panelManager),
      configurationManager_(configurationManager),
      configComponent_(), componentInitialized_(false), currentMenuIndex_(0)
{
    log_v("ConfigPanel constructor called");
}

/**
 * @brief Destructor cleans up configuration panel resources
 *
 * Safely deletes LVGL screen objects and cleans up menu state.
 * Stack-allocated component is automatically destroyed. Ensures
 * proper cleanup when exiting configuration interface.
 */
ConfigPanel::~ConfigPanel()
{
    log_v("~ConfigPanel() destructor called");

    // Components delete no LVGL objects, so destroying them after the screen is safe
    if (screen_)
    {
        lv_obj_delete(screen_);
        screen_ = nullptr;
    }
}

// Core Functionality Methods

/**
 * @brief Initializes configuration panel UI structure and menu system
 *
 * Creates LVGL screen, validates display provider, sets up component location,
 * and initializes configuration menu structure. Builds menu hierarchy for
 * automotive system configuration including themes, calibration, and settings.
 */
void ConfigPanel::Init()
{
    log_v("Init() called");
    
    if (!displayProvider_)
    {
        log_e("ConfigPanel requires display provider");
        ErrorManager::Instance().ReportCriticalError(PanelNames::CONFIG, "Missing required display provider");
        return;
    }

    screen_ = displayProvider_->CreateScreen();

    if (!styleManager_)
        return;

    styleManager_->ApplyThemeToScreen(screen_);
    
    log_i("ConfigPanel initialization completed");
}

/**
 * @brief Loads configuration panel UI and displays main menu
 *
 * Renders configuration component, sets up main menu items, applies current
 * theme styling, and loads the LVGL screen. Sets up button action handlers
 * for menu navigation and selection. Critical for configuration interface.
 */
void ConfigPanel::Load()
{
    log_v("Load() called");
    

    // Reset menu to first item
    currentMenuIndex_ = 0;

    // Apply current theme to config panel (don't change the theme, just apply existing)
    if (styleManager_)
    {
        styleManager_->ApplyThemeToScreen(screen_);
        
        // For night theme, override the screen background to use dark red instead of black
        const std::string& theme = styleManager_->GetCurrentTheme();
        if (theme == Themes::NIGHT) {
            lv_obj_set_style_bg_color(screen_, lv_color_hex(Colors::NIGHT_BACKGROUND), LV_PART_MAIN); // Very dark red
            lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);
        }
    }

    // Initialize the config component with the screen
    if (!screen_)
        return;

    // Mark component as initialized
    componentInitialized_ = true;

    configComponent_.Init(screen_);
    configComponent_.SetStyleService(styleManager_);

    // Build dynamic menu from registered configuration sections
    BuildDynamicMenus();
    configComponent_.SetMenuItems(menuItems_);
    configComponent_.SetCurrentIndex(currentMenuIndex_);

    // Register the screen loaded event callback
    lv_obj_add_event_cb(screen_, ConfigPanel::ShowPanelCompletionCallback, LV_EVENT_SCREEN_LOADED, this);
    
    lv_screen_load(screen_);
    
    log_t("ConfigPanel loaded successfully");
}

/**
 * @brief Updates configuration panel display with current settings
 *
 * Monitors for theme changes and updates component styling accordingly.
 * Configuration panels are primarily static with updates driven by
 * user interactions rather than continuous data changes.
 */
void ConfigPanel::Update()
{
    log_v("Update() called");

    // Config panel is static - no updates needed, but must reset UI state to IDLE
    if (panelManager_) {
        panelManager_->SetUiState(UIState::IDLE);
    }
}

// SetManagers and SetConfigurationManager removed - using constructor injection

/**
 * @brief Static callback for screen loaded event
 * @param event LVGL event containing panel context
 */
void ConfigPanel::ShowPanelCompletionCallback(lv_event_t *event)
{
    log_v("ShowPanelCompletionCallback() called");
    // Get the panel pointer from event user data
    auto *panel = static_cast<ConfigPanel *>(lv_event_get_user_data(event));
    if (panel && panel->panelManager_) {
        panel->panelManager_->SetUiState(UIState::IDLE);
    }
}

// ========== Public Action Handlers ==========

/**
 * @brief Handles short button press to cycle through menu items
 *
 * Increments the current menu index to navigate to the next menu item.
 * Wraps around to the first item when reaching the end of the menu.
 * Updates the UI component to reflect the new selection.
 */
void ConfigPanel::HandleShortPress()
{
    log_v("HandleShortPress() called");

    if (menuItems_.empty()) {
        log_w("No menu items available");
        return;
    }

    // Cycle to next menu item
    currentMenuIndex_ = (currentMenuIndex_ + 1) % menuItems_.size();

    if (componentInitialized_) {
        configComponent_.SetCurrentIndex(currentMenuIndex_);
    }

    log_t("ConfigPanel: %s option highlighted", menuItems_[currentMenuIndex_].label.c_str());
}

/**
 * @brief Handles long button press to execute selected menu action
 *
 * Validates the current menu state and executes the action associated
 * with the selected menu item. Actions include entering sections,
 * toggling booleans, showing options, or exiting the panel.
 */
void ConfigPanel::HandleLongPress()
{

    if (!ValidateMenuState()) {
        log_e("ConfigPanel::HandleLongPress() - ValidateMenuState failed");
        return;
    }

    const auto& item = menuItems_[currentMenuIndex_];

    ExecuteMenuAction(item);
}

// ========== Helper Methods for HandleLongPress ==========

/**
 * @brief Validates the current menu state before executing actions
 * @return true if menu state is valid, false otherwise
 *
 * Checks if menu items exist and current index is within valid range.
 * Logs warnings for invalid states to aid debugging.
 */
bool ConfigPanel::ValidateMenuState() const
{
    if (menuItems_.empty()) {
        log_w("No menu items available");
        return false;
    }

    if (currentMenuIndex_ >= menuItems_.size()) {
        log_w("Invalid menu index: %zu", currentMenuIndex_);
        return false;
    }

    return true;
}

/**
 * @brief Executes the action associated with a menu item
 * @param item The menu item containing action type and parameters
 *
 * Parses the action type and delegates to appropriate handler method.
 * Supports section navigation, value toggling, options display, and panel exit.
 */
void ConfigPanel::ExecuteMenuAction(const ConfigComponent::MenuItem& item)
{

    MenuActionType actionType = ParseActionType(item.actionType);

    switch (actionType) {
        case MenuActionType::ENTER_SECTION:
            HandleEnterSection(item.actionParam);
            break;
        case MenuActionType::TOGGLE_BOOLEAN:
            HandleToggleBoolean(item.actionParam);
            break;
        case MenuActionType::SHOW_OPTIONS:
            HandleShowOptions(item.actionParam);
            break;
        case MenuActionType::SET_CONFIG_VALUE:
            HandleSetConfigValue(item.actionParam);
            break;
        case MenuActionType::BACK:
            HandleBackAction(item.actionParam);
            break;
        case MenuActionType::NONE:
            break;
        case MenuActionType::PANEL_EXIT:
            HandlePanelExit();
            break;
        case MenuActionType::UNKNOWN:
        default:
            log_w("ExecuteMenuAction - Unknown action type: %s (enum: %d)", item.actionType.c_str(), static_cast<int>(actionType));
            break;
    }
}

/**
 * @brief Parses string action type to enum value
 * @param actionTypeStr String representation of action type
 * @return Corresponding MenuActionType enum value
 *
 * Maps string action types from menu items to strongly-typed enum values.
 * Returns UNKNOWN for unrecognized action types.
 */
ConfigPanel::MenuActionType ConfigPanel::ParseActionType(const std::string& actionTypeStr) const
{
    static const std::unordered_map<std::string, MenuActionType> actionTypeMap = {
        {UIStrings::ActionTypes::ENTER_SECTION, MenuActionType::ENTER_SECTION},
        {UIStrings::ActionTypes::TOGGLE_BOOLEAN, MenuActionType::TOGGLE_BOOLEAN},
        {UIStrings::ActionTypes::SHOW_OPTIONS, MenuActionType::SHOW_OPTIONS},
        {UIStrings::ActionTypes::SET_CONFIG_VALUE, MenuActionType::SET_CONFIG_VALUE},
        {UIStrings::ActionTypes::BACK, MenuActionType::BACK},
        {UIStrings::ActionTypes::NONE, MenuActionType::NONE},
        {UIStrings::ActionTypes::PANEL_EXIT, MenuActionType::PANEL_EXIT}
    };

    auto it = actionTypeMap.find(actionTypeStr);
    return (it != actionTypeMap.end()) ? it->second : MenuActionType::UNKNOWN;
}

/**
 * @brief Handles entering a configuration section
 * @param sectionName Name of the section to enter
 *
 * Stores the current section name and builds the section-specific menu.
 * Used when user selects a configuration category from main menu.
 */
void ConfigPanel::HandleEnterSection(const std::string& sectionName)
{
    log_t("ConfigPanel: Entering section %s", sectionName.c_str());
    currentSectionName_ = sectionName;
    BuildSectionMenu(currentSectionName_);
    log_t("ConfigPanel: Section %s menu loaded", currentSectionName_.c_str());
}

/**
 * @brief Handles toggling of boolean configuration values
 * @param fullKey Full configuration key (section.item format)
 *
 * Retrieves the configuration item, toggles its boolean value,
 * and refreshes the menu to show the updated state.
 */
void ConfigPanel::HandleToggleBoolean(const std::string& fullKey)
{
    auto [sectionName, itemKey] = ParseConfigKey(fullKey);
    if (auto section = configurationManager_->GetConfigSection(sectionName)) {
        for (const auto& configItem : section->items) {
            if (configItem.key == itemKey) {
                ShowBooleanToggle(fullKey, configItem);
                break;
            }
        }
    }
}

/**
 * @brief Displays options menu for a configuration item
 * @param fullKey Full configuration key (section.item format)
 *
 * Retrieves the configuration item and displays appropriate options
 * based on its type (enum, numeric, or string).
 */
void ConfigPanel::HandleShowOptions(const std::string& fullKey)
{
    auto [sectionName, itemKey] = ParseConfigKey(fullKey);
    if (auto section = configurationManager_->GetConfigSection(sectionName)) {
        for (const auto& configItem : section->items) {
            if (configItem.key == itemKey) {
                ShowOptionsMenu(fullKey, configItem);
                break;
            }
        }
    }
}

/**
 * @brief Sets a configuration value from menu selection
 * @param actionParam Combined key:value string
 *
 * Parses the action parameter to extract key and value, updates the
 * configuration based on item type, and returns to section menu.
 * Handles string, integer, float, boolean, and enum types.
 */
void ConfigPanel::HandleSetConfigValue(const std::string& actionParam)
{

    size_t colonPos = actionParam.find(':');
    if (colonPos == std::string::npos) {
        log_e("HandleSetConfigValue - Invalid set_config_value format: %s", actionParam.c_str());
        return;
    }

    std::string fullKey = actionParam.substr(0, colonPos);
    std::string value = actionParam.substr(colonPos + 1);

    auto [sectionName, itemKey] = ParseConfigKey(fullKey);

    if (auto section = configurationManager_->GetConfigSection(sectionName)) {
        for (const auto& configItem : section->items) {
            if (configItem.key == itemKey) {
                // Parse the string value into the correct type based on existing value
                Config::ConfigValue newValue = configurationManager_->FromString(value, configItem.value);
                log_t("ConfigPanel: Applying %s setting %s", fullKey.c_str(), value.c_str());

                // Update the configuration
                configurationManager_->UpdateConfig(fullKey, newValue);

                // If this was a theme change, update the config component colors
                if (fullKey == UIStrings::ConfigKeys::STYLE_MANAGER_THEME) {
                    configComponent_.UpdateThemeColors();
                }

                // Go back to section menu
                BuildSectionMenu(sectionName);
                log_t("ConfigPanel: Returned to section %s menu", sectionName.c_str());
                return;
            }
        }
        log_e("HandleSetConfigValue - Config item not found for key: %s", itemKey.c_str());
    } else {
        log_e("HandleSetConfigValue - Section not found: %s", sectionName.c_str());
    }
}

/**
 * @brief Handles navigation back to previous menu level
 * @param actionParam Optional parameter specifying target menu
 *
 * Navigates back from submenu to section menu or from section to main menu.
 * Uses stored section name to determine appropriate navigation target.
 */
void ConfigPanel::HandleBackAction(const std::string& actionParam)
{

    // If we're in a section menu, go back to main menu
    if (!currentSectionName_.empty()) {
        currentSectionName_.clear(); // Clear the current section
        BuildDynamicMenus();
    } else {
        // Already at main menu, go back to main menu anyway
        BuildDynamicMenus();
    }
}

/**
 * @brief Handles exit from configuration panel
 *
 * Retrieves the restoration panel from panel service and loads it.
 * Typically returns to the previous operational panel.
 */
void ConfigPanel::HandlePanelExit()
{
    log_t("ConfigPanel: Executing exit function");
    if (panelManager_) {
        const char *restorationPanel = panelManager_->GetRestorationPanel();
        panelManager_->CreateAndLoadPanel(restorationPanel, true);
    }
}

// ========== Dynamic Configuration Implementation ==========

/**
 * @brief Builds main configuration menu from registered sections
 *
 * Queries preference service for all registered configuration sections
 * and creates menu items for each. Adds exit option at the end.
 * Updates UI component with generated menu structure.
 */
void ConfigPanel::BuildDynamicMenus()
{
    log_v("BuildDynamicMenus() called");

    if (!configurationManager_) {
        log_e("Cannot build dynamic menus - preference service not available");
        return;
    }

    // Get all registered sections
    auto sectionNames = configurationManager_->GetRegisteredSectionNames();

    // Build main menu dynamically
    menuItems_.clear();

    // Add sections as menu items
    for (const auto& sectionName : sectionNames) {
        if (auto section = configurationManager_->GetConfigSection(sectionName)) {
            ConfigComponent::MenuItem item;
            item.label = section->displayName;
            item.actionType = UIStrings::ActionTypes::ENTER_SECTION;
            item.actionParam = sectionName;
            menuItems_.push_back(item);
        }
    }

    // Add exit option
    menuItems_.push_back({UIStrings::MenuLabels::EXIT, UIStrings::ActionTypes::PANEL_EXIT, ""});

    // Update component
    if (componentInitialized_) {
        configComponent_.SetTitle(UIStrings::MenuLabels::CONFIGURATION);
        configComponent_.SetMenuItems(menuItems_);
        configComponent_.SetCurrentIndex(0);
    }

    currentMenuIndex_ = 0;
    log_i("Built dynamic menu with %zu sections", sectionNames.size());
}

/**
 * @brief Builds menu for a specific configuration section
 * @param sectionName Name of the section to build menu for
 *
 * Creates menu items for each configuration item in the section.
 * Boolean items use toggle action, others show options menu.
 * Adds back option to return to main menu.
 */
void ConfigPanel::BuildSectionMenu(const std::string& sectionName)
{
    log_v("BuildSectionMenu() called for section: %s", sectionName.c_str());

    auto section = configurationManager_->GetConfigSection(sectionName);
    if (!section) {
        log_e("Section not found: %s", sectionName.c_str());
        return;
    }

    menuItems_.clear();

    // Add items from the section
    for (const auto& item : section->items) {
        ConfigComponent::MenuItem menuItem;
        menuItem.label = FormatItemLabel(item);

        // Create action based on item type
        std::string fullKey = sectionName + "." + item.key;

        // For booleans, use direct toggle. For everything else, show options
        if (std::holds_alternative<bool>(item.value)) {
            menuItem.actionType = UIStrings::ActionTypes::TOGGLE_BOOLEAN;
        } else {
            menuItem.actionType = UIStrings::ActionTypes::SHOW_OPTIONS;
        }
        menuItem.actionParam = fullKey;
        menuItems_.push_back(menuItem);
    }

    // Add back option
    menuItems_.push_back({UIStrings::MenuLabels::BACK, UIStrings::ActionTypes::BACK, ""});

    // Update component
    if (componentInitialized_) {
        configComponent_.SetTitle(section->displayName);
        configComponent_.SetMenuItems(menuItems_);
        configComponent_.SetCurrentIndex(0);
    }

    currentMenuIndex_ = 0;
}

/**
 * @brief Formats configuration item label with current value
 * @param item Configuration item to format
 * @return Formatted string "DisplayName: Value"
 *
 * Creates user-friendly label showing both item name and current value.
 * Used in section menus to display configuration state.
 */
std::string ConfigPanel::FormatItemLabel(const Config::ConfigItem& item) const
{
    std::string valueStr = configurationManager_->ToString(item.value);
    return item.displayName + ": " + valueStr;
}

/**
 * @brief Displays options menu for configuration item selection
 * @param fullKey Full configuration key (section.item format)
 * @param item Configuration item to show options for
 *
 * Main coordinator that delegates to specialized handlers based on item type.
 * Handles enum options, numeric ranges, and string values appropriately.
 * Updates UI with generated options and back navigation.
 */
void ConfigPanel::ShowOptionsMenu(const std::string& fullKey, const Config::ConfigItem& item)
{
    log_i("ShowOptionsMenu() for key: %s", fullKey.c_str());

    // Make a copy of fullKey to avoid reference invalidation when menuItems_ is cleared
    std::string keyStr = fullKey;

    menuItems_.clear();

    // Parse constraints to get available options
    auto options = ParseOptions(item.metadata.constraints);

    // Delegate to appropriate specialized method based on data type and available options
    if (!options.empty()) {
        ShowEnumOptionsMenu(keyStr, item, options);
    } else if (configurationManager_->IsNumeric(item.value)) {
        ShowNumericOptionsMenu(keyStr, item);
    } else {
        ShowStringOptionsMenu(keyStr, item);
    }

    // Add back option
    menuItems_.push_back({UIStrings::MenuLabels::BACK, UIStrings::ActionTypes::BACK, ""});

    // Update component
    if (componentInitialized_) {
        configComponent_.SetTitle(item.displayName);
        configComponent_.SetMenuItems(menuItems_);
        configComponent_.SetCurrentIndex(0);
    }

    currentMenuIndex_ = 0;
    log_t("ConfigPanel: Options menu for %s loaded", keyStr.c_str());
}

/**
 * @brief Toggles boolean configuration value immediately
 * @param fullKey Full configuration key (section.item format)
 * @param item Boolean configuration item to toggle
 *
 * Directly toggles boolean value without showing options menu.
 * Updates configuration and refreshes section menu to show new state.
 */
void ConfigPanel::ShowBooleanToggle(const std::string& fullKey, const Config::ConfigItem& item)
{
    log_i("ShowBooleanToggle() for key: %s", fullKey.c_str());

    if (auto currentValue = configurationManager_->GetValue<bool>(item.value)) {
        bool newValue = !(*currentValue);
        configurationManager_->UpdateConfig(fullKey, newValue);

        // Refresh the section menu to show updated value
        auto [sectionName, itemKey] = ParseConfigKey(fullKey);
        BuildSectionMenu(sectionName);

        log_i("Toggled %s from %s to %s", fullKey.c_str(),
              *currentValue ? "true" : "false", newValue ? "true" : "false");
    }
}

/**
 * @brief Parses comma-separated constraint string into options list
 * @param constraints Comma-separated string of valid options
 * @return Vector of trimmed option strings
 *
 * Splits constraint string by commas and trims whitespace.
 * Used for enum-type configuration items with discrete choices.
 */
std::vector<std::string> ConfigPanel::ParseOptions(const std::string& constraints) const
{
    std::vector<std::string> options;
    if (constraints.empty()) return options;

    std::stringstream ss(constraints);
    std::string option;

    while (std::getline(ss, option, ',')) {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        if (!option.empty()) {
            options.push_back(option);
        }
    }

    return options;
}

/**
 * @brief Splits configuration key into section and item components
 * @param fullKey Full configuration key in "section.item" format
 * @return Pair of (section, item) strings
 *
 * Separates section name from item key at the dot delimiter.
 * Returns empty section if no dot is found.
 */
std::pair<std::string, std::string> ConfigPanel::ParseConfigKey(const std::string& fullKey) const
{
    size_t dotPos = fullKey.find('.');
    if (dotPos == std::string::npos) {
        return {"", fullKey};
    }
    return {fullKey.substr(0, dotPos), fullKey.substr(dotPos + 1)};
}

// ========== Extracted ShowOptionsMenu Helper Methods ==========

/**
 * @brief Shows menu for enum-type configuration with discrete options
 * @param fullKey Full configuration key for action handling
 * @param item Configuration item being configured
 * @param options Vector of valid option strings
 *
 * Creates menu items for each option with selection indicator.
 * Current value is marked with arrow prefix for visual feedback.
 */
void ConfigPanel::ShowEnumOptionsMenu(const std::string& fullKey, const Config::ConfigItem& item, const std::vector<std::string>& options)
{
    std::string currentValue = configurationManager_->ToString(item.value);

    for (const auto& option : options) {
        bool isSelected = (option == currentValue);
        ConfigComponent::MenuItem menuItem = CreateMenuItemWithSelection(option, fullKey, option, isSelected);
        menuItems_.push_back(menuItem);
    }
}

/**
 * @brief Shows menu for numeric configuration with generated values
 * @param fullKey Full configuration key for action handling
 * @param item Numeric configuration item (integer or float)
 *
 * Parses numeric range, generates appropriate step values, and creates
 * menu items with formatted values and units. Includes current value,
 * min/max bounds, and calculated increment options.
 */
void ConfigPanel::ShowNumericOptionsMenu(const std::string& fullKey, const Config::ConfigItem& item)
{
    // Parse range constraints and get current value
    NumericRange range = ParseNumericRange(item);

    // Generate appropriate numeric values based on range and current value
    std::vector<float> values = GenerateNumericValues(range, item);

    // Create menu items for each value
    for (float value : values) {
        std::string valueStr = FormatNumericValue(value, item);
        bool isSelected = (value == range.currentValue);
        ConfigComponent::MenuItem menuItem = CreateMenuItemWithSelection(valueStr, fullKey, std::to_string(value), isSelected);
        menuItems_.push_back(menuItem);
    }
}

/**
 * @brief Shows display-only menu for string configuration
 * @param fullKey Full configuration key (unused for strings)
 * @param item String configuration item
 *
 * Displays current string value in read-only format.
 * String editing not supported through menu interface.
 */
void ConfigPanel::ShowStringOptionsMenu(const std::string& fullKey, const Config::ConfigItem& item)
{
    // For string types without options, just show current value
    ConfigComponent::MenuItem currentItem;
    currentItem.label = UIStrings::ConfigUI::CURRENT_LABEL_PREFIX + configurationManager_->ToString(item.value);
    currentItem.actionType = UIStrings::ConfigUI::ACTION_TYPE_NONE;
    currentItem.actionParam = UIStrings::ConfigUI::EMPTY_PARAM;
    menuItems_.push_back(currentItem);
}

/**
 * @brief Creates menu item with selection indicator
 * @param label Display text for the menu item
 * @param fullKey Configuration key for action parameter
 * @param value Value to set when item is selected
 * @param isSelected Whether this item represents current value
 * @return Configured MenuItem struct
 *
 * Centralizes menu item creation with consistent selection marking.
 * Selected items are prefixed with arrow indicator.
 */
ConfigComponent::MenuItem ConfigPanel::CreateMenuItemWithSelection(const std::string& label, const std::string& fullKey, const std::string& value, bool isSelected) const
{
    ConfigComponent::MenuItem menuItem;

    // Show marker for current selection
    if (isSelected) {
        menuItem.label = UIStrings::ConfigUI::SELECTED_MENU_PREFIX + label;
    } else {
        menuItem.label = UIStrings::ConfigUI::UNSELECTED_MENU_PREFIX + label;
    }

    menuItem.actionType = UIStrings::ActionTypes::SET_CONFIG_VALUE;
    menuItem.actionParam = fullKey + ":" + value;

    return menuItem;
}

// ========== Extracted ShowNumericOptionsMenu Helper Methods ==========

/**
 * @brief Parses numeric constraints into range structure
 * @param item Configuration item with numeric constraints
 * @return NumericRange struct with min, max, and current values
 *
 * Extracts range boundaries from constraint string (format: "min-max" or "min,max").
 * Defaults to 0-100 range if constraints are not specified.
 * Includes current value from configuration item.
 */
ConfigPanel::NumericRange ConfigPanel::ParseNumericRange(const Config::ConfigItem& item) const
{
    NumericRange range;
    range.minValue = 0;
    range.maxValue = 100;
    range.currentValue = std::stof(configurationManager_->ToString(item.value));

    if (!item.metadata.constraints.empty()) {
        std::string constraints = item.metadata.constraints;
        // Replace comma with dash for consistent parsing
        std::replace(constraints.begin(), constraints.end(), ',', '-');
        size_t dashPos = constraints.find('-');
        if (dashPos != std::string::npos) {
            range.minValue = std::stof(constraints.substr(0, dashPos));
            range.maxValue = std::stof(constraints.substr(dashPos + 1));
        }
    }

    return range;
}

/**
 * @brief Generates selectable numeric values within range
 * @param range Numeric range with min, max, and current values
 * @param item Configuration item to determine type (int/float)
 * @return Vector of numeric values for menu options
 *
 * Creates intelligent value options including current value, min/max bounds,
 * and calculated small/large steps. Integer types use rounded steps.
 * Ensures all generated values stay within specified range.
 */
std::vector<float> ConfigPanel::GenerateNumericValues(const NumericRange& range, const Config::ConfigItem& item) const
{
    // Generate reasonable step options
    float rangeSize = range.maxValue - range.minValue;
    float smallStep = rangeSize / 20.0f;  // 5% of range
    float largeStep = rangeSize / 4.0f;   // 25% of range

    // For integers, round steps
    if (std::holds_alternative<int>(item.value)) {
        smallStep = std::max(1.0f, std::round(smallStep));
        largeStep = std::max(5.0f, std::round(largeStep));
    }

    // Generate option values
    std::vector<float> values;

    // Add preset values
    if (range.currentValue - largeStep >= range.minValue) {
        values.push_back(range.currentValue - largeStep);
    }
    if (range.currentValue - smallStep >= range.minValue) {
        values.push_back(range.currentValue - smallStep);
    }
    values.push_back(range.currentValue);  // Current value
    if (range.currentValue + smallStep <= range.maxValue) {
        values.push_back(range.currentValue + smallStep);
    }
    if (range.currentValue + largeStep <= range.maxValue) {
        values.push_back(range.currentValue + largeStep);
    }

    // Also add min and max if not already included
    if (std::find(values.begin(), values.end(), range.minValue) == values.end()) {
        values.insert(values.begin(), range.minValue);
    }
    if (std::find(values.begin(), values.end(), range.maxValue) == values.end()) {
        values.push_back(range.maxValue);
    }

    return values;
}

/**
 * @brief Formats numeric value for display with units
 * @param value Numeric value to format
 * @param item Configuration item containing type and unit metadata
 * @return Formatted string with appropriate precision and units
 *
 * Integers displayed without decimals, floats with one decimal place.
 * Appends unit suffix from metadata if specified.
 */
std::string ConfigPanel::FormatNumericValue(float value, const Config::ConfigItem& item) const
{
    std::string valueStr;

    if (std::holds_alternative<int>(item.value)) {
        valueStr = std::to_string(static_cast<int>(value));
    } else {
        // Format float with 1 decimal place
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(value));
        valueStr = buffer;
    }

    // Add units if specified
    if (!item.metadata.unit.empty()) {
        valueStr += UIStrings::ConfigUI::UNIT_SEPARATOR + item.metadata.unit;
    }

    return valueStr;
}
