#include "ui/screen_manager.h"

#include <cstring>
#include <string>
#include <utility>

#include "definitions/constants.h"
#include "interfaces/i_configuration_manager.h"
#include "panels/config_panel.h"
#include "panels/error_panel.h"
#include "panels/key_panel.h"
#include "panels/lock_panel.h"
#include "panels/oem_oil_panel.h"
#include "panels/splash_panel.h"
#include "ui/theme.h"
#include "utilities/logging.h"

namespace
{
using clarity::core::DisplayRequest;
using clarity::core::OverridePanel;
using clarity::core::PanelId;

/// The panel names the log lines (and the Wokwi scenario) use; unused when release builds compile the logs out
[[maybe_unused]] const char *PanelName(PanelId panel)
{
    switch (panel)
    {
        case PanelId::Splash:
            return panel_names::SPLASH;
        case PanelId::Oil:
            return panel_names::OIL;
        case PanelId::Config:
            return panel_names::CONFIG;
        case PanelId::Error:
            return panel_names::ERROR;
        case PanelId::Key:
            return panel_names::KEY;
        case PanelId::Lock:
            return panel_names::LOCK;
    }
    return panel_names::OIL;
}

/// The default panel options are the Oil and Config panels; anything else falls back to Oil
PanelId UserPanelFromName(const std::string &name)
{
    if (name == panel_names::CONFIG)
    {
        return PanelId::Config;
    }
    if (name != panel_names::OIL)
    {
        log_w("Unknown default panel '%s', using %s", name.c_str(), panel_names::OIL);
    }
    return PanelId::Oil;
}

bool SameRequest(const DisplayRequest &a, const DisplayRequest &b)
{
    return a.panel == b.panel && a.key == b.key;
}

::KeyState ToPanelKeyState(clarity::core::KeyState state)
{
    switch (state)
    {
        case clarity::core::KeyState::Present:
            return ::KeyState::Present;
        case clarity::core::KeyState::NotPresent:
            return ::KeyState::NotPresent;
        case clarity::core::KeyState::Inactive:
            break;
    }
    return ::KeyState::Inactive;
}

bool AnyOverrideDeactivated(const clarity::core::VehicleInputs &was, const clarity::core::VehicleInputs &now)
{
    return (was.key_present && !now.key_present) || (was.key_not_present && !now.key_not_present) ||
           (was.lock && !now.lock);
}

void LogDeactivated([[maybe_unused]] const clarity::core::VehicleInputs &was,
                    [[maybe_unused]] const clarity::core::VehicleInputs &now)
{
    if (was.key_present && !now.key_present)
    {
        log_t("KeyPresentDeactivate() - Checking for restoration");
    }
    if (was.key_not_present && !now.key_not_present)
    {
        log_t("KeyNotPresentDeactivate() - Checking for restoration");
    }
    if (was.lock && !now.lock)
    {
        log_t("LockDisengagedActivate() - Checking for restoration");
    }
}
} // namespace

ScreenManager::ScreenManager(IGpioProvider &gpio, IDisplayProvider &display, Theme &theme,
                             IConfigurationManager &configuration)
    : gpio_(gpio), display_(display), theme_(theme), configuration_(configuration)
{
    std::string default_panel = panel_names::OIL;
    if (auto value = configuration_.QueryConfig<std::string>(config_constants::keys::SYSTEM_DEFAULT_PANEL))
    {
        default_panel = *value;
    }
    user_panel_ = UserPanelFromName(default_panel);
    log_i("ScreenManager: user panel is '%s'", PanelName(user_panel_));
}

// Out of line: IPanel is complete here
ScreenManager::~ScreenManager() = default;

void ScreenManager::Begin(const clarity::core::VehicleInputs &vehicle)
{
    vehicle_ = vehicle;
    ApplyTheme(vehicle_);

    applied_ = clarity::core::ResolveDisplay(vehicle_, false);
    key_state_ = ToPanelKeyState(applied_.key);
    if (applied_.panel != OverridePanel::None)
    {
        log_i("Override active at startup - skipping default panel load");
        Perform(RequestOverride(applied_));
        return;
    }

    const Transition transition = Request(user_panel_);

    bool show_splash = true;
    if (auto value = configuration_.QueryConfig<bool>(config_constants::keys::SYSTEM_SHOW_SPLASH))
    {
        show_splash = *value;
    }
    if (!show_splash)
    {
        Perform(transition);
        return;
    }

    log_i("Loading panel with splash transition: %s", PanelName(user_panel_));
    splash_target_ = user_panel_;
    Load(PanelId::Splash);
}

void ScreenManager::OnButton(clarity::core::ButtonEvent event)
{
    if (event == clarity::core::ButtonEvent::Short)
    {
        log_t("ShortPressActivate() - Executing short press action");
        log_t("HandleShortPress() called");
    }
    else if (event == clarity::core::ButtonEvent::Long)
    {
        log_t("LongPressActivate() - Executing long press action");
        log_t("HandleLongPress() called");
    }
    else
    {
        return;
    }

    if (!panel_)
    {
        log_w("No active panel for button action");
        return;
    }

    const clarity::core::NavIntent intent = panel_->OnButton(event);
    if (intent != clarity::core::NavIntent::None)
    {
        intent_ = intent;
    }
}

void ScreenManager::Apply(const clarity::core::VehicleInputs &vehicle, bool errors_active)
{
    // At most one transition per pass. A later request replaces an earlier one: the splash completing, then the
    // button's intent, then the key and lock lines, then errors
    Transition next;

    key_state_ = ToPanelKeyState(clarity::core::ResolveDisplay(vehicle, false).key);

    // The splash defers every override (Q-06), including in the pass it finishes: they are acted on once its
    // target panel is loaded
    const bool splash_running = current_ == PanelId::Splash;
    if (splash_running && panel_ && panel_->Finished())
    {
        log_i("Splash panel completed - transitioning to target panel: %s", PanelName(splash_target_));
        next = Request(splash_target_);
    }

    switch (std::exchange(intent_, clarity::core::NavIntent::None))
    {
        case clarity::core::NavIntent::OpenConfig:
            next = Request(PanelId::Config);
            break;
        case clarity::core::NavIntent::ExitConfig:
            next = Request(user_panel_);
            break;
        case clarity::core::NavIntent::ClearErrors:
            // The error panel has cleared the errors
            next = RequestRestoration();
            break;
        case clarity::core::NavIntent::None:
            break;
    }

    if (current_ == PanelId::Error)
    {
        // The error panel owns the screen and the Error theme: remember the lines, act on them at restoration
        vehicle_ = vehicle;
    }
    else
    {
        ApplyTheme(vehicle);

        if (!splash_running)
        {
            const DisplayRequest request = clarity::core::ResolveDisplay(vehicle, false);
            if (!SameRequest(request, applied_))
            {
                if (request.panel == OverridePanel::None)
                {
                    LogDeactivated(vehicle_, vehicle);
                    next = RequestRestoration();
                }
                else
                {
                    next = RequestOverride(request);
                }
                applied_ = request;
            }
            else if (request.panel != OverridePanel::None && AnyOverrideDeactivated(vehicle_, vehicle))
            {
                // An override line cleared while another still holds the screen: the old trigger handler
                // re-activated the remaining override (same panel, reloaded)
                next = RequestOverride(request);
            }
            vehicle_ = vehicle;
        }
    }

    // Any pending error, including a warning, brings up the error panel once the splash is done (2.6)
    if (!splash_running && errors_active && current_ != PanelId::Error)
    {
        log_t("ErrorOccurredActivate() - Loading ERROR panel");
        next = Request(PanelId::Error);
    }

    Perform(next);
}

void ScreenManager::Update()
{
    // Every pass: a panel that animates skips its own updates until the animation ends
    if (panel_)
    {
        panel_->Update();
    }
}

ScreenManager::Transition ScreenManager::Request(PanelId panel) const
{
    log_t("Panel transition requested: %s", PanelName(panel));
    return {TransitionKind::Load, panel};
}

ScreenManager::Transition ScreenManager::RequestOverride(const DisplayRequest &request) const
{
    if (request.panel == OverridePanel::Key)
    {
        log_t(request.key == clarity::core::KeyState::Present ? "KeyPresentActivate() - Loading KEY panel"
                                                              : "KeyNotPresentActivate() - Loading KEY panel");
        log_t("LoadPanel() called for: %s", panel_names::KEY);
        return Request(PanelId::Key);
    }
    if (request.panel == OverridePanel::Lock)
    {
        log_t("LockEngagedActivate() - Loading LOCK panel");
        log_t("LoadPanel() called for: %s", panel_names::LOCK);
        return Request(PanelId::Lock);
    }
    return {};
}

ScreenManager::Transition ScreenManager::RequestRestoration() const
{
    log_t("CheckRestoration() called");
    return {TransitionKind::Restore, user_panel_};
}

void ScreenManager::ApplyTheme(const clarity::core::VehicleInputs &vehicle)
{
    if (vehicle.lights != applied_lights_)
    {
        applied_lights_ = vehicle.lights;
        if (vehicle.lights)
        {
            log_t("LightsOnActivate() - Setting NIGHT theme");
        }
        else
        {
            log_t("LightsOffActivate() - Setting DAY theme");
        }
    }

    // Errors do not count here: Load() applies the Error theme with the error panel, and this runs only while
    // the error panel is not shown or is being left. Apply() does nothing if the theme is unchanged
    theme_.Apply(clarity::core::EffectiveTheme(clarity::core::ResolveDisplay(vehicle, false), vehicle));
}

void ScreenManager::Perform(const Transition &transition)
{
    switch (transition.kind)
    {
        case TransitionKind::Load:
            Load(transition.panel);
            break;
        case TransitionKind::Restore:
            Restore();
            break;
        case TransitionKind::None:
            break;
    }
}

void ScreenManager::Restore()
{
    // The error panel is going away, so errors do not count. The lights theme replaces Error before the next
    // panel loads
    applied_ = clarity::core::ResolveDisplay(vehicle_, false);
    key_state_ = ToPanelKeyState(applied_.key);
    ApplyTheme(vehicle_);

    if (applied_.panel != OverridePanel::None)
    {
        const Transition transition = RequestOverride(applied_);
        log_t("Active trigger found and executed - skipping restoration");
        Perform(transition);
        return;
    }

    // Restoration is always direct: the splash is only shown at startup
    log_t("No blocking interrupts - restoring to '%s'", PanelName(user_panel_));
    Load(user_panel_);
}

void ScreenManager::Load(PanelId panel)
{
    log_v("Load() called for: %s", PanelName(panel));

    // The error panel forces the Error theme (EffectiveTheme); Restore() puts the lights theme back
    if (panel == PanelId::Error)
    {
        theme_.Apply(clarity::core::ThemeId::Error);
    }

    std::unique_ptr<IPanel> next = CreatePanel(panel);
    next->Init();

    // Swap order (A-05): the previous panel is destroyed only once the new screen is the active one
    std::unique_ptr<IPanel> previous = std::move(panel_);
    panel_ = std::move(next);
    current_ = panel;
    panel_->Load();
    previous.reset();
}

std::unique_ptr<IPanel> ScreenManager::CreatePanel(PanelId panel)
{
    switch (panel)
    {
        case PanelId::Splash:
            return std::make_unique<SplashPanel>(&gpio_, &display_, &theme_, &configuration_);
        case PanelId::Config:
            return std::make_unique<ConfigPanel>(&gpio_, &display_, &theme_, &configuration_);
        case PanelId::Error:
            return std::make_unique<ErrorPanel>(&gpio_, &display_, &theme_);
        case PanelId::Key:
            return std::make_unique<KeyPanel>(&gpio_, &display_, &theme_, key_state_);
        case PanelId::Lock:
            return std::make_unique<LockPanel>(&gpio_, &display_, &theme_);
        case PanelId::Oil:
            break;
    }
    return std::make_unique<OemOilPanel>(&gpio_, &display_, &theme_, &configuration_);
}
