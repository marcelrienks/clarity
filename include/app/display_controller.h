#pragma once

#include <cstdint>

#include "app/inputs.h"
#include "clarity/core/display_policy.h"
#include "clarity/core/types.h"

class PanelManager;
class IStyleManager;

/**
 * @class DisplayController
 * @brief Turns the sampled inputs into panel requests, theme changes and button dispatch (A-02)
 *
 * @details Replaces InterruptManager::Process(). It holds no input state of its own beyond what it needs
 * to see an edge: every pass it compares clarity::core::ResolveDisplay() with the request it last acted on
 * and records a transition with the PanelManager when they differ. The transition is applied later by
 * PanelManager::ApplyPendingTransition(), never from here (D-13).
 *
 * While the UI is busy (an animation or a load in progress) nothing is acted on and the button event and
 * debug error edge are kept, latest wins, until the UI is idle again. Removing that wait is Q-07 (4.5).
 * While the error panel is shown, key and lock requests are suppressed but tracked, and are applied by
 * PanelManager::RestoreNow() through ApplyOverride() once the errors are gone (restructure.md 2.2).
 */
class DisplayController
{
  public:
    DisplayController(PanelManager &panels, IStyleManager &style) noexcept : panels_(panels), style_(style)
    {
    }

    /// Evaluate the seeded inputs once, before the first panel decision. Returns true if a key or lock
    /// override was requested, in which case the default panel (and its splash) must be skipped.
    bool Begin(const Inputs &inputs);

    /// Call once per loop pass after Inputs::Sample(). `ui_idle` is the old UIState gate.
    void Update(const Inputs &inputs, bool ui_idle);

    /// Called by PanelManager::RestoreNow(): request the key or lock override that is still active, and
    /// apply the theme the lights signal asks for. Returns true if an override panel was requested.
    bool ApplyOverride();

  private:
    void ApplyLightsTheme(bool lights);
    void RequestOverride(const clarity::core::DisplayRequest &request);
    void ExecuteButton(clarity::core::ButtonEvent event);
    void ReportDebugErrors();

    PanelManager &panels_;
    IStyleManager &style_;

    clarity::core::VehicleInputs vehicle_{};
    clarity::core::DisplayRequest applied_{clarity::core::OverridePanel::None, clarity::core::KeyState::Inactive};
    bool applied_lights_ = false; ///< Lights state the theme was last set for (the style manager starts on Day)
    clarity::core::ButtonEvent pending_button_ = clarity::core::ButtonEvent::None;
    bool pending_debug_error_ = false;
};
