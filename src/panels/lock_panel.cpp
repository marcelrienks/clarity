#include "panels/lock_panel.h"
#include "components/lock_component.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <esp32-hal-log.h>
#include <variant>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs lock panel for vehicle security status display
 * @param gpio GPIO provider for hardware interaction
 * @param display Display provider for screen management
 * @param theme Theme for the shared styles and the palette
 *
 * Creates lock panel that displays vehicle security lock status. Assumes
 * lock engaged state since panel is only loaded when lock state changes.
 * Uses stack-allocated component for efficient memory management.
 */
LockPanel::LockPanel(IGpioProvider *gpio, IDisplayProvider *display, Theme *theme)
    : BasePanel(gpio, display, theme), lock_component_(theme), component_initialized_(false)
{
    // Note: LockPanel is display-only, the screen manager loads it while the lock input is active
    // Since this panel is only loaded when lock state changes, assume engaged
    is_lock_engaged_ = true;
}

// ========== Protected Methods ==========

/**
 * @brief Creates lock panel UI content with security status display
 *
 * Renders lock component on screen showing current lock engagement state.
 * Always displays red (engaged) state as panel is triggered by lock events.
 * Provides immediate visual confirmation of vehicle security activation.
 */
void LockPanel::CreateContent()
{
    // Component is now stack-allocated and initialized in constructor
    component_initialized_ = true;

    // Create the lock component centered on screen - always shows red (engaged) state
    lock_component_.Render(screen_, center_location_, display_provider_);
}

/**
 * @brief Updates lock panel content (static panel - no updates needed)
 *
 * Lock panel displays static state determined by trigger system.
 * State changes result in panel transitions rather than content updates
 * within the same panel instance. The screen manager manages state changes.
 */
void LockPanel::UpdateContent()
{
    // Lock panel is static - no updates needed
    // State changes are handled by the screen manager loading different panels
}

/**
 * @brief Handles button presses: a long press opens the configuration menu
 */
clarity::core::NavIntent LockPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event != clarity::core::ButtonEvent::Long)
    {
        return clarity::core::NavIntent::None;
    }

    log_i("LockPanel long press - loading config panel");
    return clarity::core::NavIntent::OpenConfig;
}