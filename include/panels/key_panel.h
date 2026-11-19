#pragma once

#include "clarity/core/types.h"
#include "definitions/constants.h"
#include "ui/icon_view.h"
#include "ui/panel.h"

/**
 * @class KeyPanel
 * @brief Key status panel: a centred key icon, green while the key is present, red while it is not
 *
 * @details A long-lived Panel (A-05) with one IconView. The screen manager sets the key state before Enter() and
 * again when the key line changes while the panel is already shown, which recolours the icon in place.
 */
class KeyPanel : public Panel
{
  public:
    static constexpr const char *NAME = panel_names::KEY;

    /// Sets the key state and recolours the icon if the panel is entered; otherwise Enter() applies it
    void SetState(clarity::core::KeyState state);

    /// Long press opens Config
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

  protected:
    void Build(lv_obj_t *screen, Theme &theme) override;
    void OnExit() override;

  private:
    void ApplyColor();

    IconView icon_;
    Theme *theme_ = nullptr; ///< Valid between Build() and OnExit()
    clarity::core::KeyState state_ = clarity::core::KeyState::Inactive;
};
