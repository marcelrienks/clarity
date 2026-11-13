#include "components/error_component.h"
#include "definitions/constants.h"
#include <Arduino.h>
#include <algorithm>
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs an error display component with style service dependency
 * @param styleManager Style service for theme management
 *
 * Initializes the error component with style service for theming.
 * Sets all UI element pointers to nullptr and initializes counters to zero.
 */
ErrorComponent::ErrorComponent(IStyleManager *style_manager)
    : style_manager_(style_manager), error_container_(nullptr), error_content_area_(nullptr),
      error_count_label_(nullptr), error_level_label_(nullptr), error_source_label_(nullptr),
      error_message_label_(nullptr), navigation_indicator_(nullptr), current_error_index_(0)
{
    log_v("ErrorComponent() constructor called");
}

/**
 * @brief Destructor for error component
 *
 * LVGL objects are automatically managed by the parent screen,
 * so no manual deletion is needed. The destructor just logs cleanup.
 */
ErrorComponent::~ErrorComponent()
{
    log_v("~ErrorComponent() destructor called");
    // LVGL objects are managed by the parent screen, no manual deletion needed
}

// ========== IComponent Implementation ==========

/**
 * @brief Renders the error component on screen with full error display UI
 * @param screen LVGL screen object to render on
 * @param location Component positioning parameters
 * @param display Display provider interface
 *
 * Creates main container with circular styling for round display, creates
 * single error UI structure, and performs initial error display update.
 * Reports errors if screen or display provider is null.
 */
void ErrorComponent::Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display)
{
    log_v("Render() called");
    if (!screen)
    {
        log_e("ErrorComponent requires valid screen object");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "ErrorComponent", "Cannot render - screen is null");
        return;
    }

    if (!display)
    {
        log_e("ErrorComponent requires display provider");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "ErrorComponent",
                                             "Cannot render - display provider is null");
        return;
    }

    // Create main container using full 240x240 screen size
    error_container_ = lv_obj_create(screen);
    lv_obj_set_size(error_container_, 240, 240); // Full screen size

    // Apply location settings to the container
    lv_obj_align(error_container_, location.align, location.x_offset, location.y_offset);

    // Apply circular styling with 2px colored border from screen edge
    lv_obj_set_style_radius(error_container_, 120, 0);     // Make it circular (half of 240)
    lv_obj_set_style_border_width(error_container_, 2, 0); // 2px colored border from edge

    // Ensure container has transparent background for error panel's dark theme
    lv_obj_set_style_bg_opa(error_container_, LV_OPA_TRANSP, 0);

    // Create the internal UI structure for single error display
    CreateSingleErrorUI(error_container_);

    // Initial update with current errors - fetch and sort immediately
    UpdateErrorDisplay();
}

/**
 * @brief Refreshes error display (currently unused)
 * @param reading Sensor reading value
 *
 * This method is part of IComponent interface but error display updates
 * are controlled by ErrorPanel rather than through sensor readings.
 * Updates come directly from ErrorManager when panel requests them.
 */
void ErrorComponent::Refresh(const Reading &reading)
{
    // For single error display, we'll update directly from ErrorManager rather than using Reading
    // This allows us to get the complete error queue information and maintain current position
    // Note: Don't call UpdateErrorDisplay() here during initialization - let the panel control the display
}

// ========== Public Interface Methods ==========

/**
 * @brief Updates error display by fetching and sorting current errors
 *
 * Retrieves current error queue from ErrorManager, sorts by severity
 * (CRITICAL first, WARNING last), and updates the display with sorted errors.
 */
void ErrorComponent::UpdateErrorDisplay()
{
    // Get current errors from ErrorManager
    std::vector<ErrorInfo> new_errors = ErrorManager::Instance().GetErrorQueue();

    // Sort errors by severity (CRITICAL first, WARNING last)
    std::sort(new_errors.begin(), new_errors.end(), [](const ErrorInfo &a, const ErrorInfo &b)
              { return static_cast<int>(a.level) > static_cast<int>(b.level); });

    UpdateErrorDisplay(new_errors);
}

/**
 * @brief Updates error display with provided error list
 * @param errors Vector of ErrorInfo structures to display
 *
 * Stores error list, validates current index, displays the current error,
 * and updates container border color based on error severity.
 */
void ErrorComponent::UpdateErrorDisplay(const std::vector<ErrorInfo> &errors)
{
    // Store current error state
    current_errors_ = errors;

    // Ensure current index is valid
    if (current_error_index_ >= current_errors_.size())
    {
        current_error_index_ = 0;
    }

    // Display the current error
    DisplayCurrentError();

    // Update container border color based on current error level
    if (!current_errors_.empty() && current_error_index_ < current_errors_.size())
    {
        ErrorLevel current_level = current_errors_[current_error_index_].level;
        lv_color_t border_color = style_utils::GetErrorColor(current_level);
        lv_obj_set_style_border_color(error_container_, border_color, 0);
    }
}

/**
 * @brief Updates error display with specific error index
 * @param errors Vector of ErrorInfo structures to display
 * @param currentIndex Index of error to display
 *
 * Sets specific error to display from the list, validates index bounds,
 * and updates border color based on selected error's severity level.
 */
void ErrorComponent::UpdateErrorDisplay(const std::vector<ErrorInfo> &errors, size_t current_index)
{
    // Store current error state
    current_errors_ = errors;

    // Set the current index
    if (current_index < current_errors_.size())
    {
        current_error_index_ = current_index;
    }
    else
    {
        current_error_index_ = 0;
    }

    // Display the current error
    DisplayCurrentError();

    // Update container border color based on current error level
    if (!current_errors_.empty() && current_error_index_ < current_errors_.size())
    {
        ErrorLevel current_level = current_errors_[current_error_index_].level;
        lv_color_t border_color = style_utils::GetErrorColor(current_level);
        lv_obj_set_style_border_color(error_container_, border_color, 0);
    }
}

// ========== Private Methods ==========

/**
 * @brief Creates UI structure for single error display
 * @param parent Parent LVGL object to create UI in
 *
 * Builds complete error display UI including position indicator, content area,
 * error level/source/message labels, and navigation hints. Uses large fonts
 * and maximizes available space for error message display.
 */
void ErrorComponent::CreateSingleErrorUI(lv_obj_t *parent)
{
    log_v("CreateSingleErrorUI() called");

    // Create error count indicator at top
    error_count_label_ = lv_label_create(parent);
    lv_obj_align(error_count_label_, LV_ALIGN_TOP_MID, 0, 8); // Near top with minimal margin
    lv_obj_set_style_text_font(error_count_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(error_count_label_, lv_color_white(), 0); // Always white for dark background
    lv_label_set_text(error_count_label_, ui_strings::error_ui::SINGLE_ERROR_COUNT);

    // Create main content area using most of the 220x220 available space
    error_content_area_ = lv_obj_create(parent);
    lv_obj_set_size(error_content_area_, 200, 180);            // Large content area
    lv_obj_align(error_content_area_, LV_ALIGN_CENTER, 0, 10); // Slightly down from center
    lv_obj_set_style_bg_opa(error_content_area_, LV_OPA_0, 0); // Transparent background
    lv_obj_set_style_border_width(error_content_area_, 0, 0);  // No border
    lv_obj_set_style_pad_all(error_content_area_, 5, 0);       // Minimal padding

    // Create large error level indicator
    error_level_label_ = lv_label_create(error_content_area_);
    lv_obj_align(error_level_label_, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(error_level_label_, &lv_font_montserrat_24, 0); // Large font
    lv_obj_set_style_text_color(error_level_label_, lv_color_white(), 0);
    lv_label_set_text(error_level_label_, ui_strings::error_ui::DEFAULT_ERROR_LEVEL);

    // Create error source label
    error_source_label_ = lv_label_create(error_content_area_);
    lv_obj_align(error_source_label_, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_text_font(error_source_label_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(error_source_label_, lv_color_white(), 0);
    lv_label_set_text(error_source_label_, ui_strings::error_ui::DEFAULT_ERROR_SOURCE);

    // Create error message display with maximum available space
    error_message_label_ = lv_label_create(error_content_area_);
    lv_obj_set_size(error_message_label_, 180, 100); // Large message area
    lv_obj_align(error_message_label_, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_text_font(error_message_label_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(error_message_label_, lv_color_white(), 0);
    lv_obj_set_style_text_align(error_message_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(error_message_label_, LV_LABEL_LONG_WRAP); // Multi-line text
    lv_label_set_text(error_message_label_, ui_strings::error_ui::LOADING_ERRORS_MESSAGE);

    // Create navigation indicator at bottom
    navigation_indicator_ = lv_label_create(parent);
    lv_obj_align(navigation_indicator_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_text_font(navigation_indicator_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(navigation_indicator_, lv_color_white(), 0);
    lv_label_set_text(navigation_indicator_, ui_strings::error_ui::LOADING_NAVIGATION);
}

/**
 * @brief Displays the current error from the error list
 *
 * Updates all UI elements with current error information including position
 * indicator, error level with color coding, source, message text, and
 * navigation hints. Returns early if no errors to prevent empty display.
 */
void ErrorComponent::DisplayCurrentError()
{
    log_v("DisplayCurrentError() called");
    if (current_errors_.empty())
    {
        // Don't update display when no errors - panel should be closing
        // This prevents the brief "0/0 errors" screen from showing
        return;
    }

    if (current_error_index_ >= current_errors_.size())
    {
        current_error_index_ = 0; // Reset if out of bounds
    }

    const ErrorInfo &current_error = current_errors_[current_error_index_];

    // Update error count indicator
    char position_text[16];
    snprintf(position_text, sizeof(position_text), "errors: %zu", current_errors_.size());
    lv_label_set_text(error_count_label_, position_text);

    // Update error level with color
    const char *level_text = GetErrorLevelText(current_error.level);
    lv_label_set_text(error_level_label_, level_text);
    lv_obj_set_style_text_color(error_level_label_, style_utils::GetErrorColor(current_error.level), 0);

    // Update source
    lv_label_set_text(error_source_label_, current_error.source);

    // Update message - can display full message now with wrapping
    lv_label_set_text(error_message_label_, current_error.message);

    // Update navigation indicator - same for all errors
    // Short press cycles through errors, long press exits from any error
    lv_label_set_text(navigation_indicator_, ui_strings::error_ui::NAVIGATION_INSTRUCTIONS);
}

// ========== Helper Methods ==========

/**
 * @brief Converts error level enum to display text
 * @param level ErrorLevel enum value
 * @return String representation of error level
 *
 * Returns abbreviated text for error levels: CRIT for CRITICAL,
 * ERR for ERROR, WARN for WARNING, UNKN for unknown levels.
 */
const char *ErrorComponent::GetErrorLevelText(ErrorLevel level)
{
    log_v("GetErrorLevelText() called");
    switch (level)
    {
        case ErrorLevel::CRITICAL:
            return ui_strings::error_ui::LEVEL_CRITICAL;
        case ErrorLevel::ERROR:
            return ui_strings::error_ui::LEVEL_ERROR;
        case ErrorLevel::WARNING:
            return ui_strings::error_ui::LEVEL_WARNING;
        default:
            return ui_strings::error_ui::LEVEL_UNKNOWN;
    }
}

// Note: LVGL event callbacks removed - these were never connected to UI elements
// Error handling is now done through ErrorPanel button logic