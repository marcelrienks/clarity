#pragma once

#include "components/key_component.h"
#include "definitions/constants.h"
#include "panels/base_panel.h"
/**
 * @class KeyPanel
 * @brief Key status monitoring panel
 *
 * @details This panel displays the key/ignition status using a centered key icon.
 * It provides a simple, clean interface for monitoring key presence or ignition state.
 *
 * @presenter_role Coordinates KeyComponent with interrupt system data
 * @data_source Key status from interrupt-driven architecture
 * @update_strategy Simple boolean state updates without animation
 *
 * @ui_layout:
 * - Key Icon: Center of screen with key-solid icon
 * - Icon changes color based on key status (white=present, red=absent)
 *
 * @visual_feedback:
 * - Normal: White key icon when key is present/ignition on
 * - Alert: Red key icon when key is absent/ignition off
 * - Smooth color transitions for status changes
 *
 * @context This panel provides a dedicated view for key status monitoring.
 * It's designed to be simple and clear, focusing on the key status indication.
 */
class KeyPanel : public BasePanel
{
  public:
    // ========== Constructors and Destructor ==========
    KeyPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
             IPanelManager *panel_manager = nullptr);

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::KEY;

  protected:
    // ========== Protected Methods ==========
    void CreateContent() override;
    void UpdateContent() override;
    const char *GetPanelName() const override
    {
        return panel_names::KEY;
    }

    // Optional overrides
    void HandleLongPress() override;

  private:
    // ========== Private Methods ==========
    KeyState DetermineCurrentKeyState();

    // ========== Private Data Members ==========
    KeyComponent key_component_;
    bool component_initialized_ = false;
    KeyState current_key_state_ = KeyState::Inactive;
};