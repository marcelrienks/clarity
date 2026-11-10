#include "panels/key_panel.h"
#include "components/key_component.h"
#include "definitions/constants.h"
#include "handlers/trigger_handler.h"
#include "managers/error_manager.h"
#include "managers/interrupt_manager.h"
#include "sensors/gpio_sensor.h"
#include "utilities/logging.h"
#include <Arduino.h>
#include <variant>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs key panel with required service dependencies
 * @param gpio GPIO provider for hardware sensor access
 * @param display Display provider for screen management
 * @param styleManager Style service for theme-based icon coloring
 *
 * Creates key panel that displays vehicle ignition key status. Determines
 * current key state during construction and initializes stack-allocated
 * key component for efficient memory usage.
 */
KeyPanel::KeyPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
                   IPanelManager *panel_manager)
    : BasePanel(gpio, display, style_manager, panel_manager), key_component_(style_manager),
      component_initialized_(false)
{
    // Determine current key state by checking which sensor is currently active
    current_key_state_ = DetermineCurrentKeyState();
}

// ========== Protected Methods ==========

/**
 * @brief Creates key panel UI content with current key state display
 *
 * Renders key component on screen and applies appropriate color based on
 * current key state. Green indicates key present, red indicates key absent.
 * Essential for immediate visual feedback of vehicle ignition status.
 */
void KeyPanel::CreateContent()
{
    // Component is now stack-allocated and initialized in constructor
    component_initialized_ = true;

    // Render the component
    key_component_.Render(screen_, center_location_, display_provider_);

    // Set color directly on the concrete component
    key_component_.SetColor(current_key_state_);
}

/**
 * @brief Updates key panel content (static panel - no updates needed)
 *
 * Key panel displays static state determined at construction time.
 * State changes trigger panel transitions through the interrupt system
 * rather than content updates within the same panel instance.
 */
void KeyPanel::UpdateContent()
{
    // Key panel is static - no updates needed
    // State changes are handled by the interrupt system loading different panels
}

// Panel-specific button handling
/**
 * @brief Handles long button press events for panel navigation
 *
 * Processes long button press events by navigating to the configuration panel.
 * This provides users with access to system settings from the key status display.
 * Includes error handling for cases where the panel service is not available.
 */
void KeyPanel::HandleLongPress()
{
    if (panel_manager_)
    {
        log_i("KeyPanel long press - loading config panel");
        panel_manager_->CreateAndLoadPanel(panel_names::CONFIG, true);
    }
    else
    {
        log_w("KeyPanel: Cannot load config panel - panelManager not available");
    }
}

/**
 * @brief Determines the current key state by reading sensor inputs
 * @return Current key state (Present, NotPresent, or Inactive)
 *
 * Queries the key presence sensors through the trigger handler to determine
 * the current physical state of the ignition key. Checks both key present
 * and key not present sensors to provide accurate state detection. Returns
 * Inactive if neither sensor is active or if the trigger handler is unavailable.
 */
KeyState KeyPanel::DetermineCurrentKeyState()
{
    log_v("DetermineCurrentKeyState() called");

    // Access the sensors through InterruptManager to check their current states
    InterruptManager &interrupt_manager = InterruptManager::Instance();
    auto trigger_handler = interrupt_manager.GetTriggerHandler();

    if (!trigger_handler)
    {
        log_w("DetermineCurrentKeyState: TriggerHandler not available, defaulting to Inactive");
        return KeyState::Inactive;
    }

    // Check key present sensor (now using generic GetState method)
    auto key_present_sensor = trigger_handler->GetKeyPresentSensor();
    if (key_present_sensor && key_present_sensor->GetState())
    {
        return KeyState::Present;
    }

    // Check key not present sensor (now using generic GetState method)
    auto key_not_present_sensor = trigger_handler->GetKeyNotPresentSensor();
    if (key_not_present_sensor && key_not_present_sensor->GetState())
    {
        return KeyState::NotPresent;
    }

    // If neither sensor is active, default to inactive
    return KeyState::Inactive;
}