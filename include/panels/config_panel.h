#pragma once

#include "clarity/core/types.h"
#include "definitions/configs.h"
#include "definitions/constants.h"
#include "interfaces/i_configuration_manager.h"
#include "ui/menu_wheel.h"
#include "ui/panel.h"

#include <memory>
#include <string>
#include <vector>

/**
 * @class ConfigPanel
 * @brief Configuration settings panel for system preferences
 *
 * @details This panel provides a single-button navigable interface for
 * configuring system settings. It displays a menu structure similar to
 * the error panel but with grey colors for a settings-appropriate theme.
 *
 * @presenter_role Manages configuration UI and setting persistence
 * @navigation Single button: short press cycles options, long press selects
 * @implementation Initial version with basic configuration options
 *
 * @ui_layout:
 * - Header: "Configuration" title
 * - Menu: List of options with highlight indicator
 * - Footer: Current selection hint
 *
 * @menu_structure:
 * - Option 1 (placeholder)
 * - Option 2 (placeholder)
 * - Exit (returns to previous panel)
 *
 * @visual_style:
 * - Grey color scheme matching system settings theme
 * - Highlighted option with border/background
 * - Clean, minimalist appearance
 *
 * @context This is an initial implementation with basic configuration options.
 * Additional settings and functionality can be added as needed.
 */
class ConfigPanel : public Panel
{
  public:
    explicit ConfigPanel(IConfigurationManager &configuration);

    static constexpr const char *NAME = panel_names::CONFIG;

    /// Follows a theme change while the panel is open (the menu colours are set locally, D-06)
    void Update(uint32_t now) override;

    /// Short press highlights the next item; long press selects it. Returns ExitConfig when Exit is selected
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

  protected:
    /// Shows the main menu: every visit starts at the top
    void Build(lv_obj_t *screen, Theme &theme) override;
    void OnExit() override;

  private:
    // ========== Private Types ==========

    struct MenuItem
    {
        std::string label;
        std::string actionType;  // Type of action: "theme_switch", "panel_load", etc.
        std::string actionParam; // Parameter for the action
    };

    enum class MenuActionType
    {
        ENTER_SECTION,
        TOGGLE_BOOLEAN,
        SHOW_OPTIONS,
        SET_CONFIG_VALUE,
        BACK,
        NONE,
        PANEL_EXIT,
        UNKNOWN
    };

    // ========== Private Methods ==========

    // Dynamic configuration methods
    void BuildDynamicMenus();
    void ShowMenu(const std::string &title);
    void ApplyMenuBackground();
    void BuildSectionMenu(const std::string &section_name);
    std::string FormatItemLabel(const config::ConfigItem &item) const;
    void ShowOptionsMenu(const std::string &full_key, const config::ConfigItem &item);
    void ShowBooleanToggle(const std::string &full_key, const config::ConfigItem &item);

    // Extracted ShowOptionsMenu helpers
    void ShowEnumOptionsMenu(const std::string &full_key, const config::ConfigItem &item,
                             const std::vector<std::string> &options);
    void ShowNumericOptionsMenu(const std::string &full_key, const config::ConfigItem &item);
    void ShowStringOptionsMenu(const std::string &full_key, const config::ConfigItem &item);
    MenuItem CreateMenuItemWithSelection(const std::string &label, const std::string &full_key,
                                                          const std::string &value, bool is_selected) const;

    // Extracted ShowNumericOptionsMenu helpers
    struct NumericRange
    {
        float minValue;
        float maxValue;
        float currentValue;
    };
    NumericRange ParseNumericRange(const config::ConfigItem &item) const;
    std::vector<float> GenerateNumericValues(const NumericRange &range, const config::ConfigItem &item) const;
    std::string FormatNumericValue(float value, const config::ConfigItem &item) const;

    std::vector<std::string> ParseOptions(const std::string &constraints) const;
    std::pair<std::string, std::string> ParseConfigKey(const std::string &full_key) const;

    void HandleShortPress();
    clarity::core::NavIntent HandleLongPress();

    // Helper methods for HandleLongPress
    bool ValidateMenuState() const;
    clarity::core::NavIntent ExecuteMenuAction(const MenuItem &item);
    MenuActionType ParseActionType(const std::string &action_type_str) const;
    void HandleEnterSection(const std::string &section_name);
    void HandleToggleBoolean(const std::string &full_key);
    void HandleShowOptions(const std::string &full_key);
    void HandleSetConfigValue(const std::string &action_param);
    void HandleBackAction(const std::string &action_param);
    clarity::core::NavIntent HandlePanelExit();

    // ========== Private Data Members ==========

    // Instance Data Members
    IConfigurationManager *configuration_manager_ = nullptr;
    Theme *theme_ = nullptr; ///< Valid between Build() and OnExit()

    MenuWheel wheel_;
    clarity::core::ThemeId styled_theme_ = clarity::core::ThemeId::Day; ///< Theme the menu was coloured for

    // Dynamic menu state
    std::vector<MenuItem> menu_items_;
    size_t current_menu_index_ = 0;
    std::string current_section_name_;
};