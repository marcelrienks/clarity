#pragma once

#include "components/error_component.h"
#include "definitions/types.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "managers/error_manager.h"
#include "ui/theme.h"
/**
 * @class ErrorPanel
 * @brief Application error display and management panel
 *
 * @details This panel displays pending application errors in a scrollable list
 * format. It provides user interaction for acknowledging errors and shows
 * appropriate visual indicators based on error severity levels.
 *
 * @presenter_role Coordinates error display with ErrorManager data
 * @data_source ErrorManager providing error queue information
 * @update_strategy Real-time error list updates with auto-refresh
 *
 * @ui_layout:
 * - Header: Error count and severity indicator
 * - List: Scrollable list of individual error entries
 * - Footer: Clear all errors button (for non-critical errors)
 *
 * @visual_feedback:
 * - Critical: Red background/border for critical errors
 * - Error: Orange background/border for regular errors
 * - Warning: Yellow background/border for warnings
 * - Auto-dismiss indicators for warnings
 *
 * @context This panel provides comprehensive error management interface.
 * It automatically appears when errors occur and manages user acknowledgment
 * and dismissal workflows.
 */
class ErrorPanel : public IPanel
{
  public:
    // ========== Constructors and Destructor ==========
    ErrorPanel(IGpioProvider *gpio, IDisplayProvider *display, Theme *theme);
    ~ErrorPanel() override;

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::ERROR;
    void Init() override;
    void Load() override;
    void Update() override;

    /// Short press: the viewed error is removed and the next shown. Returns ClearErrors once the errors are
    /// cleared (a long press, or a short press on the last one), so the screen manager restores
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

  private:
    // ========== Private Methods ==========
    clarity::core::NavIntent HandleShortPress();
    clarity::core::NavIntent HandleLongPress();
    void SortErrorsBySeverity(); // Sort errors by severity (CRITICAL first, WARNING last)
    void AdvanceToNextError();   // Move to next error and update component

    // ========== Private Data Members ==========
    IGpioProvider *gpio_provider_;
    IDisplayProvider *display_provider_;
    Theme *theme_;
    lv_obj_t *screen_ = nullptr;
    ErrorComponent error_component_; // Error component - static allocation
    bool component_initialized_ = false;
    ComponentLocation center_location_;     // Component positioning
    std::vector<ErrorInfo> current_errors_; // Cache of current error state

    // Error cycling functionality
    size_t current_error_index_;           // Index of currently displayed error
    std::vector<ErrorInfo> viewed_errors_; // Track errors that have been viewed and removed
};