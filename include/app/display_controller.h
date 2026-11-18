#pragma once

#include <cstdint>

#include "app/inputs.h"
#include "clarity/core/display_policy.h"
#include "clarity/core/types.h"

class PanelManager;
class Theme;

/**
 * @class DisplayController
 * @brief Turns the sampled inputs into panel requests, theme changes and button dispatch (A-02)
 *
 * @details It holds no input state of its own beyond what it needs to see an edge: every pass it compares
 * clarity::core::ResolveDisplay() with the request it last acted on and records a transition with the
 * PanelManager when they differ. The transition is applied later by PanelManager::ApplyPendingTransition(),
 * never from here (D-13).
 *
 * Button events, the debug error edge and the lights theme are acted on in the pass they arrive, even while a
 * panel animates (Q-07). Key and lock requests wait while the splash runs (Q-06) and are acted on once its
 * target panel is loaded. While the error panel is shown, key and lock requests are suppressed but tracked,
 * and are applied by PanelManager::RestoreNow() through ApplyOverride() once the errors are gone
 * (restructure.md 2.2).
 */
class DisplayController
{
  public:
    DisplayController(PanelManager &panels, Theme &theme) noexcept : panels_(panels), theme_(theme)
    {
    }

    /// Evaluate the seeded inputs once, before the first panel decision. Returns true if a key or lock
    /// override was requested, in which case the default panel (and its splash) must be skipped.
    bool Begin(const Inputs &inputs);

    /// Call once per loop pass after Inputs::Sample().
    void Update(const Inputs &inputs);

    /// Called by PanelManager::RestoreNow(): request the key or lock override that is still active, and
    /// apply the theme the lights signal asks for (replacing Error). Returns true if an override panel was
    /// requested.
    bool ApplyOverride();

  private:
    void ApplyTheme(const clarity::core::VehicleInputs &in);
    void RequestOverride(const clarity::core::DisplayRequest &request);
    void ExecuteButton(clarity::core::ButtonEvent event);
    void ReportDebugErrors();

    PanelManager &panels_;
    Theme &theme_;

    clarity::core::VehicleInputs vehicle_{};
    clarity::core::DisplayRequest applied_{clarity::core::OverridePanel::None, clarity::core::KeyState::Inactive};
    bool applied_lights_ = false; ///< Lights state the theme was last set for (the theme starts on Day)
};
