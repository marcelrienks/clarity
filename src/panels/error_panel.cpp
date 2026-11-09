#include "panels/error_panel.h"
#include "components/error_component.h"
#include "managers/style_manager.h"
#include "managers/panel_manager.h"
#include "utilities/logging.h"
#include "definitions/constants.h"
#include <Arduino.h>
#include <algorithm>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs error panel with required service dependencies
 * @param gpio GPIO provider for hardware interaction
 * @param display Display provider for screen management
 * @param styleManager Style service for theme management
 *
 * Creates error panel with stack-allocated error component for memory efficiency.
 * Initializes component state and sets error panel as active in ErrorManager
 * for proper coordination with the trigger system.
 */
ErrorPanel::ErrorPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *styleManager,
                       IPanelManager *panelManager)
    : gpioProvider_(gpio), displayProvider_(display), styleManager_(styleManager), panelManager_(panelManager),
      errorComponent_(styleManager), componentInitialized_(false), panelLoaded_(false), previousTheme_(), currentErrorIndex_(0)
{
    log_v("ErrorPanel constructor called");
}

/**
 * @brief Destructor cleans up error panel resources and resets error state
 *
 * Safely deletes LVGL screen objects and resets ErrorManager error panel
 * active flag. Allows trigger system to resume normal operation after
 * error panel cleanup. Stack-allocated component is automatically destroyed.
 */
ErrorPanel::~ErrorPanel()
{
    log_v("~ErrorPanel() destructor called");

    if (screen_)
    {
        lv_obj_delete(screen_);
        screen_ = nullptr;
    }

    // Components delete no LVGL objects, so destroying them after the screen is safe

    // Reset error panel active flag when panel is destroyed
    ErrorManager::Instance().SetErrorPanelActive(false);

    // Don't restore theme here - let the trigger system manage themes
    // The CheckRestoration() method will apply the correct theme based on active triggers
}

// ========== Public Interface Methods ==========

/**
 * @brief Initializes error panel UI structure and loads current errors
 *
 * Creates LVGL screen, applies dark theme for error visibility, initializes
 * stack-allocated error component, and loads current error queue. Sets up
 * button action handlers for error navigation and exit functionality.
 */
void ErrorPanel::Init()
{
    log_v("Init() called");

    if (!displayProvider_ || !gpioProvider_)
    {
        log_e("ErrorPanel requires display and gpio providers");
        ErrorManager::Instance().ReportCriticalError(PanelNames::ERROR,
                                                     "Missing required providers - display or gpio provider is null");
        return;
    }

    screen_ = displayProvider_->CreateScreen();

    // Store current theme before switching to ERROR theme (but don't apply it yet)
    if (styleManager_)
    {
        previousTheme_ = styleManager_->GetCurrentTheme();
    }

    centerLocation_ = ComponentLocation(LV_ALIGN_CENTER, 0, 0);

    // Set error panel as active in ErrorManager
    ErrorManager::Instance().SetErrorPanelActive(true);
    log_i("Error panel activated - switching to error display mode");
    
    log_i("ErrorPanel initialization completed");
}

/**
 * @brief Loads error panel UI components and applies error theme
 *
 * Renders error component on screen, fetches current error queue, and applies
 * ERROR theme for optimal visibility. Sets up screen loading callback and
 * loads the LVGL screen. Critical for displaying system errors to the user
 * in automotive applications where error visibility is essential.
 */
void ErrorPanel::Load()
{
    log_v("Load() called");
    // Component is now stack-allocated and initialized in constructor
    componentInitialized_ = true;

    if (!displayProvider_)
    {
        log_e("ErrorPanel load requires display provider");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, PanelNames::ERROR,
                                             "Cannot render component - display provider is null");
        return;
    }

    // Render the component
    errorComponent_.Render(screen_, centerLocation_, displayProvider_);

    // Initialize our unviewed error list with all current errors
    currentErrors_ = ErrorManager::Instance().GetErrorQueue();
    SortErrorsBySeverity();
    currentErrorIndex_ = 0;

    // Initialize component with the first error
    if (!currentErrors_.empty())
    {
        errorComponent_.UpdateErrorDisplay(currentErrors_, currentErrorIndex_);
        log_i("Initialized error panel with %zu unviewed errors", currentErrors_.size());
    }

    lv_obj_add_event_cb(screen_, ErrorPanel::ShowPanelCompletionCallback, LV_EVENT_SCREEN_LOADED, this);

    // Add safety check before screen load
    if (!screen_)
    {
        log_e("Screen is null, cannot load error panel");
        ErrorManager::Instance().ReportCriticalError(PanelNames::ERROR, "Cannot load panel - screen creation failed");
        // Error condition - no callback needed
        return;
    }

    lv_screen_load(screen_);

    // Ensure ERROR theme is applied when panel is loaded
    if (styleManager_)
    {
        styleManager_->SetTheme(Themes::ERROR);
        styleManager_->ApplyThemeToScreen(screen_);
    }

    panelLoaded_ = true;
    
    log_t("ErrorPanel loaded successfully");
}

/**
 * @brief Updates error panel with current error data and manages auto-restoration
 *
 * Monitors ErrorManager for error queue changes, updates display when errors
 * change, and triggers auto-restoration when no errors remain. Handles error
 * sorting by severity and maintains current error index for navigation.
 * Essential for real-time error monitoring in automotive systems.
 */
void ErrorPanel::Update()
{
    log_v("Update() called");

    // Check for any NEW errors from ErrorManager (but don't re-add removed ones)
    std::vector<ErrorInfo> newErrors = ErrorManager::Instance().GetErrorQueue();


    // Add any new errors that weren't in our original unviewed list
    for (const auto& newError : newErrors)
    {
        // Check if this error is already in our unviewed list OR was in our original list
        bool alreadyExists = false;
        for (const auto& existingError : currentErrors_)
        {
            if (existingError.level == newError.level &&
                strcmp(existingError.source, newError.source) == 0 &&
                strcmp(existingError.message, newError.message) == 0)
            {
                alreadyExists = true;
                break;
            }
        }

        // Also check if it was in our original list (but viewed and removed)
        if (!alreadyExists)
        {
            for (const auto& viewedError : viewedErrors_)
            {
                if (viewedError.level == newError.level &&
                    strcmp(viewedError.source, newError.source) == 0 &&
                    strcmp(viewedError.message, newError.message) == 0)
                {
                    alreadyExists = true;
                    break;
                }
            }
        }

        // Add new error if not already present and not previously viewed
        if (!alreadyExists)
        {
            currentErrors_.push_back(newError);
            SortErrorsBySeverity();
            log_i("Added new error to unviewed list: %s - %s", newError.source, newError.message);
        }
    }

    // Update display with current error if we have any
    if (componentInitialized_ && !currentErrors_.empty())
    {
        // Ensure current index is valid
        if (currentErrorIndex_ >= currentErrors_.size())
        {
            currentErrorIndex_ = 0;
        }

        errorComponent_.UpdateErrorDisplay(currentErrors_, currentErrorIndex_);
        log_i("Displaying error %zu/%zu", currentErrorIndex_ + 1, currentErrors_.size());
    }

    // If no errors remain, trigger auto-restoration to previous panel
    if (currentErrors_.empty() && panelLoaded_)
    {
        log_i("ErrorPanel: No errors remaining - triggering auto-restoration");
        ErrorManager::Instance().SetErrorPanelActive(false);

        // Auto-restore using CheckRestoration to handle active triggers. This must run in
        // release builds too: Update() runs with the UI BUSY, and nothing else sets it IDLE
        if (panelManager_)
        {
            log_i("ErrorPanel: No errors remaining - checking for active triggers before restoration");

            // CheckRestoration may replace this panel, so keep the manager pointer and do not touch members afterwards
            IPanelManager *panelManager = panelManager_;
            PanelManager::Instance().CheckRestoration();

            // Nothing replaced this panel (no restoration target): still leave the UI in a defined state
            if (strcmp(panelManager->GetCurrentPanel(), PanelNames::ERROR) == 0)
            {
                panelManager->SetUiState(UIState::IDLE);
            }

            return; // Exit early to prevent callback execution on replaced panel
        }
    }

    // Reset UI state to IDLE to allow interrupt processing (similar to ConfigPanel pattern)
    if (panelManager_)
    {
        panelManager_->SetUiState(UIState::IDLE);
    }
    // Error panel updates are handled internally - no notification needed
}

/**
 * @brief Static callback function for LVGL screen load completion
 * @param event LVGL event containing panel instance data
 *
 * Critical callback that registers button action handlers when error panel
 * screen loading is complete. Ensures UI is ready before enabling user
 * interactions. Essential for proper error panel navigation functionality.
 */
void ErrorPanel::ShowPanelCompletionCallback(lv_event_t *event)
{
    log_i("ErrorPanel::ShowPanelCompletionCallback() called - CRITICAL for UI state");

    if (!event)
    {
        log_e("ShowPanelCompletionCallback: event is null!");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, PanelNames::ERROR,
                                            "ShowPanelCompletionCallback received null event");
        return;
    }

    auto thisInstance = static_cast<ErrorPanel *>(lv_event_get_user_data(event));
    if (thisInstance)
    {
        // Set UI state to IDLE after static panel loads so triggers can be evaluated again
        if (thisInstance->panelManager_)
        {
            log_i("ErrorPanel: Setting UI state to IDLE - this unblocks main loop processing");
            thisInstance->panelManager_->SetUiState(UIState::IDLE);
            log_i("ErrorPanel: UI state set to IDLE successfully");
        }
        else
        {
            log_e("ErrorPanel::ShowPanelCompletionCallback: panelManager_ is null!");
            ErrorManager::Instance().ReportError(ErrorLevel::ERROR, PanelNames::ERROR,
                                                "PanelService is null in completion callback");
        }
    }
    else
    {
        log_e("ErrorPanel::ShowPanelCompletionCallback: thisInstance is null!");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, PanelNames::ERROR,
                                            "Instance is null in completion callback");
    }
}

/**
 * @brief Injects manager dependencies for panel operation
 * @param panelManager Panel service for navigation control
 * @param styleManager Style service for theme management
 *
 * Dependency injection method that provides panel with required manager
 * interfaces. Called during panel creation to establish service dependencies
 * needed for proper panel operation and lifecycle management.
 */

// ========== IActionService Interface Implementation ==========

// ========== Action Handler Methods ==========

/**
 * @brief Handles short button press events for error cycling
 *
 * Processes short button press events by advancing to the next error in the
 * current error list. Cycles through all errors sequentially, wrapping back
 * to the first error after reaching the last one. Does not clear errors or
 * exit the panel - only provides navigation through the error list.
 */
void ErrorPanel::HandleShortPress()
{
    log_t("ErrorPanel::HandleShortPress() - Manual cycling to next error");
    
    if (currentErrors_.empty())
    {
        log_w("ErrorPanel::HandleShortPress() - No errors to cycle through");
        return;
    }
    
    log_t("ErrorPanel::HandleShortPress() - Current error index: %zu/%zu", 
          currentErrorIndex_, currentErrors_.size());
    
    // Always advance to next error (cycles back to 0 after last error)
    log_t("ErrorPanel::HandleShortPress() - Advancing to next error");
    AdvanceToNextError();
    
    // Short press only cycles through errors, never exits
    // User must use long press to clear and exit
}

/**
 * @brief Handles long button press events for error clearing and panel exit
 *
 * Processes long button press events by clearing all errors from the error
 * manager and exiting the error panel. Marks the error panel as inactive and
 * triggers the restoration logic to return to the appropriate previous panel
 * or default state. This provides the primary mechanism for dismissing errors.
 */
void ErrorPanel::HandleLongPress()
{
    log_t("ErrorPanel: Long press - clearing all errors and exiting panel");
    
    // Clear all errors from the error manager
    ErrorManager::Instance().ClearAllErrors();
    
    // Set error panel as inactive
    ErrorManager::Instance().SetErrorPanelActive(false);
    
    // Use standard restoration logic for all builds
    // This will check for other active triggers and restore appropriately
    PanelManager::Instance().CheckRestoration();
    log_i("ErrorPanel: Triggered restoration check after clearing errors");
}

// ========== Private Methods ==========

/**
 * @brief Sorts the current error list by severity level
 *
 * Arranges errors in priority order with CRITICAL errors first, ERROR level
 * errors in the middle, and WARNING level errors last. This ensures that the
 * most important errors are displayed first when cycling through the error list.
 * Uses a lambda comparator to sort by the numeric severity level values.
 */
void ErrorPanel::SortErrorsBySeverity()
{
    log_v("SortErrorsBySeverity() called");
    
    // Sort errors: CRITICAL (2) first, ERROR (1) middle, WARNING (0) last
    std::sort(currentErrors_.begin(), currentErrors_.end(), 
        [](const ErrorInfo& a, const ErrorInfo& b) {
            return static_cast<int>(a.level) > static_cast<int>(b.level);
        });
    
}

/**
 * @brief Advances to the next error in the current error list
 *
 * Increments the current error index to move to the next error, wrapping back
 * to index 0 when reaching the end of the list. Updates the error component
 * display to show the new current error. Handles empty error lists gracefully
 * and ensures the component is properly initialized before updating the display.
 */
void ErrorPanel::AdvanceToNextError()
{
    if (currentErrors_.empty())
    {
        log_w("AdvanceToNextError: No errors to advance through");
        return;
    }

    [[maybe_unused]] size_t oldIndex = currentErrorIndex_;

    // Remove the current error from both our local list AND the ErrorManager
    if (currentErrorIndex_ < currentErrors_.size())
    {
        ErrorInfo viewedError = currentErrors_[currentErrorIndex_];
        log_i("Removing viewed error: %s - %s", viewedError.source, viewedError.message);

        // Track this error as viewed
        viewedErrors_.push_back(viewedError);

        // Note: We cannot remove individual errors from ErrorManager, only clear all

        // Remove from our local list
        currentErrors_.erase(currentErrors_.begin() + currentErrorIndex_);
    }

    // Adjust index for the removal - stay at same position (which now shows next error)
    // or wrap to beginning if we were at the end
    if (currentErrorIndex_ >= currentErrors_.size())
    {
        currentErrorIndex_ = 0;
    }

    log_i("AdvanceToNextError: Removed viewed error %zu, now showing error %zu/%zu",
          oldIndex, currentErrorIndex_ + 1, currentErrors_.size());

    // Update the component to display the new current error
    if (componentInitialized_ && !currentErrors_.empty())
    {
        // Direct access to concrete component
        log_i("AdvanceToNextError: Updating error component display");
        errorComponent_.UpdateErrorDisplay(currentErrors_, currentErrorIndex_);
        log_i("AdvanceToNextError: Error component updated successfully");
    }
    else if (currentErrors_.empty())
    {
        log_i("AdvanceToNextError: No more errors to display - all have been viewed");
        // Clear all remaining errors from ErrorManager since we've viewed them all
        ErrorManager::Instance().ClearAllErrors();
    }
    else
    {
        log_e("AdvanceToNextError: Error component is null - cannot update display");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, PanelNames::ERROR,
                                            "Error component is null - cannot update display");
    }
}