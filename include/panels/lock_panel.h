#pragma once

#include "components/lock_component.h"
#include "definitions/constants.h"
#include "panels/base_panel.h"
/**
 * @class LockPanel
 * @brief Lock status monitoring panel
 *
 * @details This panel displays the lock status using a centered lock icon.
 * It provides a simple, clean interface for monitoring lock state.
 *
 * @presenter_role Coordinates LockComponent with LockSensor data
 * @data_source LockSensor providing boolean lock status
 * @update_strategy Simple boolean state updates without animation
 *
 * @ui_layout:
 * - Lock Icon: Center of screen with lock-alt-solid icon
 * - Icon changes color based on lock status (white=engaged, red=disengaged)
 *
 * @visual_feedback:
 * - Normal: White lock icon when lock is engaged
 * - Alert: Red lock icon when lock is disengaged
 * - Smooth color transitions for status changes
 *
 * @context This panel provides a dedicated view for lock status monitoring.
 * It's designed to be simple and clear, focusing on the lock status indication.
 */
class LockPanel : public BasePanel
{
  public:
    // ========== Constructors and Destructor ==========
    LockPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
              IPanelManager *panel_manager = nullptr);

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::LOCK;

  protected:
    // ========== Protected Methods ==========
    void CreateContent() override;
    void UpdateContent() override;
    const char *GetPanelName() const override
    {
        return panel_names::LOCK;
    }

    // Optional overrides
    void HandleLongPress() override;

  private:
    // ========== Private Data Members ==========
    LockComponent lock_component_;
    bool component_initialized_ = false;
    bool is_lock_engaged_ = false;
};