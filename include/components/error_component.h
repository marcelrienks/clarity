#pragma once

#include "definitions/styles.h"
#include "definitions/types.h"
#include "interfaces/i_component.h"
#include "interfaces/i_style_manager.h"
#include "managers/error_manager.h"

#include <lvgl.h>
#include <vector>

/**
 * @class ErrorComponent
 * @brief Single error display component optimized for full screen round display
 *
 * @details This component displays one error at a time using maximum screen real estate
 * on 240x240 round displays. Users can cycle through multiple errors using GPIO button input.
 * Each error is displayed with large, readable text and clear severity indicators.
 *
 * @view_role Renders single full-screen error with large typography and clear layout
 * @ui_elements Large error level, source, message labels with position indicator and navigation hint
 * @positioning Full 240x240 screen with 2px colored border and 220x220 text area
 *
 * @data_source Works with ErrorManager for error queue updates and maintains current position
 * @visual_states Color-coded border and error level (white/yellow/red) based on current error
 * @interaction_support GPIO button cycling managed by ErrorPanel via AdvanceToNextError method
 *
 * @round_display_optimizations:
 * - Full screen utilization with minimal margins
 * - Large fonts for maximum readability
 * - Multi-line message wrapping for full text display
 * - Circular 2px colored border indicating current error severity
 * - Clear navigation indicators showing current position (X/Y)
 *
 * @context This component displays system errors one at a time for focused attention.
 * It's designed to be used by ErrorPanel with GPIO button navigation integration.
 */
class ErrorComponent : public IComponent
{
  public:
    // ========== Constructors and Destructor ==========
    explicit ErrorComponent(IStyleManager *style_manager);
    ~ErrorComponent() override;

    // ========== Public Interface Methods ==========
    void Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display) override;
    void Refresh(const Reading &reading) override;

    // ========== Error-specific Methods ==========
    void UpdateErrorDisplay();
    void UpdateErrorDisplay(const std::vector<ErrorInfo> &errors);
    void UpdateErrorDisplay(const std::vector<ErrorInfo> &errors, size_t current_index);
    // Note: GPIO button cycling is now handled by ErrorPanel via AdvanceToNextError()

  protected:
    // Note: LVGL event handlers removed - never connected to UI elements

  private:
    // ========== Private Methods ==========
    void CreateSingleErrorUI(lv_obj_t *parent);
    void DisplayCurrentError();
    const char *GetErrorLevelText(ErrorLevel level);

    // ========== Private Data Members ==========
    IStyleManager *style_manager_;
    lv_obj_t *error_container_;      // Main container for error display
    lv_obj_t *error_content_area_;   // Single error content display area
    lv_obj_t *error_count_label_;    // Header showing current error position
    lv_obj_t *error_level_label_;    // Large error level indicator
    lv_obj_t *error_source_label_;   // Error source system label
    lv_obj_t *error_message_label_;  // Full error message display
    lv_obj_t *navigation_indicator_; // Shows current position in error queue

    std::vector<ErrorInfo> current_errors_; // Cache of current error state
    size_t current_error_index_;            // Index of currently displayed error

    // Note: clearButton_ removed but ClearAllErrorsCallback logic preserved
    // for future implementation via alternative interaction method
};