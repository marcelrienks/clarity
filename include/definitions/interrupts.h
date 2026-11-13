#pragma once

#include "definitions/constants.h"
#include "definitions/types.h"
#include "managers/panel_manager.h"
#include "managers/style_manager.h"
#include "utilities/logging.h"
#ifdef CLARITY_DEBUG
    #include "managers/error_manager.h"
    #include <Arduino.h>
#endif

/**
 * @file definitions/interrupts.h
 * @brief System trigger and action definitions for the interrupt architecture
 *
 * @details This file contains all the system-wide interrupt trigger and action
 * definitions exactly as specified in docs/interrupt-architecture.md. These
 * definitions implement the modern Trigger/Action separation architecture for
 * interrupt-driven system behavior.
 */

namespace Interrupts
{

inline std::vector<Trigger> GetSystemTriggers(class BaseSensor *key_present_sensor,
                                              class BaseSensor *key_not_present_sensor, class BaseSensor *lock_sensor,
                                              class BaseSensor *lights_sensor, class BaseSensor *error_sensor = nullptr)
{

    std::vector<Trigger> triggers = {// Key triggers - CRITICAL priority
                                     {.id = trigger_ids::KEY_PRESENT,
                                      .priority = Priority::CRITICAL,
                                      .type = TriggerType::PANEL,
                                      .activateFunc =
                                          []()
                                      {
                                          log_t("KeyPresentActivate() - Loading KEY panel");
                                          PanelManager::Instance().LoadPanel(panel_names::KEY);
                                      },
                                      .deactivateFunc =
                                          []()
                                      {
                                          log_t("KeyPresentDeactivate() - Checking for restoration");
                                          PanelManager::Instance().CheckRestoration();
                                      },
                                      .sensor = key_present_sensor,
                                      .isActive = false},
                                     {.id = trigger_ids::KEY_NOT_PRESENT,
                                      .priority = Priority::CRITICAL,
                                      .type = TriggerType::PANEL,
                                      .activateFunc =
                                          []()
                                      {
                                          log_t("KeyNotPresentActivate() - Loading KEY panel");
                                          PanelManager::Instance().LoadPanel(panel_names::KEY);
                                      },
                                      .deactivateFunc =
                                          []()
                                      {
                                          log_t("KeyNotPresentDeactivate() - Checking for restoration");
                                          PanelManager::Instance().CheckRestoration();
                                      },
                                      .sensor = key_not_present_sensor,
                                      .isActive = false},

                                     // Lock trigger - IMPORTANT priority
                                     {.id = trigger_ids::LOCK,
                                      .priority = Priority::IMPORTANT,
                                      .type = TriggerType::PANEL,
                                      .activateFunc =
                                          []()
                                      {
                                          log_t("LockEngagedActivate() - Loading LOCK panel");
                                          PanelManager::Instance().LoadPanel(panel_names::LOCK);
                                      },
                                      .deactivateFunc =
                                          []()
                                      {
                                          log_t("LockDisengagedActivate() - Checking for restoration");
                                          PanelManager::Instance().CheckRestoration();
                                      },
                                      .sensor = lock_sensor,
                                      .isActive = false},

                                     // Lights trigger - NORMAL priority
                                     {.id = trigger_ids::LIGHTS,
                                      .priority = Priority::NORMAL,
                                      .type = TriggerType::STYLE,
                                      .activateFunc =
                                          []()
                                      {
                                          log_t("LightsOnActivate() - Setting NIGHT theme");
                                          StyleManager::Instance().SetTheme(themes::NIGHT);
                                      },
                                      .deactivateFunc =
                                          []()
                                      {
                                          log_t("LightsOffActivate() - Setting DAY theme");
                                          StyleManager::Instance().SetTheme(themes::DAY);
                                      },
                                      .sensor = lights_sensor,
                                      .isActive = false}};

    // Add error trigger if sensor is provided
    // Note: This is a simple event trigger - it doesn't maintain state or activate/deactivate
    // The sensor generates errors when button is pressed, error panel loading is handled automatically
    if (error_sensor)
    {
        triggers.push_back(
            {.id = trigger_ids::ERROR,
             .priority = Priority::CRITICAL,
             .type = TriggerType::PANEL, // Keep as PANEL type for priority handling
             .activateFunc =
                 []()
             {
                 log_t("ErrorActivate() - Debug error button pressed, generating test errors");

#ifdef CLARITY_DEBUG
                 // Generate three test errors for error panel testing (unique timestamps)
                 uint32_t timestamp = millis();
                 ErrorManager::Instance().ReportWarning("DebugTest", "Test warning from debug error trigger @" +
                                                                         std::to_string(timestamp));

                 ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "DebugTest",
                                                      "Test error from debug error trigger @" +
                                                          std::to_string(timestamp));

                 ErrorManager::Instance().ReportCriticalError(
                     "DebugTest", "Test critical error from debug error trigger @" + std::to_string(timestamp));

                 log_t("Debug errors generated: 1 WARNING, 1 ERROR, 1 CRITICAL - error panel will load automatically");
#else
                 log_t("Debug error generation not available in release build");
#endif
             },
             .deactivateFunc = nullptr, // No deactivate - one-shot button press only
             .sensor = error_sensor,
             .isActive = false});
    }

    return triggers;
}

inline std::vector<Action> GetSystemActions()
{
    return {// Button actions
            {.id = trigger_ids::SHORT_PRESS,
             .executeFunc =
                 []()
             {
                 log_t("ShortPressActivate() - Executing short press action");
                 PanelManager::ActionService().HandleShortPress();
             },
             .hasTriggered = false,
             .pressType = ActionPress::SHORT},
            {.id = trigger_ids::LONG_PRESS,
             .executeFunc =
                 []()
             {
                 log_t("LongPressActivate() - Executing long press action");
                 PanelManager::ActionService().HandleLongPress();
             },
             .hasTriggered = false,
             .pressType = ActionPress::LONG}};
}

} // namespace Interrupts