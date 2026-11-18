#pragma once

// System/Library Includes
#include <lvgl.h>
#include <memory>

// Project Includes
#include "interfaces/i_action_handler.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"

// Forward declarations
class IPanelManager;
class IStyleManager;
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
 * @button_handling All panels inherit IActionHandler; button events reach the current panel through PanelManager
 *
 * @lifecycle:
 * 1. init(): Initialize panel and create components
 * 2. load(): Setup UI and start async operations with callback
 * 3. update(): Called every loop pass, also while the panel animates
 * 4. show(): Make panel visible on screen
 *
 * @async_handling:
 * - load() accepts completion callbacks for smooth transitions
 * - Panels can perform time-consuming initialization without blocking
 * - PanelManager uses callbacks to coordinate panel switching
 *
 * @sensor_integration:
 * - Display-only panels (Key, Lock): Create only components, no sensors
 * - Data panels (Oil): Create own data sensors and components
 * - Override panels (Key, Lock) get their state from the display controller
 *
 * @button_integration:
 * - All panels must implement IActionService methods
 * - The display controller sends button events to the current panel through PanelManager
 * - Button events execute current panel's functions with panel context
 *
 * @implementations:
 * - SplashPanel: Startup branding screen
 * - OemOilPanel: Main oil monitoring dashboard (creates own sensors)
 * - KeyPanel: Key status display (display-only)
 * - LockPanel: Lock status display (display-only)
 * - ErrorPanel: Error display with navigation and auto-restoration
 * - ConfigPanel: Configuration with hierarchical state machine
 *
 * @context All panels coordinate complete screen experiences and provide
 * consistent button handling through IActionHandler inheritance.
 */
class IPanel : public IActionHandler
{
  public:
    // Destructors
    ~IPanel() override = default;

    // Core Interface Methods
    virtual void Init() = 0;
    virtual void Load() = 0;
    virtual void Update() = 0;

    // Note: Button handling methods inherited from IActionHandler
    // All panels must implement:
    // - void HandleShortPress()
    // - void HandleLongPress()

    // SetManagers and SetConfigurationManager removed - using constructor injection

  protected:
    // Protected Data Members
    lv_obj_t *screen_ = nullptr;
};