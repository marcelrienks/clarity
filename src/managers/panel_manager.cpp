#include "managers/panel_manager.h"
#include "utilities/logging.h"
#include "definitions/constants.h"

// Static instance for singleton pattern
static PanelManager* instancePtr_ = nullptr;
#include "interfaces/i_action_handler.h"
// Direct panel includes (factory pattern eliminated)
#include "panels/splash_panel.h"
#include "panels/oem_oil_panel.h"
#include "panels/error_panel.h"
#include "panels/config_panel.h"
#include "panels/key_panel.h"
#include "panels/lock_panel.h"
// ActionManager include removed - button handling moved to handler-based system
#include "managers/error_manager.h"
#include "managers/interrupt_manager.h"
#include "utilities/ticker.h"
#include "definitions/constants.h"
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
PanelManager::PanelManager(IDisplayProvider *display, IGpioProvider *gpio, IStyleManager *styleManager,
                           IConfigurationManager *configurationManager, InterruptManager* interruptManager)
    : gpioProvider_(gpio), displayProvider_(display), styleManager_(styleManager),
      interruptManager_(interruptManager), configurationManager_(configurationManager),
      errorManager_(ErrorManager::Instance())
{
    log_v("PanelManager() constructor called");
    if (!display || !gpio || !styleManager || !configurationManager)
    {
        log_e("PanelManager requires all dependencies: display, gpio, styleManager, and "
              "preferenceService");
        errorManager_.ReportCriticalError(
            "PanelManager",
            "Missing required dependencies - display, gpio, styleManager, or preferenceService is null");
        // In a real embedded system, you might want to handle this more gracefully
        return;
    }

    // Set defaults
    currentPanel_ = PanelNames::OIL;
    restorationPanel_ = PanelNames::OIL;

    // Initialize restoration panel from configuration so startup trigger-driven loads
    // can restore to the user's configured default panel without main() intervention.
    if (configurationManager_)
    {
        std::string defaultPanel = PanelNames::OIL;
        if (auto nameValue = configurationManager_->QueryConfig<std::string>(ConfigConstants::Keys::SYSTEM_DEFAULT_PANEL)) {
            defaultPanel = *nameValue;
        }
        restorationPanel_ = defaultPanel;
        log_i("PanelManager: restoration panel initialized to '%s' from configuration", restorationPanel_.c_str());
    }

    // Set singleton instance
    instancePtr_ = this;
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
    if (instancePtr_ == this) {
        instancePtr_ = nullptr;
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
PanelManager& PanelManager::Instance() {
    if (!instancePtr_) {
        log_e("PanelManager::Instance() called before initialization");
        ErrorManager::Instance().ReportCriticalError("PanelManager",
                                                     "Instance() called before initialization - panels will not function");
        // In embedded systems, we need a valid instance
        // This should be set during ManagerFactory initialization
    }
    return *instancePtr_;
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

    Ticker::handleLvTasks();
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
    uiState_ = state;
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
    return uiState_;
}

/**
 * @brief Creates and loads a panel by name with optional splash screen
 * @param panelName Name of the panel to create and load
 * @param isTriggerDriven Whether this panel load was triggered by hardware event
 *
 * Main panel loading method that handles splash screen logic. Shows splash
 * screen only for user-initiated loads (not trigger-driven) and when enabled
 * in preferences. Trigger-driven loads bypass splash for immediate response.
 */
void PanelManager::CreateAndLoadPanel(const char *panelName, bool isTriggerDriven)
{
    log_t("Panel transition requested: %s", panelName);

    // Check if splash screen should be shown - only if NOT trigger-driven and splash is enabled
    bool showSplash = false;
    if (configurationManager_ && !isTriggerDriven)
    {
        // Query system configuration for splash screen setting
        if (auto splashValue = configurationManager_->QueryConfig<bool>(ConfigConstants::Keys::SYSTEM_SHOW_SPLASH)) {
            showSplash = *splashValue;
        } else {
            showSplash = true; // Default to true
        }
    }

    if (showSplash)
    {
        log_i("Loading panel with splash transition: %s", panelName);
        CreateAndLoadPanelWithSplash(panelName, isTriggerDriven);
    }
    else
    {
        log_v("Loading panel directly: %s", panelName);
        CreateAndLoadPanelDirect(panelName, isTriggerDriven);
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
        Ticker::handleLvTasks();
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
    return currentPanel_.c_str();
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
    return restorationPanel_.c_str();
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
    return currentPanelIsTriggerDriven_;
}

/**
 * @brief Callback executed when trigger-driven panel loading is complete
 * @param triggerId Identifier of the trigger that initiated the panel switch
 *
 * Called when a trigger-driven panel has finished loading. Sets the UI state
 * to IDLE to allow other triggers to be processed. This callback ensures
 * proper coordination between the trigger system and panel lifecycle.
 */
void PanelManager::TriggerPanelSwitchCallback(const char *triggerId)
{
    log_v("TriggerPanelSwitchCallback() called for trigger: %s", triggerId);
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
void PanelManager::OnPanelLoadComplete(IPanel* panel) {
    if (!panel) {
        log_e("PanelManager::OnPanelLoadComplete: Panel is null!");
        ErrorManager::Instance().ReportCriticalError("PanelManager",
                                                     "OnPanelLoadComplete called with null panel - transition broken");
        return;
    }

    log_i("Panel load completed for panel: %p", panel);

    // Check if this is a splash panel completion that should trigger target panel load
    if (currentPanel_ == PanelNames::SPLASH && !splashTargetPanel_.empty()) {
        log_i("Splash panel completed - transitioning to target panel: %s", splashTargetPanel_.c_str());
        // Splash target panel processing
        SplashCompletionCallback(splashTargetPanel_.c_str());

        // Clear the target panel after using it
        splashTargetPanel_.clear();
    } else {
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
void PanelManager::HandleShortPress() {
    log_t("HandleShortPress() called");

    if (!panel_) {
        log_w("No active panel for short press action");
        return;
    }

    // Try to cast panel to IActionHandler to handle press
    IActionHandler* actionService = dynamic_cast<IActionHandler*>(panel_.get());
    if (actionService) {
        actionService->HandleShortPress();
    } else {
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
void PanelManager::HandleLongPress() {
    log_t("HandleLongPress() called");

    if (!panel_) {
        log_w("No active panel for long press action");
        return;
    }

    // Try to cast panel to IActionHandler to handle press
    IActionHandler* actionService = dynamic_cast<IActionHandler*>(panel_.get());
    if (actionService) {
        actionService->HandleLongPress();
    }
}

// ========== ITriggerExecutionService Implementation ==========

/**
 * @brief Loads a panel by name (new architecture)
 * @param panelName Name of the panel to load
 *
 * Part of the ITriggerExecutionService interface. Loads the specified panel
 * and marks it as trigger-driven to enable proper restoration logic when
 * triggers become inactive. Used by the trigger system for automatic panel
 * switching based on hardware events.
 */
void PanelManager::LoadPanel(const char* panelName) {
    log_t("LoadPanel() called for: %s", panelName);
    CreateAndLoadPanel(panelName, true);  // Mark as trigger-driven
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
void PanelManager::CheckRestoration() {
    log_t("CheckRestoration() called");

    // First check if any PANEL triggers are active that should take priority
    // This ensures we don't restore to oil panel if a trigger is still active
    if (interruptManager_) {
        // Ask the interrupt manager to evaluate and execute any active triggers
        // If a trigger is active, it will load its panel and we should not restore
        if (interruptManager_->CheckAndExecuteHighestPriorityTrigger()) {
            log_t("Active trigger found and executed - skipping restoration");
            return;
        }

        // Check for active STYLE triggers and apply them BEFORE loading restoration panel
        // This ensures theme triggers like lights are applied to the oil panel
        interruptManager_->CheckAndExecuteActiveStyleTriggers();
    }

    // No active panel triggers, proceed with restoration
    if (!restorationPanel_.empty()) {
        log_t("No blocking interrupts - restoring to '%s'", restorationPanel_.c_str());

        // Restoration should ALWAYS be direct - never use splash screen
        // Splash is only for application startup, not trigger restoration
        CreateAndLoadPanelDirect(restorationPanel_.c_str(), false);  // Direct restoration without splash
    }
}

// ========== Helper Methods ==========

/**
 * @brief Updates restoration tracking for panel transitions
 * @param panelName Name of the panel being loaded
 * @param isTriggerDriven Whether the panel load was trigger-driven
 */
void PanelManager::UpdateRestorationTracking(const char* panelName, bool isTriggerDriven) {
    // Skip tracking for splash panel - it's purely transitional
    if (strcmp(panelName, PanelNames::SPLASH) == 0) {
        return;
    }

    // Save current panel for restoration if loading a trigger-driven panel
    if (isTriggerDriven && !currentPanelIsTriggerDriven_ && !currentPanel_.empty()) {
        restorationPanel_ = currentPanel_;
        log_i("Saving current panel '%s' for restoration when triggers deactivate", restorationPanel_.c_str());
    }

    // Track trigger state for non-splash panels
    currentPanelIsTriggerDriven_ = isTriggerDriven;
}

/**
 * @brief Injects preference service into panels that support it
 * @param panelName Name of the panel to inject service into
 */

/**
 * @brief Handles panel creation errors
 * @param panelName Name of the panel that failed to create
 */
void PanelManager::HandlePanelCreationError(const char* panelName) {
    log_e("Failed to create panel: %s", panelName);

    // Use char buffer to avoid string allocation in error path
    char errorMsg[128];
    snprintf(errorMsg, sizeof(errorMsg), "Panel creation failed for: %s", panelName);
    errorManager_.ReportError(ErrorLevel::ERROR, "PanelManager", errorMsg);
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
void PanelManager::UpdatePanelButtonFunctions(IPanel* panel)
{
    log_v("UpdatePanelButtonFunctions() called");

    if (!panel || !interruptManager_)
    {
        log_e("Cannot update button functions - panel=%p, interrupt_manager=%p",
              (void*)panel, (void*)interruptManager_);
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "PanelManager",
                                            "Cannot update button functions - invalid panel or interrupt manager");
        return;
    }

    IActionHandler* actionService = dynamic_cast<IActionHandler*>(panel);
    if (!actionService) {
        log_e("UpdatePanelButtonFunctions: Panel does not implement IActionHandler");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "PanelManager",
                                            "Panel does not implement IActionHandler");
        return;
    }

    log_i("UpdatePanelButtonFunctions: Setting current panel - panel=%p", (void*)actionService);

    // Set the current panel for direct method calls
    interruptManager_->SetCurrentPanel(actionService);

    log_i("UpdatePanelButtonFunctions: Successfully updated universal button interrupts with panel functions");
}

// ========== Private Methods ==========

/**
 * @brief Creates a panel based on the given type name
 * @param panelName The type name of the panel to be created
 * @return Interface type representing the panel, or nullptr if creation fails
 *
 * Factory method that instantiates panels directly without using a factory pattern
 * for better performance and memory usage. Each panel type is created with the
 * required dependencies injected. Reports errors for unknown panel types.
 */
std::shared_ptr<IPanel> PanelManager::CreatePanel(const char *panelName)
{
    log_v("CreatePanel() called for: %s", panelName);

    // Direct panel instantiation (factory pattern eliminated)

    if (strcmp(panelName, PanelNames::SPLASH) == 0) {
        return std::make_shared<SplashPanel>(gpioProvider_, displayProvider_, styleManager_, this, configurationManager_);
    }

    if (strcmp(panelName, PanelNames::OIL) == 0) {
        return std::make_shared<OemOilPanel>(gpioProvider_, displayProvider_, styleManager_, this, configurationManager_);
    }

    if (strcmp(panelName, PanelNames::ERROR) == 0) {
        return std::make_shared<ErrorPanel>(gpioProvider_, displayProvider_, styleManager_, this);
    }

    if (strcmp(panelName, PanelNames::CONFIG) == 0) {
        return std::make_shared<ConfigPanel>(gpioProvider_, displayProvider_, styleManager_, this, configurationManager_);
    }

    if (strcmp(panelName, PanelNames::KEY) == 0) {
        return std::make_shared<KeyPanel>(gpioProvider_, displayProvider_, styleManager_, this);
    }

    if (strcmp(panelName, PanelNames::LOCK) == 0) {
        return std::make_shared<LockPanel>(gpioProvider_, displayProvider_, styleManager_, this);
    }

    // Unknown panel type
    log_e("Failed to create panel: %s", panelName);
    
    // Use char buffer to avoid string allocation in error path
    char errorMsg[128];
    snprintf(errorMsg, sizeof(errorMsg), "Failed to create panel: %s", panelName);
    errorManager_.ReportError(ErrorLevel::ERROR, "PanelManager", errorMsg);
    return nullptr;
}

/**
 * @brief Internal method to create and load a panel directly without splash
 * @param panelName Name of the panel to create and load
 * @param isTriggerDriven Whether this panel load was triggered by hardware event
 *
 * Core panel creation and loading logic. Handles restoration panel tracking,
 * dependency injection for specific panel types, theme application, and button
 * function registration. Sets UI state appropriately during the loading process.
 */
void PanelManager::CreateAndLoadPanelDirect(const char *panelName, bool isTriggerDriven)
{
    log_v("CreateAndLoadPanelDirect() called for: %s", panelName);

    // Update restoration tracking for non-splash panels
    UpdateRestorationTracking(panelName, isTriggerDriven);

    // Clean up existing panel
    if (panel_) {
        panel_.reset();
    }

    // Create new panel
    panel_ = CreatePanel(panelName);
    if (!panel_) {
        HandlePanelCreationError(panelName);
        return;
    }

    // Dependencies already injected via constructor

    // Initialize panel
    panel_->Init();

    // Update current panel directly (PanelNames are static constants)
    currentPanel_ = panelName;

    // Apply current theme from preferences BEFORE panel is loaded
    // This ensures correct theme is set before any rendering happens
    if (styleManager_)
    {
        styleManager_->ApplyCurrentTheme();
    }

    // Update universal button interrupts with this panel's functions
    UpdatePanelButtonFunctions(panel_.get());

    // Set BUSY before loading panel
    SetUiState(UIState::BUSY);

    panel_->Load();
    Ticker::handleLvTasks();

    log_v("CreateAndLoadPanelDirect() completed");
}

/**
 * @brief Internal method to load a panel after first showing a splash screen transition
 * @param panelName Name of the target panel to load after splash
 * @param isTriggerDriven Whether the target panel load was triggered by hardware event
 *
 * Used only during application startup, never during restoration. Stores the
 * target panel information and loads the splash screen first. The splash panel
 * will transition to the target panel when its display duration expires.
 */
void PanelManager::CreateAndLoadPanelWithSplash(const char *panelName, bool isTriggerDriven)
{
    log_v("CreateAndLoadPanelWithSplash() called for: %s", panelName);

    // Store the target panel name for after splash completion
    // Using std::string for safe data storage (see docs/guidelines.md)
    splashTargetPanel_ = panelName;

    // Store the original trigger state to preserve restoration behavior
    splashTargetTriggerDriven_ = isTriggerDriven;

    // Splash panel itself is neutral - doesn't affect restoration logic
    // Use false for splash since it's just a transitional UI element
    CreateAndLoadPanelDirect(PanelNames::SPLASH, false);
}

/**
 * @brief Callback executed when splash screen loading is complete
 * @param panelName Name of the target panel to load after splash
 *
 * Cleans up the splash panel and loads the target panel with the original
 * trigger state preserved. This maintains proper restoration behavior for
 * panels loaded after splash screen transitions.
 */
void PanelManager::SplashCompletionCallback(const char *panelName)
{
    log_v("SplashCompletionCallback() called for panel: %s", panelName);

    panel_.reset();
    Ticker::handleLvTasks();

    // Use the stored trigger state to preserve restoration behavior
    CreateAndLoadPanelDirect(panelName, splashTargetTriggerDriven_);
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