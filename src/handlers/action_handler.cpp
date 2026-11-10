#include "handlers/action_handler.h"
#include "definitions/constants.h"
#include "hardware/gpio_pins.h"
#include "interfaces/i_action_handler.h"
#include "interfaces/i_configuration_manager.h"
#include "managers/error_manager.h"
#include "sensors/button_sensor.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>

#include "esp32-hal-log.h"
#include "managers/configuration_manager.h"
#include "utilities/logging.h"

// Self-registration at program startup
static bool action_handler_config_registered = []()
{
    ConfigurationManager::AddSchema(ActionHandler::RegisterConfigSchema);
    return true;
}();

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs ActionHandler with GPIO provider and initializes button sensor
 * @param gpioProvider GPIO provider for hardware button access
 *
 * Creates ActionHandler instance that owns and manages a ButtonSensor for detecting
 * button press events. Initializes internal state machine and timing variables
 * for precise button action detection.
 */
ActionHandler::ActionHandler(IGpioProvider *gpio_provider)
    : pending_action_type_(ButtonAction::NONE), has_pending_action_(false), button_state_(ButtonState::IDLE),
      button_press_start_time_(0), button_press_end_time_(0), gpio_provider_(gpio_provider)
{
    // Create and initialize ButtonSensor owned by this handler
    if (gpio_provider_)
    {
        button_sensor_ = std::make_unique<ButtonSensor>(gpio_provider_);
        button_sensor_->Init();

        log_i("ActionHandler created and initialized ButtonSensor");
    }
    else
    {
        log_e("ActionHandler created without GPIO provider - ButtonSensor not initialized. Application will not "
              "function correctly!");
        ErrorManager::Instance().ReportCriticalError("ActionHandler",
                                                     "Created without GPIO provider - button input will not function");
    }
}

/**
 * @brief Destructor cleans up ActionHandler resources
 *
 * ButtonSensor is automatically cleaned up through unique_ptr. No manual
 * cleanup required for GPIO resources as they're managed by the provider.
 */
ActionHandler::~ActionHandler()
{
}

// ========== Public Interface Methods ==========

/**
 * @brief Validate actions by processing button events continuously
 *
 * Called every main loop cycle to provide continuous button monitoring.
 * Updates button state machine and processes button events to detect actions.
 * Does NOT execute pending actions - that is done by ExecutePendingActions() during UI IDLE only.
 * Designed for real-time responsiveness in automotive applications.
 */
void ActionHandler::ValidateActions()
{
    // Update button state machine
    UpdateButtonState();

    // Process button events and detect action type
    ProcessButtonEvents();
}

/**
 * @brief Sets the pending action, replacing any existing one
 * @param actionType The type of button action to set as pending
 *
 * Implements LIFO behavior with size 1 - only the most recent action
 * is kept. This ensures we always execute the latest user input.
 */
void ActionHandler::SetPendingAction(ButtonAction action_type)
{
    pending_action_type_ = action_type;
    has_pending_action_ = true;

    log_i("Set pending action: %s",
          actionType == ButtonAction::SHORT_PRESS  ? ui_strings::button_action_strings::SHORT_PRESS
          : actionType == ButtonAction::LONG_PRESS ? ui_strings::button_action_strings::LONG_PRESS
                                                   : ui_strings::button_action_strings::NONE);
}

/**
 * @brief Clears the pending action after execution
 */
void ActionHandler::ClearPendingAction()
{
    pending_action_type_ = ButtonAction::NONE;
    has_pending_action_ = false;
}

/**
 * @brief Checks if there's a pending action to execute
 * @return true if an action is pending
 */
bool ActionHandler::HasPendingAction() const
{
    return has_pending_action_;
}

/**
 * @brief Executes the single pending action
 *
 * Executes the pending action through the current panel's methods.
 * Only one action is pending at a time (LIFO with size 1), ensuring
 * we always execute the most recent user input. Clears the pending
 * action after execution.
 */
void ActionHandler::ExecutePendingActions()
{
    if (!has_pending_action_)
    {
        return; // No pending action is normal
    }

    if (!current_panel_)
    {
        log_e("ExecutePendingActions: No current panel set - button actions cannot be executed!");
        ErrorManager::Instance().ReportCriticalError("ActionHandler",
                                                     "No current panel set - button input is non-functional");
        return;
    }

    log_i("ExecutePendingActions: Executing %s press", pendingActionType_ == ButtonAction::SHORT_PRESS
                                                           ? ui_strings::button_action_strings::SHORT
                                                           : ui_strings::button_action_strings::LONG);

    // Execute through panel methods
    if (pending_action_type_ == ButtonAction::SHORT_PRESS)
    {
        current_panel_->HandleShortPress();
    }
    else if (pending_action_type_ == ButtonAction::LONG_PRESS)
    {
        current_panel_->HandleLongPress();
    }

    // Clear pending action after execution
    ClearPendingAction();
}

void ActionHandler::UpdateButtonState()
{
    bool current_pressed = IsButtonPressed();
    unsigned long current_time = millis();

    // State machine transitions
    switch (button_state_)
    {
        case ButtonState::IDLE:
            if (current_pressed)
            {
                button_state_ = ButtonState::PRESSED;
                StartButtonTiming();
                log_t("Button press started");
            }
            break;

        case ButtonState::PRESSED:
            if (!current_pressed)
            {
                button_state_ = ButtonState::RELEASED;
                StopButtonTiming();
                log_t("Button press ended");
            }
            else
            {
                // Check for long press during hold
                unsigned long press_duration = current_time - button_press_start_time_;
                unsigned long long_press_threshold = GetLongPressMs();
                log_v("Checking for long press: duration=%lu ms, threshold=%lu ms", press_duration,
                      long_press_threshold);
                if (press_duration >= long_press_threshold)
                {
                    button_state_ = ButtonState::LONG_PRESS_TRIGGERED;
                    log_d("Long press triggered after %lu ms (threshold: %lu ms)", press_duration,
                          long_press_threshold);
                    log_t("Long press triggered during hold");
                    // Immediately set the pending action when long press is detected
                    SetPendingAction(ButtonAction::LONG_PRESS);
                }
            }
            break;

        case ButtonState::LONG_PRESS_TRIGGERED:
            if (!current_pressed)
            {
                button_state_ = ButtonState::IDLE; // Skip RELEASED state for long press
                button_press_end_time_ = current_time;
            }
            break;

        case ButtonState::RELEASED:
            button_state_ = ButtonState::IDLE;
            break;
    }
}

/**
 * @brief Converts button state enum to human-readable string
 * @param state Button state to convert
 * @return String representation of the state
 *
 * Utility function for debug logging and diagnostic output. Provides
 * clear state names for troubleshooting button behavior issues.
 */
const char *ActionHandler::StateToString(ButtonState state) const
{
    switch (state)
    {
        case ButtonState::IDLE:
            return "IDLE";
        case ButtonState::PRESSED:
            return "PRESSED";
        case ButtonState::LONG_PRESS_TRIGGERED:
            return "LONG_TRIGGERED";
        case ButtonState::RELEASED:
            return "RELEASED";
        default:
            return "UNKNOWN";
    }
}

/**
 * @brief Processes button state changes and sets pending action
 *
 * Detects button actions based on state machine transitions and stores them
 * as pending. Only the most recent action is kept (LIFO with size 1).
 * Called every loop cycle to ensure responsive button handling.
 */
void ActionHandler::ProcessButtonEvents()
{
    // Check for short press action on button release
    if (button_state_ == ButtonState::RELEASED)
    {
        unsigned long press_duration = button_press_end_time_ - button_press_start_time_;

        // Only trigger short press if we haven't already triggered a long press
        if (press_duration >= 50 && press_duration < GetLongPressMs())
        {
            SetPendingAction(ButtonAction::SHORT_PRESS);
            log_i("Short press detected: duration=%lu ms", press_duration);
        }

        // Move to IDLE after processing RELEASED state
        button_state_ = ButtonState::IDLE;
    }
    // Long press is already handled in UpdateButtonState when threshold is reached
}

/**
 * @brief Captures button press start time for duration calculation
 *
 * Records timestamp when button press begins, resetting end time to ensure
 * clean timing state. Essential for accurate press duration measurement
 * used in short/long press classification.
 */
void ActionHandler::StartButtonTiming()
{
    button_press_start_time_ = millis();
    button_press_end_time_ = 0;
}

/**
 * @brief Captures button press end time for duration calculation
 *
 * Records timestamp when button press ends, enabling calculation of total
 * press duration. Used in conjunction with start time to classify button
 * actions as short or long presses.
 */
void ActionHandler::StopButtonTiming()
{
    button_press_end_time_ = millis();
}

/**
 * @brief Calculates button action type from press duration
 * @param pressDuration Duration of button press in milliseconds
 * @return ButtonAction classification based on timing thresholds
 *
 * Core classification logic that converts press duration into action type.
 * Uses automotive-appropriate timing thresholds: 50-2000ms for short press,
 * 2000ms+ for long press. Handles edge cases and provides diagnostic logging.
 */
ButtonAction ActionHandler::CalculateButtonAction(unsigned long press_duration)
{
    unsigned long debounce_ms = GetDebounceMs();
    unsigned long long_press_ms = GetLongPressMs();

    if (press_duration < debounce_ms)
    {
        return ButtonAction::NONE;
    }
    else if (press_duration < long_press_ms)
    {
        return ButtonAction::SHORT_PRESS;
    }
    else
    {
        // Any press >= longPressMs is a long press when released
        return ButtonAction::LONG_PRESS;
    }
}

/**
 * @brief Checks current button state from hardware sensor
 * @return true if button is currently pressed, false otherwise
 *
 * Reads current button state through ButtonSensor, handling different
 * reading types and providing diagnostic logging for troubleshooting.
 * Essential for real-time button state monitoring in the state machine.
 */
bool ActionHandler::IsButtonPressed() const
{
    if (!button_sensor_)
    {
        return false;
    }

    // Get button state from sensor - assuming bool reading
    Reading reading = button_sensor_->GetReading();
    if (std::holds_alternative<bool>(reading))
    {
        bool pressed = std::get<bool>(reading);
        return pressed;
    }

    // Check what type the reading actually is
    if (std::holds_alternative<int32_t>(reading))
    {
        log_w("IsButtonPressed: Got int32_t reading instead of bool: %d", std::get<int32_t>(reading));
    }
    else if (std::holds_alternative<std::monostate>(reading))
    {
        log_w("IsButtonPressed: Got monostate reading (uninitialized)");
    }
    else
    {
        log_w("IsButtonPressed: Got unexpected reading type");
    }

    return false;
}

// Function injection system
/**
 * @brief Updates injected panel functions for button actions
 * @param shortPressFunc Function to call for short press actions
 * @param longPressFunc Function to call for long press actions
 * @param context Panel context pointer passed to functions
 *
 * Function injection system that allows panels to override default action
 * execution. Enables dynamic button behavior changes during panel transitions
 * while maintaining consistent action registration.
 */
void ActionHandler::SetCurrentPanel(IActionHandler *panel)
{
    current_panel_ = panel;
    log_i("Set current panel: %p", (void *)panel);
}

/**
 * @brief Clears injected panel functions, reverting to action defaults
 *
 * Resets function injection system to use actions' own execution functions.
 * Called during panel cleanup to ensure clean state for next panel.
 * Prevents accidental execution of previous panel's functions.
 */
void ActionHandler::ClearCurrentPanel()
{
    current_panel_ = nullptr;
    log_i("Cleared current panel");
}

/**
 * @brief Registers button timing configuration schema with preference service
 * @param configurationManager The preference service to register with
 *
 * Registers configurable button press timing values including:
 * - Debounce timing: 300-700ms (default 500ms)
 * - Long press threshold: 1000-2000ms (default 1500ms)
 */
void ActionHandler::RegisterConfigSchema(IConfigurationManager *configuration_manager)
{
    if (!configuration_manager)
    {
        log_e("ActionHandler::RegisterConfigSchema: ConfigurationManager is null - button config registration failed!");
        ErrorManager::Instance().ReportCriticalError("ActionHandler", "ConfigManager null - button config failed");
        return;
    }

    // Check if already registered to prevent duplicates
    if (configuration_manager->IsSchemaRegistered(config_constants::sections::BUTTON_SENSOR))
    {
        log_d("ActionHandler schema already registered");
        return;
    }

    using namespace config;

    ConfigSection section(config_constants::sections::BUTTON_SENSOR, config_constants::sections::BUTTON_SENSOR,
                          config_constants::section_names::BUTTON_SENSOR);

    section.AddItem(debounce_config);
    section.AddItem(long_press_config);

    configuration_manager->RegisterConfigSection(section);
    log_i("ActionHandler configuration schema registered (static)");
}

/**
 * @brief Sets preference service for configuration access
 * @param configurationManager The preference service to use for configuration
 */
void ActionHandler::SetConfigurationManager(IConfigurationManager *configuration_manager)
{
    configuration_manager_ = configuration_manager;
}

/**
 * @brief Gets configured debounce time in milliseconds
 * @return Debounce time in ms, or default if config unavailable
 */
unsigned long ActionHandler::GetDebounceMs() const
{
    if (!configuration_manager_)
    {
        return config_constants::defaults::DEFAULT_DEBOUNCE_MS;
    }

    auto value = configuration_manager_->QueryConfig<int>(config_constants::keys::BUTTON_DEBOUNCE_MS);
    return value ? static_cast<unsigned long>(*value) : config_constants::defaults::DEFAULT_DEBOUNCE_MS;
}

/**
 * @brief Gets configured long press threshold in milliseconds
 * @return Long press threshold in ms, or default if config unavailable
 */
unsigned long ActionHandler::GetLongPressMs() const
{
    if (!configuration_manager_)
    {
        log_w("No preference service, using default long press threshold: %d ms",
              config_constants::defaults::DEFAULT_LONG_PRESS_MS);
        return config_constants::defaults::DEFAULT_LONG_PRESS_MS;
    }

    auto value = configuration_manager_->template QueryConfig<int>(config_constants::keys::BUTTON_LONG_PRESS_MS);
    unsigned long result =
        value ? static_cast<unsigned long>(*value) : config_constants::defaults::DEFAULT_LONG_PRESS_MS;
    log_v("GetLongPressMs returning: %lu ms (config key: %s)", result, config_constants::keys::BUTTON_LONG_PRESS_MS);
    return result;
}