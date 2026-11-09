#include "components/key_component.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <esp32-hal-log.h>
#include <icons/key_solid.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs a key icon component with style service dependency
 * @param styleManager Style service for theme-based color management
 *
 * Initializes the key component with style service for applying theme colors.
 * Sets key icon pointer to nullptr for later initialization.
 */
KeyComponent::KeyComponent(IStyleManager *styleManager) : keyIcon_(nullptr), styleManager_(styleManager)
{
}

/**
 * @brief Destructor; deletes nothing
 *
 * The key icon is a child of the panel's screen and is deleted with it.
 */
KeyComponent::~KeyComponent() = default;

// ========== IComponent Implementation ==========

/**
 * @brief This method initializes the key present icon with location parameters
 * @param screen The screen object to render the component on.
 * @param location The location parameters for positioning the component.
 */
void KeyComponent::Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display)
{
    
    if (!display)
    {
        log_e("KeyComponent requires display provider");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "KeyComponent",
                                             "Cannot render - display provider is null");
        return;
    }
    keyIcon_ = display->CreateImage(screen);
    lv_image_set_src(keyIcon_, &key_solid);

    // Apply location settings
    lv_obj_align(keyIcon_, location.align, location.x_offset, location.y_offset);
}

// ========== Public Interface Methods ==========

/**
 * @brief Set the key icon color based on key state
 * @param keyState The current key state to determine color
 */
void KeyComponent::SetColor(KeyState keyState)
{
    
    if (!styleManager_ || !keyIcon_)
    {
        return;
    }
    
    lv_color_t colour;
    if (keyState == KeyState::Present)
    {
        colour = styleManager_->GetThemeColors().keyPresent;
        log_t("Key icon color set to GREEN");
    }
    else // KeyState::NotPresent or KeyState::Inactive
    {
        colour = styleManager_->GetThemeColors().keyNotPresent;
        log_t("Key icon color set to RED");
    }

    lv_obj_set_style_image_recolor(keyIcon_, colour, static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
    lv_obj_set_style_image_recolor_opa(keyIcon_, LV_OPA_COVER, static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);
}