#pragma once

#include "definitions/configs.h"
#include "definitions/constants.h"
#include "definitions/types.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_gpio_provider.h"
#include <memory>
#include <vector>

// Forward declarations for sensors
class ButtonSensor;
class IActionHandler;

#include "esp32-hal-log.h"

/**
 * @class ActionHandler
 * @brief Handles event-based actions with ButtonSensor ownership and press duration detection
 *
 * @details Processes button events with timing detection and continuous evaluation:
 * - Short press: 50ms - 2000ms duration
 * - Long press: 2000ms - 5000ms duration
 * - Continuous evaluation: Actions evaluated every main loop cycle
 *
 * @architecture Owns ButtonSensor for exclusive button monitoring
 * @timing_detection Precise press duration measurement for action classification
 * @function_injection Receives current panel functions dynamically
 */
class ActionHandler
{
  public:
    // ========== Constructors and Destructor ==========
    ActionHandler(IGpioProvider *gpio_provider);
    ~ActionHandler();

    // ========== Public Interface Methods ==========
    // Main action processing methods
    void ValidateActions();
    void ExecutePendingActions();

    // Panel management for direct method calls
    void SetCurrentPanel(IActionHandler *panel);
    void ClearCurrentPanel();

    // Sensor access for action context
    ButtonSensor *GetButtonSensor() const
    {
        return button_sensor_.get();
    }

    // Status and diagnostics
    bool HasPendingAction() const;
    void ClearPendingAction();

    // Configuration management
    static void RegisterConfigSchema(IConfigurationManager *configuration_manager);
    void SetConfigurationManager(IConfigurationManager *configuration_manager);

    // ========== Public Data Members ==========
    // Button state machine (moved to public for StateToString access)
    enum class ButtonState
    {
        IDLE,
        PRESSED,
        LONG_PRESS_TRIGGERED,
        RELEASED
    };

  private:
    // ========== Private Methods ==========
    // Core action processing
    void ProcessButtonEvents();
    void SetPendingAction(ButtonAction action_type);

    // Button timing detection
    void UpdateButtonState();
    void StartButtonTiming();
    void StopButtonTiming();
    ButtonAction CalculateButtonAction(unsigned long press_duration);

    // Helper methods
    bool IsButtonPressed() const;
    const char *StateToString(ButtonState state) const;

    // Configuration helpers
    unsigned long GetDebounceMs() const;
    unsigned long GetLongPressMs() const;

    // ========== Private Data Members ==========
    // Configuration items for button timing
    inline static config::ConfigItem debounce_config{
        config_constants::items::DEBOUNCE_MS, ui_strings::config_labels::DEBOUNCE_MS,
        config_constants::defaults::DEFAULT_DEBOUNCE_MS,
        config::ConfigMetadata(config_constants::options::DEBOUNCE_OPTIONS, "ms", config::ConfigItemType::Selection)};
    inline static config::ConfigItem long_press_config{
        config_constants::items::LONG_PRESS_MS, ui_strings::config_labels::LONG_PRESS_MS,
        config_constants::defaults::DEFAULT_LONG_PRESS_MS,
        config::ConfigMetadata(config_constants::options::LONG_PRESS_OPTIONS, "ms", config::ConfigItemType::Selection)};

    // Single pending action (LIFO with size 1)
    ButtonAction pending_action_type_ = ButtonAction::NONE;
    bool has_pending_action_ = false;

    // Button state
    ButtonState button_state_ = ButtonState::IDLE;
    unsigned long button_press_start_time_ = 0;
    unsigned long button_press_end_time_ = 0;

    // Current panel for direct method calls
    class IActionHandler *current_panel_ = nullptr;

    // Preference service for configuration access
    IConfigurationManager *configuration_manager_ = nullptr;

    // Handler-owned sensor
    IGpioProvider *gpio_provider_;
    std::unique_ptr<ButtonSensor> button_sensor_;
};