#include "panels/config_panel.h"
#include "definitions/constants.h"
#include "definitions/types.h"
#include "managers/error_manager.h"
#include "ui/theme.h"
#include "utilities/logging.h"
#include <Arduino.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <unordered_map>

ConfigPanel::ConfigPanel(IConfigurationManager &configuration) : configuration_manager_(&configuration)
{
}

void ConfigPanel::Build(lv_obj_t *screen, Theme &theme)
{
    (void)screen;
    theme_ = &theme;

    // Every visit starts at the main menu, on its first item
    current_menu_index_ = 0;
    current_section_name_.clear();

    ApplyMenuBackground();
    wheel_.Build(screen_, theme.Colors());

    // Build dynamic menu from registered configuration sections
    BuildDynamicMenus();
}

void ConfigPanel::OnExit()
{
    theme_ = nullptr;
    menu_items_.clear();
}

/**
 * @brief Follows a theme change while the panel is open
 *
 * Otherwise static: updates are driven by button presses.
 */
void ConfigPanel::Update(uint32_t now)
{
    (void)now;
    if (screen_ && theme_->Active() != styled_theme_)
    {
        ApplyMenuBackground();
        wheel_.Restyle(theme_->Colors());
    }
}

/// Shows menu_items_ under a title, selecting the first item
void ConfigPanel::ShowMenu(const std::string &title)
{
    if (!screen_)
    {
        return;
    }

    std::vector<std::string> labels;
    labels.reserve(menu_items_.size());
    for (const MenuItem &item : menu_items_)
    {
        labels.push_back(item.label);
    }

    wheel_.SetTitle(title);
    wheel_.SetItems(labels);
    wheel_.SetIndex(0);
}

/**
 * @brief Gives the screen the theme's menu background (dark red at night instead of black)
 */
void ConfigPanel::ApplyMenuBackground()
{
    styled_theme_ = theme_->Active();
    lv_obj_set_style_bg_color(screen_, theme_->Colors().menuBackground, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, LV_PART_MAIN);
}

// SetManagers and SetConfigurationManager removed - using constructor injection

// ========== Public Action Handlers ==========

clarity::core::NavIntent ConfigPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event == clarity::core::ButtonEvent::Short)
    {
        HandleShortPress();
        return clarity::core::NavIntent::None;
    }
    if (event == clarity::core::ButtonEvent::Long)
    {
        return HandleLongPress();
    }
    return clarity::core::NavIntent::None;
}

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

    if (menu_items_.empty())
    {
        log_w("No menu items available");
        return;
    }

    // Cycle to next menu item
    current_menu_index_ = (current_menu_index_ + 1) % menu_items_.size();

    if (screen_)
    {
        wheel_.SetIndex(current_menu_index_);
    }

    log_t("ConfigPanel: %s option highlighted", menu_items_[current_menu_index_].label.c_str());
}

/**
 * @brief Handles long button press to execute selected menu action
 *
 * Validates the current menu state and executes the action associated
 * with the selected menu item. Actions include entering sections,
 * toggling booleans, showing options, or exiting the panel.
 */
clarity::core::NavIntent ConfigPanel::HandleLongPress()
{

    if (!ValidateMenuState())
    {
        log_e("ConfigPanel::HandleLongPress() - ValidateMenuState failed");
        return clarity::core::NavIntent::None;
    }

    // A copy: entering a section or an option list rebuilds menu_items_
    const MenuItem item = menu_items_[current_menu_index_];

    return ExecuteMenuAction(item);
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
    if (menu_items_.empty())
    {
        log_w("No menu items available");
        return false;
    }

    if (current_menu_index_ >= menu_items_.size())
    {
        log_w("Invalid menu index: %zu", current_menu_index_);
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
clarity::core::NavIntent ConfigPanel::ExecuteMenuAction(const MenuItem &item)
{

    MenuActionType action_type = ParseActionType(item.actionType);

    switch (action_type)
    {
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
            return HandlePanelExit();
        case MenuActionType::UNKNOWN:
        default:
            log_w("ExecuteMenuAction - Unknown action type: %s (enum: %d)", item.actionType.c_str(),
                  static_cast<int>(action_type));
            break;
    }
    return clarity::core::NavIntent::None;
}

/**
 * @brief Parses string action type to enum value
 * @param actionTypeStr String representation of action type
 * @return Corresponding MenuActionType enum value
 *
 * Maps string action types from menu items to strongly-typed enum values.
 * Returns UNKNOWN for unrecognized action types.
 */
ConfigPanel::MenuActionType ConfigPanel::ParseActionType(const std::string &action_type_str) const
{
    static const std::unordered_map<std::string, MenuActionType> ACTION_TYPE_MAP = {
        {ui_strings::action_types::ENTER_SECTION, MenuActionType::ENTER_SECTION},
        {ui_strings::action_types::TOGGLE_BOOLEAN, MenuActionType::TOGGLE_BOOLEAN},
        {ui_strings::action_types::SHOW_OPTIONS, MenuActionType::SHOW_OPTIONS},
        {ui_strings::action_types::SET_CONFIG_VALUE, MenuActionType::SET_CONFIG_VALUE},
        {ui_strings::action_types::BACK, MenuActionType::BACK},
        {ui_strings::action_types::NONE, MenuActionType::NONE},
        {ui_strings::action_types::PANEL_EXIT, MenuActionType::PANEL_EXIT}};

    auto it = ACTION_TYPE_MAP.find(action_type_str);
    return (it != ACTION_TYPE_MAP.end()) ? it->second : MenuActionType::UNKNOWN;
}

/**
 * @brief Handles entering a configuration section
 * @param section_name Name of the section to enter
 *
 * Stores the current section name and builds the section-specific menu.
 * Used when user selects a configuration category from main menu.
 */
void ConfigPanel::HandleEnterSection(const std::string &section_name)
{
    log_t("ConfigPanel: Entering section %s", section_name.c_str());
    current_section_name_ = section_name;
    BuildSectionMenu(current_section_name_);
    log_t("ConfigPanel: Section %s menu loaded", current_section_name_.c_str());
}

/**
 * @brief Handles toggling of boolean configuration values
 * @param full_key Full configuration key (section.item format)
 *
 * Retrieves the configuration item, toggles its boolean value,
 * and refreshes the menu to show the updated state.
 */
void ConfigPanel::HandleToggleBoolean(const std::string &full_key)
{
    auto [section_name, item_key] = ParseConfigKey(full_key);
    if (auto section = configuration_manager_->GetConfigSection(section_name))
    {
        for (const auto &config_item : section->items)
        {
            if (config_item.key == item_key)
            {
                ShowBooleanToggle(full_key, config_item);
                break;
            }
        }
    }
}

/**
 * @brief Displays options menu for a configuration item
 * @param full_key Full configuration key (section.item format)
 *
 * Retrieves the configuration item and displays appropriate options
 * based on its type (enum, numeric, or string).
 */
void ConfigPanel::HandleShowOptions(const std::string &full_key)
{
    auto [section_name, item_key] = ParseConfigKey(full_key);
    if (auto section = configuration_manager_->GetConfigSection(section_name))
    {
        for (const auto &config_item : section->items)
        {
            if (config_item.key == item_key)
            {
                ShowOptionsMenu(full_key, config_item);
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
void ConfigPanel::HandleSetConfigValue(const std::string &action_param)
{

    size_t colon_pos = action_param.find(':');
    if (colon_pos == std::string::npos)
    {
        log_e("HandleSetConfigValue - Invalid set_config_value format: %s", action_param.c_str());
        return;
    }

    std::string full_key = action_param.substr(0, colon_pos);
    std::string value = action_param.substr(colon_pos + 1);

    auto [section_name, item_key] = ParseConfigKey(full_key);

    if (auto section = configuration_manager_->GetConfigSection(section_name))
    {
        for (const auto &config_item : section->items)
        {
            if (config_item.key == item_key)
            {
                // Parse the string value into the correct type based on existing value
                config::ConfigValue new_value = configuration_manager_->FromString(value, config_item.value);
                log_t("ConfigPanel: Applying %s setting %s", full_key.c_str(), value.c_str());

                // Update the configuration
                configuration_manager_->UpdateConfig(full_key, new_value);

                // Go back to section menu
                BuildSectionMenu(section_name);
                log_t("ConfigPanel: Returned to section %s menu", section_name.c_str());
                return;
            }
        }
        log_e("HandleSetConfigValue - Config item not found for key: %s", item_key.c_str());
    }
    else
    {
        log_e("HandleSetConfigValue - Section not found: %s", section_name.c_str());
    }
}

/**
 * @brief Handles navigation back to previous menu level
 * @param actionParam Optional parameter specifying target menu
 *
 * Navigates back from submenu to section menu or from section to main menu.
 * Uses stored section name to determine appropriate navigation target.
 */
void ConfigPanel::HandleBackAction(const std::string & /*action_param*/)
{

    // If we're in a section menu, go back to main menu
    if (!current_section_name_.empty())
    {
        current_section_name_.clear(); // Clear the current section
        BuildDynamicMenus();
    }
    else
    {
        // Already at main menu, go back to main menu anyway
        BuildDynamicMenus();
    }
}

/**
 * @brief Handles exit from configuration panel
 *
 * Asks the screen manager to load the user panel (ExitConfig).
 */
clarity::core::NavIntent ConfigPanel::HandlePanelExit()
{
    log_t("ConfigPanel: Executing exit function");
    return clarity::core::NavIntent::ExitConfig;
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

    if (!configuration_manager_)
    {
        log_e("Cannot build dynamic menus - preference service not available");
        return;
    }

    // Get all registered sections
    auto section_names = configuration_manager_->GetRegisteredSectionNames();

    // Build main menu dynamically
    menu_items_.clear();

    // Add sections as menu items
    for (const auto &section_name : section_names)
    {
        if (auto section = configuration_manager_->GetConfigSection(section_name))
        {
            MenuItem item;
            item.label = section->displayName;
            item.actionType = ui_strings::action_types::ENTER_SECTION;
            item.actionParam = section_name;
            menu_items_.push_back(item);
        }
    }

    // Add exit option
    menu_items_.push_back({ui_strings::menu_labels::EXIT, ui_strings::action_types::PANEL_EXIT, ""});

    ShowMenu(ui_strings::menu_labels::CONFIGURATION);

    current_menu_index_ = 0;
    log_i("Built dynamic menu with %zu sections", section_names.size());
}

/**
 * @brief Builds menu for a specific configuration section
 * @param section_name Name of the section to build menu for
 *
 * Creates menu items for each configuration item in the section.
 * Boolean items use toggle action, others show options menu.
 * Adds back option to return to main menu.
 */
void ConfigPanel::BuildSectionMenu(const std::string &section_name)
{
    log_v("BuildSectionMenu() called for section: %s", section_name.c_str());

    auto section = configuration_manager_->GetConfigSection(section_name);
    if (!section)
    {
        log_e("Section not found: %s", section_name.c_str());
        return;
    }

    menu_items_.clear();

    // Add items from the section
    for (const auto &item : section->items)
    {
        MenuItem menu_item;
        menu_item.label = FormatItemLabel(item);

        // Create action based on item type
        std::string full_key = section_name + "." + item.key;

        // For booleans, use direct toggle. For everything else, show options
        if (std::holds_alternative<bool>(item.value))
        {
            menu_item.actionType = ui_strings::action_types::TOGGLE_BOOLEAN;
        }
        else
        {
            menu_item.actionType = ui_strings::action_types::SHOW_OPTIONS;
        }
        menu_item.actionParam = full_key;
        menu_items_.push_back(menu_item);
    }

    // Add back option
    menu_items_.push_back({ui_strings::menu_labels::BACK, ui_strings::action_types::BACK, ""});

    ShowMenu(section->displayName);

    current_menu_index_ = 0;
}

/**
 * @brief Formats configuration item label with current value
 * @param item Configuration item to format
 * @return Formatted string "DisplayName: Value"
 *
 * Creates user-friendly label showing both item name and current value.
 * Used in section menus to display configuration state.
 */
std::string ConfigPanel::FormatItemLabel(const config::ConfigItem &item) const
{
    std::string value_str = configuration_manager_->ToString(item.value);
    return item.displayName + ": " + value_str;
}

/**
 * @brief Displays options menu for configuration item selection
 * @param full_key Full configuration key (section.item format)
 * @param item Configuration item to show options for
 *
 * Main coordinator that delegates to specialized handlers based on item type.
 * Handles enum options, numeric ranges, and string values appropriately.
 * Updates UI with generated options and back navigation.
 */
void ConfigPanel::ShowOptionsMenu(const std::string &full_key, const config::ConfigItem &item)
{
    log_i("ShowOptionsMenu() for key: %s", full_key.c_str());

    // Make a copy of full_key to avoid reference invalidation when menu_items_ is cleared: callers pass a
    // reference into menu_items_, so a reference here would dangle after clear()
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const std::string key_str = full_key;

    menu_items_.clear();

    // Parse constraints to get available options
    auto options = ParseOptions(item.metadata.constraints);

    // Delegate to appropriate specialized method based on data type and available options
    if (!options.empty())
    {
        ShowEnumOptionsMenu(key_str, item, options);
    }
    else if (configuration_manager_->IsNumeric(item.value))
    {
        ShowNumericOptionsMenu(key_str, item);
    }
    else
    {
        ShowStringOptionsMenu(key_str, item);
    }

    // Add back option
    menu_items_.push_back({ui_strings::menu_labels::BACK, ui_strings::action_types::BACK, ""});

    ShowMenu(item.displayName);

    current_menu_index_ = 0;
    log_t("ConfigPanel: Options menu for %s loaded", key_str.c_str());
}

/**
 * @brief Toggles boolean configuration value immediately
 * @param full_key Full configuration key (section.item format)
 * @param item Boolean configuration item to toggle
 *
 * Directly toggles boolean value without showing options menu.
 * Updates configuration and refreshes section menu to show new state.
 */
void ConfigPanel::ShowBooleanToggle(const std::string &full_key, const config::ConfigItem &item)
{
    log_i("ShowBooleanToggle() for key: %s", full_key.c_str());

    if (auto current_value = configuration_manager_->GetValue<bool>(item.value))
    {
        bool new_value = !(*current_value);
        configuration_manager_->UpdateConfig(full_key, new_value);

        // Refresh the section menu to show updated value
        auto [section_name, item_key] = ParseConfigKey(full_key);
        BuildSectionMenu(section_name);

        log_i("Toggled %s from %s to %s", full_key.c_str(), *current_value ? "true" : "false",
              new_value ? "true" : "false");
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
std::vector<std::string> ConfigPanel::ParseOptions(const std::string &constraints) const
{
    std::vector<std::string> options;
    if (constraints.empty())
    {
        return options;
    }

    std::stringstream ss(constraints);
    std::string option;

    while (std::getline(ss, option, ','))
    {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        if (!option.empty())
        {
            options.push_back(option);
        }
    }

    return options;
}

/**
 * @brief Splits configuration key into section and item components
 * @param full_key Full configuration key in "section.item" format
 * @return Pair of (section, item) strings
 *
 * Separates section name from item key at the dot delimiter.
 * Returns empty section if no dot is found.
 */
std::pair<std::string, std::string> ConfigPanel::ParseConfigKey(const std::string &full_key) const
{
    size_t dot_pos = full_key.find('.');
    if (dot_pos == std::string::npos)
    {
        return {"", full_key};
    }
    return {full_key.substr(0, dot_pos), full_key.substr(dot_pos + 1)};
}

// ========== Extracted ShowOptionsMenu Helper Methods ==========

/**
 * @brief Shows menu for enum-type configuration with discrete options
 * @param full_key Full configuration key for action handling
 * @param item Configuration item being configured
 * @param options Vector of valid option strings
 *
 * Creates menu items for each option with selection indicator.
 * Current value is marked with arrow prefix for visual feedback.
 */
void ConfigPanel::ShowEnumOptionsMenu(const std::string &full_key, const config::ConfigItem &item,
                                      const std::vector<std::string> &options)
{
    std::string current_value = configuration_manager_->ToString(item.value);

    for (const auto &option : options)
    {
        bool is_selected = (option == current_value);
        MenuItem menu_item = CreateMenuItemWithSelection(option, full_key, option, is_selected);
        menu_items_.push_back(menu_item);
    }
}

/**
 * @brief Shows menu for numeric configuration with generated values
 * @param full_key Full configuration key for action handling
 * @param item Numeric configuration item (integer or float)
 *
 * Parses numeric range, generates appropriate step values, and creates
 * menu items with formatted values and units. Includes current value,
 * min/max bounds, and calculated increment options.
 */
void ConfigPanel::ShowNumericOptionsMenu(const std::string &full_key, const config::ConfigItem &item)
{
    // Parse range constraints and get current value
    NumericRange range = ParseNumericRange(item);

    // Generate appropriate numeric values based on range and current value
    std::vector<float> values = GenerateNumericValues(range, item);

    // Create menu items for each value
    for (float value : values)
    {
        std::string value_str = FormatNumericValue(value, item);
        bool is_selected = (value == range.currentValue);
        MenuItem menu_item =
            CreateMenuItemWithSelection(value_str, full_key, std::to_string(value), is_selected);
        menu_items_.push_back(menu_item);
    }
}

/**
 * @brief Shows display-only menu for string configuration
 * @param full_key Full configuration key (unused for strings)
 * @param item String configuration item
 *
 * Displays current string value in read-only format.
 * String editing not supported through menu interface.
 */
void ConfigPanel::ShowStringOptionsMenu(const std::string & /*full_key*/, const config::ConfigItem &item)
{
    // For string types without options, just show current value
    MenuItem current_item;
    current_item.label = ui_strings::config_ui::CURRENT_LABEL_PREFIX + configuration_manager_->ToString(item.value);
    current_item.actionType = ui_strings::config_ui::ACTION_TYPE_NONE;
    current_item.actionParam = ui_strings::config_ui::EMPTY_PARAM;
    menu_items_.push_back(current_item);
}

/**
 * @brief Creates menu item with selection indicator
 * @param label Display text for the menu item
 * @param full_key Configuration key for action parameter
 * @param value Value to set when item is selected
 * @param isSelected Whether this item represents current value
 * @return Configured MenuItem struct
 *
 * Centralizes menu item creation with consistent selection marking.
 * Selected items are prefixed with arrow indicator.
 */
ConfigPanel::MenuItem ConfigPanel::CreateMenuItemWithSelection(const std::string &label,
                                                                   const std::string &full_key,
                                                                   const std::string &value, bool is_selected) const
{
    MenuItem menu_item;

    // Show marker for current selection
    if (is_selected)
    {
        menu_item.label = ui_strings::config_ui::SELECTED_MENU_PREFIX + label;
    }
    else
    {
        menu_item.label = ui_strings::config_ui::UNSELECTED_MENU_PREFIX + label;
    }

    menu_item.actionType = ui_strings::action_types::SET_CONFIG_VALUE;
    menu_item.actionParam = full_key + ":" + value;

    return menu_item;
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
ConfigPanel::NumericRange ConfigPanel::ParseNumericRange(const config::ConfigItem &item) const
{
    NumericRange range{};
    range.minValue = 0;
    range.maxValue = 100;
    range.currentValue = std::stof(configuration_manager_->ToString(item.value));

    if (!item.metadata.constraints.empty())
    {
        std::string constraints = item.metadata.constraints;
        // Replace comma with dash for consistent parsing
        std::replace(constraints.begin(), constraints.end(), ',', '-');
        size_t dash_pos = constraints.find('-');
        if (dash_pos != std::string::npos)
        {
            range.minValue = std::stof(constraints.substr(0, dash_pos));
            range.maxValue = std::stof(constraints.substr(dash_pos + 1));
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
std::vector<float> ConfigPanel::GenerateNumericValues(const NumericRange &range, const config::ConfigItem &item) const
{
    // Generate reasonable step options
    float range_size = range.maxValue - range.minValue;
    float small_step = range_size / 20.0f; // 5% of range
    float large_step = range_size / 4.0f;  // 25% of range

    // For integers, round steps
    if (std::holds_alternative<int>(item.value))
    {
        small_step = std::max(1.0f, std::round(small_step));
        large_step = std::max(5.0f, std::round(large_step));
    }

    // Generate option values
    std::vector<float> values;

    // Add preset values
    if (range.currentValue - large_step >= range.minValue)
    {
        values.push_back(range.currentValue - large_step);
    }
    if (range.currentValue - small_step >= range.minValue)
    {
        values.push_back(range.currentValue - small_step);
    }
    values.push_back(range.currentValue); // Current value
    if (range.currentValue + small_step <= range.maxValue)
    {
        values.push_back(range.currentValue + small_step);
    }
    if (range.currentValue + large_step <= range.maxValue)
    {
        values.push_back(range.currentValue + large_step);
    }

    // Also add min and max if not already included
    if (std::find(values.begin(), values.end(), range.minValue) == values.end())
    {
        values.insert(values.begin(), range.minValue);
    }
    if (std::find(values.begin(), values.end(), range.maxValue) == values.end())
    {
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
std::string ConfigPanel::FormatNumericValue(float value, const config::ConfigItem &item) const
{
    std::string value_str;

    if (std::holds_alternative<int>(item.value))
    {
        value_str = std::to_string(static_cast<int>(value));
    }
    else
    {
        // Format float with 1 decimal place
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(value));
        value_str = buffer;
    }

    // Add units if specified
    if (!item.metadata.unit.empty())
    {
        value_str += ui_strings::config_ui::UNIT_SEPARATOR + item.metadata.unit;
    }

    return value_str;
}
