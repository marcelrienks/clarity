#pragma once
#include "definitions/types.h"
#include "interfaces/i_action_handler.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_style_manager.h"

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

class PanelManager : public IPanelManager, public IActionHandler
{
  public:
    // ========== Constructors/Destructor ==========
    PanelManager(IDisplayProvider *display, IGpioProvider *gpio, IStyleManager *style, IConfigurationManager *config,
                 InterruptManager *interrupts = nullptr);
    PanelManager(const PanelManager &) = delete;
    PanelManager &operator=(const PanelManager &) = delete;
    ~PanelManager() override;

    // ========== Static Methods ==========
    // Singleton access for new interrupt architecture
    static PanelManager &Instance();

    // Interface accessors for dependency injection (CRITICAL for testability)
    static IActionHandler &ActionService()
    {
        return Instance();
    }

    // ========== IPanelManager Implementation ==========
    void Init() override;
    void SetUiState(UIState state) override;
    UIState GetUiState() const override;
    void CreateAndLoadPanel(const char *panel_name, bool is_trigger_driven = false) override;
    void UpdatePanel() override;
    const char *GetCurrentPanel() const override;
    const char *GetRestorationPanel() const override;
    bool IsCurrentPanelTriggerDriven() const override;
    void TriggerPanelSwitchCallback(const char *trigger_id) override;

    // ========== Panel Notification Methods ==========
    void OnPanelLoadComplete(IPanel *panel);

    // ========== Deferred Transitions ==========
    /// Applies the request recorded by CreateAndLoadPanel/LoadPanel/CheckRestoration. Call once per loop.
    void ApplyPendingTransition();

    // ========== IActionHandler Implementation ==========
    void HandleShortPress() override;
    void HandleLongPress() override;

    // ========== Trigger Execution Methods ==========
    void LoadPanel(const char *panel_name);
    void CheckRestoration();

    // ========== Other Public Methods ==========
    void UpdatePanelButtonFunctions(IPanel *panel);

  private:
    // ========== Private Methods ==========
    std::shared_ptr<IPanel> CreatePanel(const char *panel_name);
    void CreateAndLoadPanelDirect(const char *panel_name, bool is_trigger_driven = false);
    void CreateAndLoadPanelWithSplash(const char *panel_name, bool is_trigger_driven);
    void LoadRequested(const char *panel_name, bool is_trigger_driven);
    void RestoreNow();
    void PanelCompletionCallback();

    // Helper methods for CreateAndLoadPanelDirect
    void UpdateRestorationTracking(const char *panel_name, bool is_trigger_driven);
    void HandlePanelCreationError(const char *panel_name);

    // ========== Panel State Data Members ==========
    std::string current_panel_ = panel_names::OIL;     ///< Current panel state
    std::string restoration_panel_ = panel_names::OIL; ///< Panel to restore when all triggers are inactive
    std::shared_ptr<IPanel> panel_ = nullptr;
    UIState ui_state_ = UIState::IDLE;             ///< Current UI processing state
    bool current_panel_is_trigger_driven_ = false; ///< Track if current panel is trigger-driven

    // ========== Deferred Transition State ==========
    enum class PendingKind : uint8_t
    {
        None,
        Load,       ///< Load `panel_name`
        Restore,    ///< Evaluate restoration
        SplashDone, ///< Splash finished; load its stored target
    };
    struct PendingTransition
    {
        PendingKind kind = PendingKind::None;
        std::string panel_name;
        bool is_trigger_driven = false;
    };
    PendingTransition pending_; ///< Latest unapplied request; panels never swap themselves

    // ========== Panel Name Storage Data Members ==========
    std::string splash_target_panel_;           ///< Target panel for splash transition
    bool splash_target_trigger_driven_ = false; ///< Preserve trigger state through splash transitions

    // ========== Service Dependencies ==========
    IGpioProvider *gpio_provider_ = nullptr;                 ///< GPIO provider for hardware access
    IDisplayProvider *display_provider_ = nullptr;           ///< Display provider for UI operations
    IStyleManager *style_manager_ = nullptr;                 ///< Style service for UI theming
    InterruptManager *interrupt_manager_ = nullptr;          ///< Interrupt manager for button function injection
    IConfigurationManager *configuration_manager_ = nullptr; ///< Preference service for configuration settings

    // ========== Cached Service References ==========
    class ErrorManager &error_manager_; ///< Cached ErrorManager reference
};