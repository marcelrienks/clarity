#pragma once

#include "clarity/core/types.h"
#include "definitions/constants.h"
#include "ui/icon_view.h"
#include "ui/panel.h"

/**
 * @class LockPanel
 * @brief Lock status panel: a centred lock icon in the engaged colour
 *
 * @details A long-lived Panel (A-05) with one IconView. The screen manager shows it while the lock line is
 * active, so the icon is always the engaged colour.
 */
class LockPanel : public Panel
{
  public:
    static constexpr const char *NAME = panel_names::LOCK;

    /// Long press opens Config
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

  protected:
    void Build(lv_obj_t *screen, Theme &theme) override;

  private:
    IconView icon_;
};
