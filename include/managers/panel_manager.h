#pragma once
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_style_manager.h"
#include "interfaces/i_action_handler.h"
#include "definitions/types.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @class PanelManager
 * @brief Panel lifecycle management and transitions service implementing IPanelService
 *
 * @details This service handles the complete lifecycle of panels including
 * creation, loading, updating, and transitions. It implements dependency injection
 * patterns to provide centralized panel management with dynamic panel creation.
 *
 * @design_patterns:
 * - Dependency Injection: Dependencies injected via constructor
 * - Factory: Dynamic panel creation via IPanelFactory
 * - Service: Implements IPanelService interface
 *
 * @panel_lifecycle:
 * 1. Register panel types with register_panel<T>()
 * 2. Create panels dynamically via CreatePanel()
 * 3. Load panels with init() → load() → show callbacks
 * 4. Update panels with periodic refresh_panel()
 * 5. Transition between panels with load_panel()
 *
 * @registered_panels:
 * - SplashPanel: Startup/branding screen
 * - OemOilPanel: Main oil monitoring dashboard
 * - Future panels: Easy extensibility via registration
 *
 * @state_management:
 * - _ui_state: Controls UI processing state and prevents concurrent operations
 * - _panel: Current active panel instance
 * - Callback-based completion handling
 *
 * @special_features:
 * - create_and_load_panel_with_splash(): Smooth transitions with splash screen
 * - refresh_panel(): Periodic updates without full reload
 * - Thread-safe loading state management
 *
 * @context This is the main coordinator for all panel operations.
 * It manages the current panel (likely OemOilPanel) and handles transitions.
 * The factory pattern allows easy addition of new panel types.
 */

// Forward declarations
class StyleManager;
class InterruptManager;

class PanelManager : public IPanelManager,
                     public IActionHandler
{
  public:
    // ========== Constructors/Destructor ==========
    PanelManager(IDisplayProvider *display, IGpioProvider *gpio, IStyleManager *style,
                 IConfigurationManager *config, InterruptManager* interrupts = nullptr);
    PanelManager(const PanelManager &) = delete;
    PanelManager &operator=(const PanelManager &) = delete;
    ~PanelManager();

    // ========== Static Methods ==========
    // Singleton access for new interrupt architecture
    static PanelManager& Instance();

    // Interface accessors for dependency injection (CRITICAL for testability)
    static IActionHandler& ActionService() { return Instance(); }

    // ========== IPanelManager Implementation ==========
    void Init() override;
    void SetUiState(UIState state) override;
    UIState GetUiState() const override;
    void CreateAndLoadPanel(const char *panelName, bool isTriggerDriven = false) override;
    void UpdatePanel() override;
    const char *GetCurrentPanel() const override;
    const char *GetRestorationPanel() const override;
    bool IsCurrentPanelTriggerDriven() const override;
    void TriggerPanelSwitchCallback(const char *triggerId) override;

    // ========== Panel Notification Methods ==========
    void OnPanelLoadComplete(IPanel* panel);

    // ========== IActionHandler Implementation ==========
    void HandleShortPress() override;
    void HandleLongPress() override;

    // ========== Trigger Execution Methods ==========
    void LoadPanel(const char* panelName);
    void CheckRestoration();

    // ========== Other Public Methods ==========
    void UpdatePanelButtonFunctions(IPanel* panel);

  private:
    // ========== Private Methods ==========
    std::shared_ptr<IPanel> CreatePanel(const char *panelName);
    void CreateAndLoadPanelDirect(const char *panelName, bool isTriggerDriven = false);
    void CreateAndLoadPanelWithSplash(const char *panelName, bool isTriggerDriven);
    void SplashCompletionCallback(const char *panelName);
    void PanelCompletionCallback();

    // Helper methods for CreateAndLoadPanelDirect
    void UpdateRestorationTracking(const char* panelName, bool isTriggerDriven);
    void HandlePanelCreationError(const char* panelName);

    // ========== Panel State Data Members ==========
    std::string currentPanel_ = PanelNames::OIL;     ///< Current panel state
    std::string restorationPanel_ = PanelNames::OIL; ///< Panel to restore when all triggers are inactive
    std::shared_ptr<IPanel> panel_ = nullptr;
    UIState uiState_ = UIState::IDLE;          ///< Current UI processing state
    bool currentPanelIsTriggerDriven_ = false; ///< Track if current panel is trigger-driven

    // ========== Panel Name Storage Data Members ==========
    std::string splashTargetPanel_;                   ///< Target panel for splash transition
    bool splashTargetTriggerDriven_ = false;          ///< Preserve trigger state through splash transitions

    // ========== Service Dependencies ==========
    IGpioProvider *gpioProvider_ = nullptr;           ///< GPIO provider for hardware access
    IDisplayProvider *displayProvider_ = nullptr;     ///< Display provider for UI operations
    IStyleManager *styleManager_ = nullptr;           ///< Style service for UI theming
    InterruptManager *interruptManager_ = nullptr;    ///< Interrupt manager for button function injection
    IConfigurationManager *configurationManager_ = nullptr; ///< Preference service for configuration settings

    // ========== Cached Service References ==========
    class ErrorManager& errorManager_;                ///< Cached ErrorManager reference
};