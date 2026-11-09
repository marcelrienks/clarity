#include "components/clarity_component.h"
#include "managers/error_manager.h"
#include "definitions/constants.h"
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs a clarity component with style service dependency
 * @param styleManager Style service for theme and styling management
 *
 * Initializes the clarity component with the provided style service for
 * applying themes and visual styling to the splash screen display.
 */
ClarityComponent::ClarityComponent(IStyleManager *styleManager) : styleManager_(styleManager)
{
    log_v("ClarityComponent constructor called");
}

// ========== IComponent Implementation ==========

/**
 * @brief Initialises the Clarity Component by rendering a splash screen with location parameters
 * @param screen the screen on which to render the component
 * @param location the location parameters for positioning the component
 */
void ClarityComponent::Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display)
{
    log_v("Render() called");
    
    if (!display)
    {
        log_e("ClarityComponent requires display provider");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "ClarityComponent",
                                             "Cannot render - display provider is null");
        return;
    }
    lv_obj_t *splash = display->CreateLabel(screen);
    lv_label_set_text(splash, UIConstants::APP_NAME);

    // Apply the current theme's text style
    lv_obj_add_style(splash, &styleManager_->GetTextStyle(), static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT);

    // Apply location settings
    lv_obj_align(splash, location.align, location.x_offset, location.y_offset);

    // Set font size
    lv_obj_set_style_text_font(splash, &lv_font_montserrat_20, 0);
}