#include "panels/key_panel.h"
#include "components/key_component.h"
#include "definitions/constants.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <Arduino.h>
#include <variant>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs key panel with required service dependencies
 * @param gpio GPIO provider for hardware sensor access
 * @param display Display provider for screen management
 * @param theme Theme for the shared styles and the palette
 *
 * Creates key panel that displays vehicle ignition key status. Determines
 * current key state during construction and initializes stack-allocated
 * key component for efficient memory usage.
 */
KeyPanel::KeyPanel(IGpioProvider *gpio, IDisplayProvider *display, Theme *theme, KeyState key_state)
    : BasePanel(gpio, display, theme), key_component_(theme), component_initialized_(false),
      current_key_state_(key_state)
{
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
 * State changes reload the panel through the display controller
 * rather than content updates within the same panel instance.
 */
void KeyPanel::UpdateContent()
{
    // Key panel is static - no updates needed
    // State changes are handled by the screen manager reloading the panel
}

// Panel-specific button handling
/**
 * @brief Handles button presses: a long press opens the configuration panel
 */
clarity::core::NavIntent KeyPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event != clarity::core::ButtonEvent::Long)
    {
        return clarity::core::NavIntent::None;
    }

    log_i("KeyPanel long press - loading config panel");
    return clarity::core::NavIntent::OpenConfig;
}
