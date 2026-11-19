#pragma once

#include <cstdint>

#include <lvgl.h>

#include "clarity/core/types.h"
#include "ui/theme.h"

/**
 * @class Panel
 * @brief Base for every screen panel (A-05): one LVGL tree per panel, created on Enter() and deleted on Exit()
 *
 * @details Panels are long-lived: the screen manager constructs each one once and only its LVGL tree is created
 * and destroyed. A panel never switches screens itself. OnButton() returns a NavIntent that the screen manager
 * applies on its next Apply(), so no panel is exited from inside its own call or from an LVGL callback.
 *
 * Swap order: the screen manager calls Enter() on the next panel, which loads its screen, and only then Exit()
 * on the previous one, so the active screen is never the one being deleted.
 *
 * LVGL ownership rules:
 * 1. Only Panel::Exit() deletes LVGL objects, and only screen_. Deleting the screen deletes every child with it.
 * 2. Widgets hold non-owning lv_obj_t pointers, valid from Build() until Exit(). Widgets never delete.
 * 3. Animations use an LVGL object of the panel's tree as their var (for example a gauge's scale), never the
 *    panel or widget object, so LVGL removes them when Exit() deletes the tree.
 * 4. Panels own no lv_timer. Time-based behaviour lives in Update(now).
 *
 * Splash, Key and Lock use this base; Oil, Error and Config are still legacy IPanel panels (rest of 5.5).
 */
class Panel
{
  public:
    Panel() = default;
    Panel(const Panel &) = delete;
    Panel &operator=(const Panel &) = delete;
    /// Does not delete the LVGL tree (rule 1): the owner calls Exit() first
    virtual ~Panel() = default;

    /// Creates screen_ with the theme's background, calls Build() and loads the screen. Does nothing if the
    /// panel is already entered
    void Enter(Theme &theme);

    /// Calls OnExit() and deletes screen_ and everything on it. Does nothing if the panel is not entered.
    /// The screen must no longer be the active one (swap order)
    void Exit();

    /// Called every loop pass while the panel is entered
    virtual void Update(uint32_t now)
    {
        (void)now;
    }

    /// Handles a completed button press and says what the screen manager should do next
    virtual clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event)
    {
        (void)event;
        return clarity::core::NavIntent::None;
    }

    /// False while the panel must not be replaced by an override (the splash until it is done, Q-06)
    virtual bool Finished() const
    {
        return true;
    }

  protected:
    /// Creates the panel's widgets on screen; they stay valid until Exit(). Colours a widget sets locally come
    /// from theme.Colors()
    virtual void Build(lv_obj_t *screen, Theme &theme) = 0;

    /// Called by Exit() before the tree is deleted: reset per-visit state. Delete no LVGL objects here
    virtual void OnExit()
    {
    }

    /// The root of the panel's LVGL tree and its only owner; nullptr while the panel is not entered
    lv_obj_t *screen_ = nullptr;
};
