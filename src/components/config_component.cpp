#include "components/config_component.h"
#include "definitions/constants.h"
#include "managers/error_manager.h"
#include "ui/theme.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs a configuration menu component with service dependencies
 * @param panelManager Panel service for navigation control
 * @param theme Theme for the menu colours
 *
 * Initializes the configuration component with dependencies for panel navigation
 * and theme styling. These services enable menu interactions and visual theming.
 */
ConfigComponent::ConfigComponent(IPanelManager *panel_manager, Theme *theme)
    : panel_manager_(panel_manager), theme_(theme)
{
    log_v("ConfigComponent constructor called");
}

// ========== IComponent Implementation ==========

/**
 * @brief Renders the configuration component on screen
 * @param screen LVGL screen object to render on
 * @param location Component positioning parameters
 * @param display Display provider interface
 *
 * Main rendering method that creates and displays the configuration UI.
 * Delegates to Init() for actual UI creation. Part of IComponent interface.
 */
void ConfigComponent::Render(lv_obj_t *screen, const ComponentLocation & /*location*/, IDisplayProvider * /*display*/)
{
    log_v("Render() called");
    Init(screen);
}

/**
 * @brief Sets component value (not used for configuration menu)
 * @param value Integer value to set
 *
 * This method is part of IComponent interface but is not applicable
 * for configuration menu which uses specific methods for updates.
 */
void ConfigComponent::SetValue(int32_t /*value*/)
{
    log_v("SetValue() called");
    // Not used for config menu - updates handled via specific methods
}

// ========== Public Interface Methods ==========

/**
 * @brief Executes configuration menu actions
 * @param actionType Type of action to execute (submenu, panel_exit, panel_load)
 * @param actionParam Parameter for the action (e.g., panel name)
 *
 * Handles menu navigation actions including submenu entry, panel exit to restoration,
 * and direct panel loading. Used by ConfigPanel for menu interaction handling.
 */
void ConfigComponent::ExecuteAction(const std::string &action_type, const std::string &action_param)
{
    log_v("ExecuteAction() called with type: %s, param: %s", action_type.c_str(), action_param.c_str());

    if (action_type == ui_strings::action_types::SUBMENU)
    {
        // Handle submenu navigation - delegate back to config panel
        // This would require a callback to the ConfigPanel
    }
    else if (action_type == ui_strings::action_types::PANEL_EXIT)
    {
        // Exit config panel and return to restoration panel
        if (panel_manager_)
        {
            const char *restoration_panel = panel_manager_->GetRestorationPanel();
            panel_manager_->CreateAndLoadPanel(restoration_panel, true);
        }
    }
    else if (action_type == ui_strings::action_types::PANEL_LOAD)
    {
        // Load specific panel
        if (panel_manager_)
        {
            panel_manager_->CreateAndLoadPanel(action_param.c_str(), true);
        }
    }
    else
    {
        log_w("Unknown action type: %s", action_type.c_str());
    }
}

// ========== Private Methods ==========

/**
 * @brief Initializes the configuration component UI structure
 * @param screen LVGL screen object to create UI on
 *
 * Creates the main container with circular styling for the round display,
 * sets up transparent background, and creates all UI elements including
 * title, menu items, and hint text.
 */
void ConfigComponent::Init(lv_obj_t *screen)
{
    log_v("Init() called");

    if (!screen)
    {
        log_e("ConfigComponent requires screen object");
        ErrorManager::Instance().ReportCriticalError("ConfigComponent",
                                                     "Screen object is null - cannot display configuration menu");
        return;
    }
    screen_ = screen;

    // Create main container using full 240x240 screen size (like ErrorComponent)
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, 240, 240); // Full screen size
    lv_obj_align(container_, LV_ALIGN_CENTER, 0, 0);

    // Apply circular styling to container, not screen
    lv_obj_set_style_radius(container_, 120, LV_PART_MAIN); // Make it circular (half of 240)

    // Default transparent background (UpdateThemeColors() sets the theme's menu background)
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, LV_PART_MAIN);

    CreateUI();
}

/**
 * @brief Sets the title text of the configuration menu
 * @param title Title string to display
 *
 * Updates the title label at the top of the configuration menu.
 * Used to indicate the current menu level or section.
 */
void ConfigComponent::SetTitle(const std::string &title)
{
    log_v("SetTitle() called");
    current_title_ = title;
    if (title_label_)
    {
        lv_label_set_text(title_label_, title.c_str());
    }
}

/**
 * @brief Sets the menu items to display
 * @param items Vector of MenuItem structures with labels and actions
 *
 * Updates the menu with new items and refreshes the display.
 * Each MenuItem contains a label and associated action information.
 */
void ConfigComponent::SetMenuItems(const std::vector<MenuItem> &items)
{
    log_v("SetMenuItems() called");
    menu_items_ = items;
    UpdateMenuDisplay();
}

/**
 * @brief Sets the currently selected menu item index
 * @param index Zero-based index of the item to select
 *
 * Updates the selected menu item and refreshes the display to show
 * the selection with proper highlighting and positioning.
 */
void ConfigComponent::SetCurrentIndex(size_t index)
{
    log_v("SetCurrentIndex() called");
    if (index < menu_items_.size())
    {
        current_index_ = index;
        UpdateMenuDisplay();
    }
}

/**
 * @brief Sets the hint text at the bottom of the menu
 * @param hint Hint text to display
 *
 * Updates the hint label that shows button instructions or
 * contextual help for the current menu state.
 */
void ConfigComponent::SetHintText(const std::string &hint)
{
    log_v("SetHintText() called");
    if (hint_label_)
    {
        lv_label_set_text(hint_label_, hint.c_str());
    }
}

/**
 * @brief Sets the theme for the menu colours
 * @param theme Pointer to the theme
 *
 * Updates the theme reference and immediately applies
 * the current theme colors to all UI elements.
 */
void ConfigComponent::SetTheme(Theme *theme)
{
    log_v("ConfigComponent::SetTheme() called");
    theme_ = theme;
    UpdateThemeColors();
}

/**
 * @brief Updates all UI elements with current theme colors
 *
 * Applies theme-specific colors to container background, title,
 * hint text, and menu items. Handles both day and night themes
 * with appropriate color schemes.
 */
void ConfigComponent::UpdateThemeColors()
{
    log_v("UpdateThemeColors() called");

    if (!theme_)
    {
        log_w("No theme available for theme update");
        return;
    }

    const Palette &colours = theme_->Colors();

    if (container_)
    {
        lv_obj_set_style_bg_color(container_, colours.menuBackground, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, LV_PART_MAIN);
    }

    if (title_label_)
    {
        lv_obj_set_style_text_color(title_label_, colours.menuTitle, LV_PART_MAIN);
    }

    if (hint_label_)
    {
        lv_obj_set_style_text_color(hint_label_, colours.menuHint, LV_PART_MAIN);
    }

    // Update menu display to refresh all menu item colors
    UpdateMenuDisplay();
}

/**
 * @brief Calculates gradient color for menu items based on theme and position
 * @param distanceFromCenter Distance from the center selected item
 * @param isSelected Whether this is the selected item
 * @return LVGL color for the menu item
 *
 * Creates a gradient effect where items further from center are darker.
 * Uses red gradients for night theme and gray gradients for day theme.
 */
lv_color_t ConfigComponent::GetThemeGradientColor(int distance_from_center, bool is_selected) const
{
    // Fallback to the Day colours if no theme is set
    const Palette &colours = theme_ ? theme_->Colors() : PaletteFor(clarity::core::ThemeId::Day);
    if (is_selected)
    {
        return colours.menuSelectedItem;
    }

    // Reduce intensity based on distance (further = darker)
    float intensity = 1.0f - (static_cast<float>(distance_from_center) * 0.3f);
    if (intensity < 0.2f)
    {
        intensity = 0.2f; // Minimum visibility
    }

    const lv_color_t base = colours.menuItem;
    return lv_color_make(static_cast<uint8_t>(static_cast<float>(base.red) * intensity),
                         static_cast<uint8_t>(static_cast<float>(base.green) * intensity),
                         static_cast<uint8_t>(static_cast<float>(base.blue) * intensity));
}

// ========== Helper Methods ==========

/**
 * @brief Creates all UI elements for the configuration menu
 *
 * Builds the complete UI structure including title label, menu container,
 * menu item labels array, and hint text. Sets up initial styling and
 * positioning for all elements.
 */
void ConfigComponent::CreateUI()
{
    log_v("CreateUI() called");

    // Note: Circular styling and background theme are now applied to container in Init()

    // Create title label (child of container, not screen)
    title_label_ = lv_label_create(container_);
    lv_label_set_text(title_label_, current_title_.c_str());
    lv_obj_set_style_text_color(title_label_, PaletteFor(clarity::core::ThemeId::Day).menuTitle,
                                LV_PART_MAIN); // Default
    lv_obj_set_style_text_font(title_label_, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title_label_, LV_ALIGN_TOP_MID, 0, 15);

    // Create menu container - covers most of the screen for round display (child of container)
    menu_container_ = lv_obj_create(container_);
    lv_obj_set_size(menu_container_, 240, 180); // Full width, most height
    lv_obj_align(menu_container_, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(menu_container_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(menu_container_, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(menu_container_, 0, LV_PART_MAIN);
    lv_obj_clear_flag(menu_container_, LV_OBJ_FLAG_SCROLLABLE);

    // Create menu item labels
    menu_labels_.clear();
    for (int i = 0; i < VISIBLE_ITEMS; ++i)
    {
        lv_obj_t *item = lv_label_create(menu_container_);
        lv_obj_set_width(item, MENU_WIDTH);
        lv_obj_set_style_text_align(item, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

        // Position items vertically
        int y_offset = -60 + (i * ITEM_HEIGHT);
        lv_obj_align(item, LV_ALIGN_CENTER, 0, y_offset);

        // Set initial style
        lv_obj_set_style_text_font(item, &lv_font_montserrat_16, LV_PART_MAIN);

        menu_labels_.push_back(item);
    }

    // Create hint label (child of container, not screen)
    hint_label_ = lv_label_create(container_);
    lv_label_set_text(hint_label_, ui_strings::hint_text::SHORT_LONG_PRESS);
    lv_obj_set_style_text_color(hint_label_, PaletteFor(clarity::core::ThemeId::Day).menuHint, LV_PART_MAIN); // Default
    lv_obj_set_style_text_font(hint_label_, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(hint_label_, LV_ALIGN_BOTTOM_MID, 0, -15);

    // Initial display update
    UpdateMenuDisplay();
}

/**
 * @brief Updates the menu display with current items and selection
 *
 * Refreshes all visible menu items with scrolling effect, applies
 * distance-based styling including font size, opacity, and color gradients.
 * Handles circular scrolling and item positioning adjustments.
 */
void ConfigComponent::UpdateMenuDisplay()
{
    log_v("UpdateMenuDisplay() called");

    if (menu_items_.empty() || menu_labels_.empty())
    {
        log_w("Cannot update menu display - empty menu items or labels");
        return;
    }
    // Update menu items with scrolling effect
    for (int i = 0; i < VISIBLE_ITEMS && i < static_cast<int>(menu_labels_.size()); ++i)
    {
        // Calculate which menu item to show at this position
        int menu_item_index =
            (static_cast<int>(current_index_) - CENTER_INDEX + i + static_cast<int>(menu_items_.size())) %
            static_cast<int>(menu_items_.size());

        if (menu_item_index >= 0 && menu_item_index < static_cast<int>(menu_items_.size()))
        {
            lv_label_set_text(menu_labels_[i], menu_items_[menu_item_index].label.c_str());

            // Calculate distance from center
            int distance_from_center = abs(i - CENTER_INDEX);

            // Apply fading effect based on distance from center
            if (i == CENTER_INDEX)
            {
                ApplyCenterItemStyle(menu_labels_[i]);
            }
            else
            {
                // Apply progressive fading and font size reduction based on distance
                uint8_t opacity = LV_OPA_100 - (distance_from_center * 35);

                // Use theme-appropriate gradient colors
                lv_obj_set_style_text_color(menu_labels_[i], GetThemeGradientColor(distance_from_center), LV_PART_MAIN);

                // Progressive font size reduction - more dramatic stepping
                const lv_font_t *font = nullptr;
                switch (distance_from_center)
                {
                    case 1:
                        font = &lv_font_montserrat_14;
                        break;
                    case 2:
                        font = &lv_font_montserrat_12;
                        break;
                    default:
                        font = &lv_font_montserrat_8;
                        break;
                }
                lv_obj_set_style_text_font(menu_labels_[i], font, LV_PART_MAIN);
                lv_obj_set_style_text_opa(menu_labels_[i], opacity, LV_PART_MAIN);

                // Remove background and border from non-selected items
                lv_obj_set_style_bg_opa(menu_labels_[i], LV_OPA_0, LV_PART_MAIN);
                lv_obj_set_style_border_width(menu_labels_[i], 0, LV_PART_MAIN);
                lv_obj_set_style_pad_all(menu_labels_[i], 0, LV_PART_MAIN);
            }

            // Apply distance-based positioning with adjusted padding
            int base_y_offset = -60 + (i * ITEM_HEIGHT);
            int adjusted_y_offset = base_y_offset;

            // Adjust positioning based on distance from center
            if (distance_from_center == 1)
            {
                // Adjacent settings - increase gap from selected item (bigger breathing room)
                adjusted_y_offset = base_y_offset + (i < CENTER_INDEX ? -6 : 6);
            }
            else if (distance_from_center >= 2)
            {
                // Outermost settings - compress slightly toward center
                adjusted_y_offset = base_y_offset + (i < CENTER_INDEX ? -4 : 4);
            }

            lv_obj_align(menu_labels_[i], LV_ALIGN_CENTER, 0, adjusted_y_offset);

            // Show the label
            lv_obj_clear_flag(menu_labels_[i], LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            // Hide labels that don't have corresponding menu items
            lv_obj_add_flag(menu_labels_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/**
 * @brief Applies highlighting style to the selected center item
 * @param label LVGL label object to style
 *
 * Applies enhanced styling to the selected menu item including larger font,
 * full opacity, background highlight with border, and theme-specific colors.
 */
void ConfigComponent::ApplyCenterItemStyle(lv_obj_t *label)
{
    if (!label)
    {
        log_e("ConfigComponent::ApplyCenterItemStyle: Label is null - cannot apply center item styling!");
        ErrorManager::Instance().ReportCriticalError("ConfigComponent",
                                                     "Label object is null - config menu styling is broken");
        return;
    }

    // Center item - fully highlighted with bold styling
    lv_obj_set_style_text_color(label, GetThemeGradientColor(0, true), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_100, LV_PART_MAIN);

    // Enhanced background highlight
    ApplyCenterItemBackground(label);

    lv_obj_set_style_bg_opa(label, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_radius(label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(label, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(label, 1, LV_PART_MAIN);
}

/**
 * @brief Applies background color to selected item based on theme
 * @param label LVGL label object to style
 *
 * Sets background and border colors for the selected menu item.
 * Uses dark red for night theme and gray for day theme.
 */
void ConfigComponent::ApplyCenterItemBackground(lv_obj_t *label)
{
    if (!theme_)
    {
        ApplyDefaultCenterBackground(label);
        return;
    }

    const Palette &colours = theme_->Colors();
    lv_obj_set_style_bg_color(label, colours.menuSelectedBackground, LV_PART_MAIN);
    lv_obj_set_style_border_color(label, colours.menuSelectedBorder, LV_PART_MAIN);
}

/**
 * @brief Applies default background when no theme is set
 * @param label LVGL label object to style
 *
 * Fallback styling for selected item when no theme is set.
 * Uses default gray color scheme.
 */
void ConfigComponent::ApplyDefaultCenterBackground(lv_obj_t *label)
{
    const Palette &day = PaletteFor(clarity::core::ThemeId::Day);
    lv_obj_set_style_bg_color(label, day.menuSelectedBackground, LV_PART_MAIN);
    lv_obj_set_style_border_color(label, day.menuSelectedBorder, LV_PART_MAIN);
}
