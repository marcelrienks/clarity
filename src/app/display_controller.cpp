#include "app/display_controller.h"

#include <Arduino.h>
#include <cstring>
#include <string>

#include "definitions/constants.h"
#include "definitions/enums.h"
#include "managers/error_manager.h"
#include "managers/panel_manager.h"
#include "ui/theme.h"
#include "utilities/logging.h"

namespace
{
using clarity::core::DisplayRequest;
using clarity::core::OverridePanel;

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

bool DisplayController::Begin(const Inputs &inputs)
{
    vehicle_ = inputs.Vehicle();
    ApplyTheme(vehicle_);

    applied_ = clarity::core::ResolveDisplay(vehicle_, false);
    panels_.SetKeyState(ToPanelKeyState(applied_.key));
    if (applied_.panel == OverridePanel::None)
    {
        return false;
    }

    log_i("Override active at startup - skipping default panel load");
    RequestOverride(applied_);
    return true;
}

void DisplayController::Update(const Inputs &inputs)
{
    // Button first, then the vehicle lines, the order the old interrupt manager used. Both act at once (Q-07): the
    // transitions they request are applied later in the pass, outside any panel or LVGL callback
    const clarity::core::ButtonEvent button = inputs.Button();
    if (button == clarity::core::ButtonEvent::Long)
    {
        log_t("Long press triggered during hold");
    }
    if (button != clarity::core::ButtonEvent::None)
    {
        ExecuteButton(button);
    }

    if (inputs.DebugErrorEdge())
    {
        ReportDebugErrors();
    }

    const clarity::core::VehicleInputs now = inputs.Vehicle();
    panels_.SetKeyState(ToPanelKeyState(clarity::core::ResolveDisplay(now, false).key));

    // The error panel owns the screen and the Error palette: remember the lines, act on them at restoration
    if (std::strcmp(panels_.GetCurrentPanel(), panel_names::ERROR) == 0)
    {
        vehicle_ = now;
        return;
    }

    ApplyTheme(now);

    // The splash finishes before an override replaces it (Q-06): keep the last acted-on lines, so the
    // difference is acted on once the target panel is loaded
    if (panels_.IsSplashRunning())
    {
        return;
    }

    const DisplayRequest request = clarity::core::ResolveDisplay(now, false);
    if (!SameRequest(request, applied_))
    {
        if (request.panel == OverridePanel::None)
        {
            LogDeactivated(vehicle_, now);
            panels_.CheckRestoration();
        }
        else
        {
            RequestOverride(request);
        }
        applied_ = request;
    }
    else if (request.panel != OverridePanel::None && AnyOverrideDeactivated(vehicle_, now))
    {
        // An override line cleared while another still holds the screen: the old trigger handler
        // re-activated the remaining override (same panel, reloaded)
        RequestOverride(request);
    }
    vehicle_ = now;
}

bool DisplayController::ApplyOverride()
{
    // Called while the restoration is applied; the error panel is going away, so errors do not count
    applied_ = clarity::core::ResolveDisplay(vehicle_, false);
    panels_.SetKeyState(ToPanelKeyState(applied_.key));

    ApplyTheme(vehicle_);

    if (applied_.panel == OverridePanel::None)
    {
        return false;
    }
    RequestOverride(applied_);
    return true;
}

void DisplayController::ApplyTheme(const clarity::core::VehicleInputs &in)
{
    if (in.lights != applied_lights_)
    {
        applied_lights_ = in.lights;
        if (in.lights)
        {
            log_t("LightsOnActivate() - Setting NIGHT theme");
        }
        else
        {
            log_t("LightsOffActivate() - Setting DAY theme");
        }
    }

    // Errors do not count here: the error panel applies the Error theme itself, and this runs only while it is
    // not shown or is being replaced. Apply() does nothing if the theme is unchanged
    theme_.Apply(clarity::core::EffectiveTheme(clarity::core::ResolveDisplay(in, false), in));
}

void DisplayController::RequestOverride(const DisplayRequest &request)
{
    if (request.panel == OverridePanel::Key)
    {
        log_t(request.key == clarity::core::KeyState::Present ? "KeyPresentActivate() - Loading KEY panel"
                                                              : "KeyNotPresentActivate() - Loading KEY panel");
        panels_.LoadPanel(panel_names::KEY);
    }
    else if (request.panel == OverridePanel::Lock)
    {
        log_t("LockEngagedActivate() - Loading LOCK panel");
        panels_.LoadPanel(panel_names::LOCK);
    }
}

void DisplayController::ExecuteButton(clarity::core::ButtonEvent event)
{
    if (event == clarity::core::ButtonEvent::Short)
    {
        log_t("ShortPressActivate() - Executing short press action");
        panels_.HandleShortPress();
    }
    else if (event == clarity::core::ButtonEvent::Long)
    {
        log_t("LongPressActivate() - Executing long press action");
        panels_.HandleLongPress();
    }
}

void DisplayController::ReportDebugErrors()
{
#ifdef CLARITY_DEBUG
    log_t("ErrorActivate() - Debug error button pressed, generating test errors");

    // Three test errors with unique timestamps, as the old debug trigger generated
    const std::string stamp = std::to_string(millis());
    ErrorManager &errors = ErrorManager::Instance();
    errors.ReportWarning("DebugTest", "Test warning from debug error trigger @" + stamp);
    errors.ReportError(ErrorLevel::ERROR, "DebugTest", "Test error from debug error trigger @" + stamp);
    errors.ReportCriticalError("DebugTest", "Test critical error from debug error trigger @" + stamp);

    log_t("Debug errors generated: 1 WARNING, 1 ERROR, 1 CRITICAL - error panel will load automatically");
#endif
}
