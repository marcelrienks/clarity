#pragma once

// System/Library Includes
#include <lvgl.h>
#include <memory>

// Project Includes
#include "clarity/core/types.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"

// Forward declarations
class Theme;
class IConfigurationManager;

/**
 * @interface IPanel
 * @brief Base interface for all screen panels with universal button handling
 *
 * @details This interface defines the contract for panels that manage complete
 * screens in the MVP architecture. Panels act as Presenters, coordinating
 * between sensors (models) and components (views) to create cohesive displays.
 * All panels implement IActionService for consistent button input handling.
 *
 * @design_pattern Presenter in MVP - coordinates models and views
 * @button_handling OnButton() returns a NavIntent that ScreenManager applies on its next Apply()
 *
 * @lifecycle:
 * 1. init(): Initialize panel and create components
 * 2. load(): Setup UI and start async operations with callback
 * 3. update(): Called every loop pass, also while the panel animates
 * 4. show(): Make panel visible on screen
 *
 * @transitions:
 * - Panels never switch screens: ScreenManager creates, loads and destroys them
 * - The splash reports Finished(); ScreenManager then loads its target
 *
 * @sensor_integration:
 * - Display-only panels (Key, Lock): Create only components, no sensors
 * - Data panels (Oil): Create own data sensors and components
 * - The key panel gets its key state from ScreenManager when it is created
 *
 * @implementations:
 * - SplashPanel: Startup branding screen
 * - OemOilPanel: Main oil monitoring dashboard (creates own sensors)
 * - KeyPanel: Key status display (display-only)
 * - LockPanel: Lock status display (display-only)
 * - ErrorPanel: Error display with navigation and auto-restoration
 * - ConfigPanel: Configuration with hierarchical state machine
 *
 * @context Legacy panel interface: 5.5 moves every panel onto ui/panel.h.
 */
class IPanel
{
  public:
    // Destructors
    virtual ~IPanel() = default;

    // Core Interface Methods
    virtual void Init() = 0;
    virtual void Load() = 0;
    virtual void Update() = 0;

    /// Handles a completed button press and says what the screen manager should do next
    virtual clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event)
    {
        (void)event;
        return clarity::core::NavIntent::None;
    }

    /// False while the panel must not be replaced by an override (the splash until it is done, Q-06)
    virtual bool Finished() const
    {
        return true;
    }

  protected:
    // Protected Data Members
    lv_obj_t *screen_ = nullptr;
};