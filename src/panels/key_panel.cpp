#include "panels/key_panel.h"
#include "components/key_component.h"
#include "definitions/constants.h"
#include "handlers/trigger_handler.h"
#include "managers/error_manager.h"
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
    current_key_state_ = panel_manager_->GetKeyState();
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
