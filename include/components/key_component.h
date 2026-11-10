#pragma once

#include "definitions/types.h"
#include "interfaces/i_component.h"
#include "interfaces/i_style_manager.h"

#include <lvgl.h>

/**
 * @class KeyComponent
 * @brief Key/ignition status indicator component
 *
 * @details This component displays a key icon to indicate the ignition or
 * key status of the vehicle. It provides visual feedback for key-related
 * states and conditions.
 *
 * @view_role Renders key status icon with theme-aware styling
 * @ui_elements Key icon with conditional styling based on status
 * @positioning Supports all ComponentLocation alignment options
 *
 * @data_source Receives key status data from interrupt system
 * @visual_states Normal, active, warning states with color changes
 * @icon_resource Uses lock-alt-solid icon from icon resources
 *
 * @context This component shows key/ignition status. It's part of
 * the broader vehicle monitoring system and could be used in various
 * panels for status indication.
 */
class KeyComponent : public IComponent
{
  public:
    // ========== Constructors and Destructor ==========
    explicit KeyComponent(IStyleManager *style_manager);
    virtual ~KeyComponent();

    // ========== Public Interface Methods ==========
    void Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display) override;
    void SetColor(KeyState key_state);

  protected:
    // ========== Protected Data Members ==========
    lv_obj_t *key_icon_;
    IStyleManager *style_manager_;

  private:
    // ========== Private Methods ==========
};