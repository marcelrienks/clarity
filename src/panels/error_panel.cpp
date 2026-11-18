#include "panels/error_panel.h"
#include "components/error_component.h"
#include "definitions/constants.h"
#include "ui/theme.h"
#include "utilities/logging.h"
#include <Arduino.h>
#include <algorithm>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs error panel with required service dependencies
 * @param gpio GPIO provider for hardware interaction
 * @param display Display provider for screen management
 * @param theme Theme for the shared styles and the palette
 *
 * Creates error panel with stack-allocated error component for memory efficiency.
 * Initializes component state and sets error panel as active in ErrorManager
 * for proper coordination with the trigger system.
 */
ErrorPanel::ErrorPanel(IGpioProvider *gpio, IDisplayProvider *display, Theme *theme)
    : gpio_provider_(gpio), display_provider_(display), theme_(theme), error_component_(theme),
      component_initialized_(false), current_error_index_(0)
{
    log_v("ErrorPanel constructor called");
}

/**
 * @brief Destructor cleans up error panel resources
 *
 * Safely deletes LVGL screen objects. Stack-allocated component is automatically destroyed.
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

    // Don't restore theme here: the screen manager applies the lights theme when it restores
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

    if (!display_provider_ || !gpio_provider_)
    {
        log_e("ErrorPanel requires display and gpio providers");
        ErrorManager::Instance().ReportCriticalError(panel_names::ERROR,
                                                     "Missing required providers - display or gpio provider is null");
        return;
    }

    screen_ = display_provider_->CreateScreen();

    center_location_ = ComponentLocation(LV_ALIGN_CENTER, 0, 0);

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
    component_initialized_ = true;

    if (!display_provider_)
    {
        log_e("ErrorPanel load requires display provider");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, panel_names::ERROR,
                                             "Cannot render component - display provider is null");
        return;
    }

    // Render the component
    error_component_.Render(screen_, center_location_, display_provider_);

    // Initialize our unviewed error list with all current errors
    current_errors_ = ErrorManager::Instance().GetErrorQueue();
    SortErrorsBySeverity();
    current_error_index_ = 0;

    // Initialize component with the first error
    if (!current_errors_.empty())
    {
        error_component_.UpdateErrorDisplay(current_errors_, current_error_index_);
        log_i("Initialized error panel with %zu unviewed errors", current_errors_.size());
    }

    // Add safety check before screen load
    if (!screen_)
    {
        log_e("Screen is null, cannot load error panel");
        ErrorManager::Instance().ReportCriticalError(panel_names::ERROR, "Cannot load panel - screen creation failed");
        // Error condition - no callback needed
        return;
    }

    lv_screen_load(screen_);

    // The screen manager applied the Error theme (EffectiveTheme) before creating this panel
    if (theme_)
    {
        theme_->ApplyBackground(screen_);
    }

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
    std::vector<ErrorInfo> new_errors = ErrorManager::Instance().GetErrorQueue();

    // Add any new errors that weren't in our original unviewed list
    for (const auto &new_error : new_errors)
    {
        // Check if this error is already in our unviewed list OR was in our original list
        bool already_exists = false;
        for (const auto &existing_error : current_errors_)
        {
            if (existing_error.level == new_error.level && strcmp(existing_error.source, new_error.source) == 0 &&
                strcmp(existing_error.message, new_error.message) == 0)
            {
                already_exists = true;
                break;
            }
        }

        // Also check if it was in our original list (but viewed and removed)
        if (!already_exists)
        {
            for (const auto &viewed_error : viewed_errors_)
            {
                if (viewed_error.level == new_error.level && strcmp(viewed_error.source, new_error.source) == 0 &&
                    strcmp(viewed_error.message, new_error.message) == 0)
                {
                    already_exists = true;
                    break;
                }
            }
        }

        // Add new error if not already present and not previously viewed
        if (!already_exists)
        {
            current_errors_.push_back(new_error);
            SortErrorsBySeverity();
            log_i("Added new error to unviewed list: %s - %s", new_error.source, new_error.message);
        }
    }

    // Update display with current error if we have any
    if (component_initialized_ && !current_errors_.empty())
    {
        // Ensure current index is valid
        if (current_error_index_ >= current_errors_.size())
        {
            current_error_index_ = 0;
        }

        error_component_.UpdateErrorDisplay(current_errors_, current_error_index_);
        log_i("Displaying error %zu/%zu", current_error_index_ + 1, current_errors_.size());
    }

    // No errors remaining: the press that cleared them returned ClearErrors, and the screen manager restores
}

// ========== Action Handler Methods ==========

clarity::core::NavIntent ErrorPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event == clarity::core::ButtonEvent::Short)
    {
        return HandleShortPress();
    }
    if (event == clarity::core::ButtonEvent::Long)
    {
        return HandleLongPress();
    }
    return clarity::core::NavIntent::None;
}

/**
 * @brief Handles short button press events for error cycling
 *
 * Processes short button press events by removing the viewed error and
 * showing the next one. Once the last one is viewed, all errors are cleared
 * and the panel asks the screen manager to restore (Q-12 decides whether
 * that stays).
 */
clarity::core::NavIntent ErrorPanel::HandleShortPress()
{
    log_t("ErrorPanel::HandleShortPress() - Manual cycling to next error");

    if (current_errors_.empty())
    {
        log_w("ErrorPanel::HandleShortPress() - No errors to cycle through");
        return clarity::core::NavIntent::ClearErrors;
    }

    log_t("ErrorPanel::HandleShortPress() - Current error index: %zu/%zu", current_error_index_,
          current_errors_.size());

    // Always advance to next error (cycles back to 0 after last error)
    log_t("ErrorPanel::HandleShortPress() - Advancing to next error");
    AdvanceToNextError();

    // The last error was viewed: AdvanceToNextError() cleared them all
    return current_errors_.empty() ? clarity::core::NavIntent::ClearErrors : clarity::core::NavIntent::None;
}

/**
 * @brief Handles long button press events for error clearing and panel exit
 *
 * Processes long button press events by clearing all errors from the error
 * manager and exiting the error panel. Marks the error panel as inactive and
 * triggers the restoration logic to return to the appropriate previous panel
 * or default state. This provides the primary mechanism for dismissing errors.
 */
clarity::core::NavIntent ErrorPanel::HandleLongPress()
{
    log_t("ErrorPanel: Long press - clearing all errors and exiting panel");

    // Clear all errors from the error manager
    ErrorManager::Instance().ClearAllErrors();

    // The screen manager restores: it shows a key or lock override that is still active, otherwise the user panel
    return clarity::core::NavIntent::ClearErrors;
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
    std::sort(current_errors_.begin(), current_errors_.end(), [](const ErrorInfo &a, const ErrorInfo &b)
              { return static_cast<int>(a.level) > static_cast<int>(b.level); });
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
    if (current_errors_.empty())
    {
        log_w("AdvanceToNextError: No errors to advance through");
        return;
    }

    [[maybe_unused]] size_t old_index = current_error_index_;

    // Remove the current error from both our local list AND the ErrorManager
    if (current_error_index_ < current_errors_.size())
    {
        ErrorInfo viewed_error = current_errors_[current_error_index_];
        log_i("Removing viewed error: %s - %s", viewed_error.source, viewed_error.message);

        // Track this error as viewed
        viewed_errors_.push_back(viewed_error);

        // Note: We cannot remove individual errors from ErrorManager, only clear all

        // Remove from our local list
        current_errors_.erase(current_errors_.begin() + static_cast<std::ptrdiff_t>(current_error_index_));
    }

    // Adjust index for the removal - stay at same position (which now shows next error)
    // or wrap to beginning if we were at the end
    if (current_error_index_ >= current_errors_.size())
    {
        current_error_index_ = 0;
    }

    log_i("AdvanceToNextError: Removed viewed error %zu, now showing error %zu/%zu", old_index,
          current_error_index_ + 1, current_errors_.size());

    // Update the component to display the new current error
    if (component_initialized_ && !current_errors_.empty())
    {
        // Direct access to concrete component
        log_i("AdvanceToNextError: Updating error component display");
        error_component_.UpdateErrorDisplay(current_errors_, current_error_index_);
        log_i("AdvanceToNextError: Error component updated successfully");
    }
    else if (current_errors_.empty())
    {
        log_i("AdvanceToNextError: No more errors to display - all have been viewed");
        // Clear all remaining errors from ErrorManager since we've viewed them all
        ErrorManager::Instance().ClearAllErrors();
    }
    else
    {
        log_e("AdvanceToNextError: Error component is null - cannot update display");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, panel_names::ERROR,
                                             "Error component is null - cannot update display");
    }
}