#pragma once

#include <cstddef>
#include <cstdint>

#include "clarity/core/error_log.h"
#include "clarity/core/types.h"
#include "definitions/constants.h"
#include "ui/error_view.h"
#include "ui/panel.h"

/**
 * @class ErrorPanel
 * @brief Shows the pending errors one at a time, most severe first
 *
 * @details A long-lived Panel (A-05) over one ErrorView. It reads the application's ErrorLog directly (no copy):
 * Update() picks the most severe entry that has not been viewed and the view redraws only when the log's
 * revision or that entry changed (D-19).
 *
 * A short press views the shown error and moves on: warnings and errors leave the log, a critical error is
 * marked acknowledged and stays until the log is cleared (ErrorLog::DismissAt). Viewing the last one clears the
 * log and returns ClearErrors (Q-12 decides whether that stays); a long press clears everything at once. The
 * screen manager then restores the panel that was shown before.
 */
class ErrorPanel : public Panel
{
  public:
    explicit ErrorPanel(clarity::core::ErrorLog &log);

    static constexpr const char *NAME = panel_names::ERROR;

    void Update(uint32_t now) override;

    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

  protected:
    void Build(lv_obj_t *screen, Theme &theme) override;

  private:
    /// The most severe entry not yet viewed (the first of them on a tie); Count() when there is none
    std::size_t NextToView() const;

    clarity::core::NavIntent HandleShortPress();
    clarity::core::NavIntent HandleLongPress();

    clarity::core::ErrorLog &log_;
    ErrorView view_;
};
