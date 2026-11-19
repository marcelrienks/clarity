#pragma once

#include <cstdint>

#include "clarity/core/display_policy.h"
#include "clarity/core/types.h"
#include "panels/config_panel.h"
#include "panels/error_panel.h"
#include "panels/key_panel.h"
#include "panels/lock_panel.h"
#include "panels/oem_oil_panel.h"
#include "panels/splash_panel.h"

class IConfigurationManager;
class IGpioProvider;
class Panel;
class Theme;

/**
 * @class ScreenManager
 * @brief The single owner of the screen: decides and applies every panel transition (A-02, A-05)
 *
 * @details Nothing else changes the active screen. The loop hands it the button event (OnButton()) and then the
 * sampled inputs once per pass (Apply()), which compares clarity::core::ResolveDisplay() with what is on screen
 * and applies at most one transition, outside any panel or LVGL callback (D-13). Panels never call back: a button
 * press returns a NavIntent, applied on the next Apply(), and the splash reports Finished().
 *
 * - Overrides by priority: errors, then key (present wins over not present), then lock. A key state change
 *   reloads the key panel. While the error panel is shown, key and lock changes are tracked and acted on when it
 *   is left (restructure.md 2.2).
 * - Restoration: when the last override clears, or the error panel is cleared, the override that is still
 *   active is shown, otherwise the user panel, without splash.
 * - Config is modal over the user panel: entering it does not change the user panel, so Exit and restoration
 *   both return to the user panel.
 * - The splash defers every override until it is done (Q-06); its target loads in the pass it finishes.
 * - Theme: the lights theme, applied every pass while the error panel is not shown, and Error while it is
 *   (EffectiveTheme, Q-05).
 *
 * The user panel is the default panel from the configuration (system.default_panel), read once.
 *
 * The six panels are long-lived members: Enter() builds a panel's LVGL tree and Exit() deletes it, so a panel keeps
 * its state between visits only where it chooses to. The previous panel is exited only once the next screen is
 * loaded (swap order, A-05). A reload of the panel already shown (key state change, a remaining override
 * re-activated) recolours the key panel in place and does nothing for the others.
 */
class ScreenManager
{
  public:
    ScreenManager(IGpioProvider &gpio, Theme &theme, IConfigurationManager &configuration);
    ScreenManager(const ScreenManager &) = delete;
    ScreenManager &operator=(const ScreenManager &) = delete;
    ~ScreenManager();

    /// Shows the first panel from the seeded inputs: a key or lock override directly, with no splash and no
    /// default panel, otherwise the default panel, behind the splash when system.show_splash is set
    void Begin(const clarity::core::VehicleInputs &vehicle);

    /// Hands a completed press to the current panel and keeps the intent it returns for the next Apply()
    void OnButton(clarity::core::ButtonEvent event);

    /// The single transition point: call once per loop pass, after OnButton()
    void Apply(const clarity::core::VehicleInputs &vehicle, bool errors_active);

    /// Calls the current panel's Update(now), every loop pass
    void Update(uint32_t now);

  private:
    enum class TransitionKind : uint8_t
    {
        None,
        Load,    ///< Load `panel` directly
        Restore, ///< Show the override that is still active, otherwise the user panel
    };
    struct Transition
    {
        TransitionKind kind = TransitionKind::None;
        clarity::core::PanelId panel = clarity::core::PanelId::Oil;
    };

    Transition Request(clarity::core::PanelId panel) const;
    Transition RequestOverride(const clarity::core::DisplayRequest &request) const;
    Transition RequestRestoration() const;
    void ApplyTheme(const clarity::core::VehicleInputs &vehicle);
    void Perform(const Transition &transition);
    void Restore();
    void Load(clarity::core::PanelId panel);
    Panel &PanelFor(clarity::core::PanelId panel);
    bool PanelFinished() const;

    Theme &theme_;
    IConfigurationManager &configuration_;

    SplashPanel splash_;
    KeyPanel key_;
    LockPanel lock_;
    OemOilPanel oil_;
    ConfigPanel config_;
    ErrorPanel error_;
    Panel *active_ = nullptr; ///< The panel on screen; nullptr before Begin()
    clarity::core::PanelId current_ = clarity::core::PanelId::Splash;
    clarity::core::PanelId user_panel_ = clarity::core::PanelId::Oil;
    clarity::core::PanelId splash_target_ = clarity::core::PanelId::Oil;
    clarity::core::NavIntent intent_ = clarity::core::NavIntent::None;

    clarity::core::VehicleInputs vehicle_{}; ///< Lines last acted on: held while the splash runs
    clarity::core::DisplayRequest applied_{clarity::core::OverridePanel::None, clarity::core::KeyState::Inactive};
    clarity::core::KeyState key_state_ = clarity::core::KeyState::Inactive; ///< What the key panel shows
    bool applied_lights_ = false; ///< Lights state the theme was last set for (Day at start)
};
