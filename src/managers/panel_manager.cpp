#include "managers/panel_manager.h"
#include "definitions/constants.h"
#include "utilities/logging.h"

// Static instance for singleton pattern
static PanelManager *instance_ptr = nullptr;
#include "interfaces/i_action_handler.h"
// Direct panel includes (factory pattern eliminated)
#include "panels/config_panel.h"
#include "panels/error_panel.h"
#include "panels/key_panel.h"
#include "panels/lock_panel.h"
#include "panels/oem_oil_panel.h"
#include "panels/splash_panel.h"
// ActionManager include removed - button handling moved to handler-based system
#include "definitions/constants.h"
#include "managers/error_manager.h"
#include "managers/interrupt_manager.h"
#include "utilities/ticker.h"
#include <cstring>
#include <esp32-hal-log.h>

// ========== Constructors/Destructor ==========

/**
 * @brief Constructs the PanelManager with all required service dependencies
 * @param display Display provider interface for panel rendering
 * @param gpio GPIO provider interface for hardware interaction
 * @param styleManager Style service interface for theme management
 * @param preferenceService Preference service interface for configuration persistence
 * @param interruptManager Interrupt manager for button and trigger handling
 *
 * Initializes the panel manager with dependency injection pattern. Validates all
 * required dependencies and sets up the default oil panel as the initial panel.
 * Establishes singleton instance for global access by interrupt system.
 */
PanelManager::PanelManager(IDisplayProvider *display, IGpioProvider *gpio, IStyleManager *style_manager,
                           IConfigurationManager *configuration_manager, InterruptManager *interrupt_manager)
    : gpio_provider_(gpio), display_provider_(display), style_manager_(style_manager),
      interrupt_manager_(interrupt_manager), configuration_manager_(configuration_manager),
      error_manager_(ErrorManager::Instance())
{
    log_v("PanelManager() constructor called");
    if (!display || !gpio || !style_manager || !configuration_manager)
    {
        log_e("PanelManager requires all dependencies: display, gpio, styleManager, and "
              "preferenceService");
        error_manager_.ReportCriticalError(
            "PanelManager",
            "Missing required dependencies - display, gpio, styleManager, or preferenceService is null");
        // In a real embedded system, you might want to handle this more gracefully
        return;
    }

    // Set defaults
    current_panel_ = panel_names::OIL;
    restoration_panel_ = panel_names::OIL;

    // Initialize restoration panel from configuration so startup trigger-driven loads
    // can restore to the user's configured default panel without main() intervention.
    if (configuration_manager_)
    {
        std::string default_panel = panel_names::OIL;
        if (auto name_value =
                configuration_manager_->QueryConfig<std::string>(config_constants::keys::SYSTEM_DEFAULT_PANEL))
        {
            default_panel = *name_value;
        }
        restoration_panel_ = default_panel;
        log_i("PanelManager: restoration panel initialized to '%s' from configuration", restoration_panel_.c_str());
    }

    // Set singleton instance
    instance_ptr = this;
}

/**
 * @brief Destructor that cleans up panel resources and singleton reference
 *
 * Safely destroys the current panel and clears the singleton instance pointer
 * to prevent dangling references. Ensures proper cleanup of LVGL objects
 * and prevents memory leaks.
 */
PanelManager::~PanelManager()
{
    log_v("~PanelManager() destructor called");
    panel_.reset();

    // Clear singleton instance
    if (instance_ptr == this)
    {
        instance_ptr = nullptr;
    }
}

// ========== Static Methods ==========

/**
 * @brief Provides singleton access to the PanelManager instance
 * @return Reference to the singleton PanelManager instance
 *
 * Used by the interrupt system to access panel management functionality.
 * Must be called after PanelManager has been initialized through the
 * ManagerFactory. This provides global access while maintaining the
 * dependency injection pattern for the main initialization.
 */
PanelManager &PanelManager::Instance()
{
    if (!instance_ptr)
    {
        log_e("PanelManager::Instance() called before initialization");
        ErrorManager::Instance().ReportCriticalError(
            "PanelManager", "Instance() called before initialization - panels will not function");
        // In embedded systems, we need a valid instance
        // This should be set during ManagerFactory initialization
    }
    return *instance_ptr;
}

// ========== IPanelManager Implementation ==========

/**
 * @brief Initializes the panel service to control the flow and rendering of all panels
 *
 * Handles LVGL tasks to ensure proper graphics initialization. Uses direct panel
 * instantiation rather than factory pattern for better performance and memory usage.
 * Hardware providers are already injected via constructor dependency injection.
 */
void PanelManager::Init()
{
    log_v("Init() called");

    Ticker::HandleLvTasks();
    log_i("PanelManager initialization completed");
}

/**
 * @brief Sets current UI state for synchronization with interrupt system
 * @param state New UI state (BUSY or IDLE)
 *
 * Updates the UI state to coordinate with the interrupt manager. BUSY state
 * prevents triggers from interrupting panel animations, while IDLE state
 * allows triggers to be processed. Used for performance optimization.
 */
void PanelManager::SetUiState(UIState state)
{
    log_v("SetUiState() called with state: %s", UIStateToString(state));
    ui_state_ = state;
}

/**
 * @brief Gets the current UI state
 * @return Current UI state (BUSY or IDLE)
 *
 * Returns the current UI state for use by the interrupt manager to determine
 * when it's safe to process triggers. BUSY state indicates panel transitions
 * or animations are in progress.
 */
UIState PanelManager::GetUiState() const
{
    return ui_state_;
}

/**
 * @brief Requests a panel transition; it is applied by ApplyPendingTransition()
 * @param panel_name Name of the panel to create and load
 * @param isTriggerDriven Whether this panel load was triggered by hardware event
 *
 * Never swaps panels itself, so a panel can call this from its own button handler or an LVGL
 * callback without being destroyed while it is executing (D-13). A later request replaces an
 * earlier one that has not been applied yet.
 */
void PanelManager::CreateAndLoadPanel(const char *panel_name, bool is_trigger_driven)
{
    log_t("Panel transition requested: %s", panel_name);

    pending_.kind = PendingKind::Load;
    pending_.panel_name = panel_name;
    pending_.is_trigger_driven = is_trigger_driven;
}

/**
 * @brief Applies the pending transition request, if any (called once per main loop)
 *
 * Runs outside any panel or LVGL callback, so replacing the current panel is safe here.
 */
void PanelManager::ApplyPendingTransition()
{
    if (pending_.kind == PendingKind::None)
    {
        return;
    }

    // Take the request first: applying it can record a new one (a trigger loading its panel)
    const PendingKind kind = pending_.kind;
    const std::string panel_name = std::move(pending_.panel_name);
    const bool is_trigger_driven = pending_.is_trigger_driven;
    pending_ = PendingTransition{};

    switch (kind)
    {
    case PendingKind::Load:
        LoadRequested(panel_name.c_str(), is_trigger_driven);
        break;
    case PendingKind::Restore:
        RestoreNow();
        break;
    case PendingKind::SplashDone:
        // Ignore a completion that arrives after something else replaced the splash
        if (current_panel_ == panel_names::SPLASH && !splash_target_panel_.empty())
        {
            const std::string target = std::move(splash_target_panel_);
            splash_target_panel_.clear();
            log_i("Splash panel completed - transitioning to target panel: %s", target.c_str());
            CreateAndLoadPanelDirect(target.c_str(), splash_target_trigger_driven_);
        }
        break;
    case PendingKind::None:
        break;
    }
}

/**
 * @brief Loads a requested panel, with the splash first when enabled for user-initiated loads
 */
void PanelManager::LoadRequested(const char *panel_name, bool is_trigger_driven)
{

    // Check if splash screen should be shown - only if NOT trigger-driven and splash is enabled
    bool show_splash = false;
    if (configuration_manager_ && !is_trigger_driven)
    {
        // Query system configuration for splash screen setting
        if (auto splash_value = configuration_manager_->QueryConfig<bool>(config_constants::keys::SYSTEM_SHOW_SPLASH))
        {
            show_splash = *splash_value;
        }
        else
        {
            show_splash = true; // Default to true
        }
    }

    if (show_splash)
    {
        log_i("Loading panel with splash transition: %s", panel_name);
        CreateAndLoadPanelWithSplash(panel_name, is_trigger_driven);
    }
    else
    {
        log_v("Loading panel directly: %s", panel_name);
        CreateAndLoadPanelDirect(panel_name, is_trigger_driven);
    }
}

/**
 * @brief Updates the currently active panel (called from main loop)
 *
 * Calls the Update method on the current panel and handles LVGL tasks.
 * Sets UI state to BUSY during update to prevent interrupt interference
 * with panel animations and updates. This is the main panel lifecycle
 * method called continuously from the main loop.
 */
void PanelManager::UpdatePanel()
{
    log_v("UpdatePanel() called");
    if (panel_)
    {
        // Set BUSY before updating panel
        SetUiState(UIState::BUSY);

        panel_->Update();
        Ticker::HandleLvTasks();
    }
}

/**
 * @brief Gets the current panel name
 * @return Name of the currently active panel
 *
 * Returns the name of the panel that is currently loaded and active.
 * Used by the interrupt system and other managers to determine the
 * current system state.
 */
const char *PanelManager::GetCurrentPanel() const
{
    return current_panel_.c_str();
}

/**
 * @brief Gets the restoration panel name (panel to restore when triggers are inactive)
 * @return Name of the panel to restore to when no triggers are active
 *
 * Returns the panel that should be restored when all trigger-driven panels
 * are deactivated. This allows the system to return to the user's chosen
 * panel after temporary interruptions like error or key panels.
 */
const char *PanelManager::GetRestorationPanel() const
{
    return restoration_panel_.c_str();
}

/**
 * @brief Checks if the current panel was loaded by a trigger
 * @return true if panel was loaded by hardware trigger, false if user-driven
 *
 * Determines whether the current panel was activated by a hardware trigger
 * (like key insertion or error condition) or by user action (button press).
 * Used for restoration logic to determine if the panel should be restored
 * when triggers become inactive.
 */
bool PanelManager::IsCurrentPanelTriggerDriven() const
{
    log_v("IsCurrentPanelTriggerDriven() called");
    return current_panel_is_trigger_driven_;
}

/**
 * @brief Callback executed when trigger-driven panel loading is complete
 * @param triggerId Identifier of the trigger that initiated the panel switch
 *
 * Called when a trigger-driven panel has finished loading. Sets the UI state
 * to IDLE to allow other triggers to be processed. This callback ensures
 * proper coordination between the trigger system and panel lifecycle.
 */
void PanelManager::TriggerPanelSwitchCallback([[maybe_unused]] const char *trigger_id)
{
    log_v("TriggerPanelSwitchCallback() called for trigger: %s", trigger_id);
    SetUiState(UIState::IDLE);
}

// ========== IPanelNotificationService Implementation ==========

/**
 * @brief Handles panel load completion notifications
 * @param panel Pointer to the panel that completed loading
 *
 * Part of the IPanelNotificationService interface. Handles special logic
 * for splash screen completion by transitioning to the target panel, or
 * normal panel completion by setting the UI state to IDLE. This enables
 * proper panel lifecycle management and transition coordination.
 */
void PanelManager::OnPanelLoadComplete(IPanel *panel)
{
    if (!panel)
    {
        log_e("PanelManager::OnPanelLoadComplete: Panel is null!");
        ErrorManager::Instance().ReportCriticalError("PanelManager",
                                                     "OnPanelLoadComplete called with null panel - transition broken");
        return;
    }

    log_i("Panel load completed for panel: %p", panel);

    // Check if this is a splash panel completion that should trigger target panel load
    if (current_panel_ == panel_names::SPLASH && !splash_target_panel_.empty())
    {
        // Called from the splash's LVGL timer: the splash is deleted later, by ApplyPendingTransition()
        log_i("Splash panel completed - target panel: %s", splash_target_panel_.c_str());
        pending_.kind = PendingKind::SplashDone;
    }
    else
    {
        // Normal panel completion
        // Normal panel completion processing
        PanelCompletionCallback();
    }
}

// ========== IActionHandler Implementation ==========

/**
 * @brief Handles short button press action (new architecture)
 *
 * Part of the IActionHandler interface. Delegates the short press
 * action to the current panel if it implements IActionHandler. This allows
 * panels to define their own button behavior while maintaining a consistent
 * action handling interface.
 */
void PanelManager::HandleShortPress()
{
    log_t("HandleShortPress() called");

    if (!panel_)
    {
        log_w("No active panel for short press action");
        return;
    }

    // Try to cast panel to IActionHandler to handle press
    IActionHandler *action_service = dynamic_cast<IActionHandler *>(panel_.get());
    if (action_service)
    {
        action_service->HandleShortPress();
    }
    else
    {
        // Current panel does not support button actions
    }
}

/**
 * @brief Handles long button press action (new architecture)
 *
 * Part of the IActionHandler interface. Delegates the long press
 * action to the current panel if it implements IActionHandler. This allows
 * panels to define their own button behavior while maintaining a consistent
 * action handling interface.
 */
void PanelManager::HandleLongPress()
{
    log_t("HandleLongPress() called");

    if (!panel_)
    {
        log_w("No active panel for long press action");
        return;
    }

    // Try to cast panel to IActionHandler to handle press
    IActionHandler *action_service = dynamic_cast<IActionHandler *>(panel_.get());
    if (action_service)
    {
        action_service->HandleLongPress();
    }
}

// ========== ITriggerExecutionService Implementation ==========

/**
 * @brief Loads a panel by name (new architecture)
 * @param panel_name Name of the panel to load
 *
 * Part of the ITriggerExecutionService interface. Loads the specified panel
 * and marks it as trigger-driven to enable proper restoration logic when
 * triggers become inactive. Used by the trigger system for automatic panel
 * switching based on hardware events.
 */
void PanelManager::LoadPanel(const char *panel_name)
{
    log_t("LoadPanel() called for: %s", panel_name);
    CreateAndLoadPanel(panel_name, true); // Mark as trigger-driven
}

/**
 * @brief Checks restoration and loads appropriate panel (new architecture)
 *
 * Part of the ITriggerExecutionService interface. Handles panel restoration
 * logic when triggers become inactive. First checks for any active triggers
 * that should take priority, then applies style triggers, and finally restores
 * to the saved restoration panel. Uses direct loading without splash screen
 * for immediate restoration.
 */
void PanelManager::CheckRestoration()
{
    log_t("CheckRestoration() called");

    pending_.kind = PendingKind::Restore;
}

/**
 * @brief Evaluates restoration now (only called by ApplyPendingTransition())
 */
void PanelManager::RestoreNow()
{
    // First check if any PANEL triggers are active that should take priority
    // This ensures we don't restore to oil panel if a trigger is still active
    if (interrupt_manager_)
    {
        // Ask the interrupt manager to evaluate and execute any active triggers
        // If a trigger is active, it will load its panel and we should not restore
        if (interrupt_manager_->CheckAndExecuteHighestPriorityTrigger())
        {
            log_t("Active trigger found and executed - skipping restoration");
            return;
        }

        // Check for active STYLE triggers and apply them BEFORE loading restoration panel
        // This ensures theme triggers like lights are applied to the oil panel
        interrupt_manager_->CheckAndExecuteActiveStyleTriggers();
    }

    // No active panel triggers, proceed with restoration
    if (!restoration_panel_.empty())
    {
        log_t("No blocking interrupts - restoring to '%s'", restoration_panel_.c_str());

        // Restoration should ALWAYS be direct - never use splash screen
        // Splash is only for application startup, not trigger restoration
        CreateAndLoadPanelDirect(restoration_panel_.c_str(), false); // Direct restoration without splash
    }
}

// ========== Helper Methods ==========

/**
 * @brief Updates restoration tracking for panel transitions
 * @param panel_name Name of the panel being loaded
 * @param isTriggerDriven Whether the panel load was trigger-driven
 */
void PanelManager::UpdateRestorationTracking(const char *panel_name, bool is_trigger_driven)
{
    // Skip tracking for splash panel - it's purely transitional
    if (strcmp(panel_name, panel_names::SPLASH) == 0)
    {
        return;
    }

    // Save current panel for restoration if loading a trigger-driven panel
    if (is_trigger_driven && !current_panel_is_trigger_driven_ && !current_panel_.empty())
    {
        restoration_panel_ = current_panel_;
        log_i("Saving current panel '%s' for restoration when triggers deactivate", restoration_panel_.c_str());
    }

    // Track trigger state for non-splash panels
    current_panel_is_trigger_driven_ = is_trigger_driven;
}

/**
 * @brief Injects preference service into panels that support it
 * @param panel_name Name of the panel to inject service into
 */

/**
 * @brief Handles panel creation errors
 * @param panel_name Name of the panel that failed to create
 */
void PanelManager::HandlePanelCreationError(const char *panel_name)
{
    log_e("Failed to create panel: %s", panel_name);

    // Use char buffer to avoid string allocation in error path
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg), "Panel creation failed for: %s", panel_name);
    error_manager_.ReportError(ErrorLevel::ERROR, "PanelManager", error_msg);
}

// ========== Other Public Methods ==========

/**
 * @brief Updates universal button interrupts with current panel's functions
 * @param panel Pointer to the panel whose button functions should be registered
 *
 * Extracts button functions from panels that implement IActionHandler and
 * registers them with the interrupt manager. This enables dynamic button
 * behavior that changes based on the current panel. Validates all function
 * pointers before registration to ensure system stability.
 */
void PanelManager::UpdatePanelButtonFunctions(IPanel *panel)
{
    log_v("UpdatePanelButtonFunctions() called");

    if (!panel || !interrupt_manager_)
    {
        log_e("Cannot update button functions - panel=%p, interrupt_manager=%p", (void *)panel,
              (void *)interrupt_manager_);
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "PanelManager",
                                             "Cannot update button functions - invalid panel or interrupt manager");
        return;
    }

    IActionHandler *action_service = dynamic_cast<IActionHandler *>(panel);
    if (!action_service)
    {
        log_e("UpdatePanelButtonFunctions: Panel does not implement IActionHandler");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "PanelManager",
                                             "Panel does not implement IActionHandler");
        return;
    }

    log_i("UpdatePanelButtonFunctions: Setting current panel - panel=%p", (void *)action_service);

    // Set the current panel for direct method calls
    interrupt_manager_->SetCurrentPanel(action_service);

    log_i("UpdatePanelButtonFunctions: Successfully updated universal button interrupts with panel functions");
}

// ========== Private Methods ==========

/**
 * @brief Creates a panel based on the given type name
 * @param panel_name The type name of the panel to be created
 * @return Interface type representing the panel, or nullptr if creation fails
 *
 * Factory method that instantiates panels directly without using a factory pattern
 * for better performance and memory usage. Each panel type is created with the
 * required dependencies injected. Reports errors for unknown panel types.
 */
std::shared_ptr<IPanel> PanelManager::CreatePanel(const char *panel_name)
{
    log_v("CreatePanel() called for: %s", panel_name);

    // Direct panel instantiation (factory pattern eliminated)

    if (strcmp(panel_name, panel_names::SPLASH) == 0)
    {
        return std::make_shared<SplashPanel>(gpio_provider_, display_provider_, style_manager_, this,
                                             configuration_manager_);
    }

    if (strcmp(panel_name, panel_names::OIL) == 0)
    {
        return std::make_shared<OemOilPanel>(gpio_provider_, display_provider_, style_manager_, this,
                                             configuration_manager_);
    }

    if (strcmp(panel_name, panel_names::ERROR) == 0)
    {
        return std::make_shared<ErrorPanel>(gpio_provider_, display_provider_, style_manager_, this);
    }

    if (strcmp(panel_name, panel_names::CONFIG) == 0)
    {
        return std::make_shared<ConfigPanel>(gpio_provider_, display_provider_, style_manager_, this,
                                             configuration_manager_);
    }

    if (strcmp(panel_name, panel_names::KEY) == 0)
    {
        return std::make_shared<KeyPanel>(gpio_provider_, display_provider_, style_manager_, this);
    }

    if (strcmp(panel_name, panel_names::LOCK) == 0)
    {
        return std::make_shared<LockPanel>(gpio_provider_, display_provider_, style_manager_, this);
    }

    // Unknown panel type
    log_e("Failed to create panel: %s", panel_name);

    // Use char buffer to avoid string allocation in error path
    char error_msg[128];
    snprintf(error_msg, sizeof(error_msg), "Failed to create panel: %s", panel_name);
    error_manager_.ReportError(ErrorLevel::ERROR, "PanelManager", error_msg);
    return nullptr;
}

/**
 * @brief Internal method to create and load a panel directly without splash
 * @param panel_name Name of the panel to create and load
 * @param isTriggerDriven Whether this panel load was triggered by hardware event
 *
 * Core panel creation and loading logic. Handles restoration panel tracking,
 * dependency injection for specific panel types, theme application, and button
 * function registration. Sets UI state appropriately during the loading process.
 */
void PanelManager::CreateAndLoadPanelDirect(const char *panel_name, bool is_trigger_driven)
{
    log_v("CreateAndLoadPanelDirect() called for: %s", panel_name);

    // Update restoration tracking for non-splash panels
    UpdateRestorationTracking(panel_name, is_trigger_driven);

    // Create the new panel first; on failure the current panel stays on screen
    std::shared_ptr<IPanel> new_panel = CreatePanel(panel_name);
    if (!new_panel)
    {
        HandlePanelCreationError(panel_name);
        return;
    }

    // Dependencies already injected via constructor

    // Initialize panel
    new_panel->Init();

    // Apply current theme from preferences BEFORE panel is loaded
    // This ensures correct theme is set before any rendering happens
    if (style_manager_)
    {
        style_manager_->ApplyCurrentTheme();
    }

    // Swap: the old panel stays alive until the new screen is the active one (D-13)
    std::shared_ptr<IPanel> old_panel = std::move(panel_);
    panel_ = std::move(new_panel);

    // Update current panel directly (PanelNames are static constants)
    current_panel_ = panel_name;

    // Update universal button interrupts with this panel's functions
    UpdatePanelButtonFunctions(panel_.get());

    // Set BUSY before loading panel
    SetUiState(UIState::BUSY);

    panel_->Load();
    old_panel.reset();
    Ticker::HandleLvTasks();

    log_v("CreateAndLoadPanelDirect() completed");
}

/**
 * @brief Internal method to load a panel after first showing a splash screen transition
 * @param panel_name Name of the target panel to load after splash
 * @param isTriggerDriven Whether the target panel load was triggered by hardware event
 *
 * Used only during application startup, never during restoration. Stores the
 * target panel information and loads the splash screen first. The splash panel
 * will transition to the target panel when its display duration expires.
 */
void PanelManager::CreateAndLoadPanelWithSplash(const char *panel_name, bool is_trigger_driven)
{
    log_v("CreateAndLoadPanelWithSplash() called for: %s", panel_name);

    // Store the target panel name for after splash completion
    // Using std::string for safe data storage (see docs/guidelines.md)
    splash_target_panel_ = panel_name;

    // Store the original trigger state to preserve restoration behavior
    splash_target_trigger_driven_ = is_trigger_driven;

    // Splash panel itself is neutral - doesn't affect restoration logic
    // Use false for splash since it's just a transitional UI element
    CreateAndLoadPanelDirect(panel_names::SPLASH, false);
}

/**
 * @brief Callback executed when normal panel loading is complete
 *
 * Sets the UI state to IDLE when panel loading completes, allowing the
 * interrupt system to process triggers. This completes the panel loading
 * lifecycle and signals that the system is ready for user interaction.
 */
void PanelManager::PanelCompletionCallback()
{
    log_v("PanelCompletionCallback() called");

    // Set IDLE when panel operation completes
    SetUiState(UIState::IDLE);
}