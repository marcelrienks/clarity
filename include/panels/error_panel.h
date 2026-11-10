#pragma once

#include "components/error_component.h"
#include "definitions/types.h"
#include "interfaces/i_action_handler.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_style_manager.h"
#include "managers/error_manager.h"
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
    ErrorPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
               IPanelManager *panel_manager);
    ~ErrorPanel();

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::ERROR;
    void Init() override;
    void Load() override;
    void Update() override;
    // IActionService Interface Implementation (inherited through IPanel)
    // Old function pointer methods removed - using direct HandleShortPress/HandleLongPress

    // Public action handlers
    void HandleShortPress();
    void HandleLongPress();

  private:
    // ========== Static Methods ==========
    static void ShowPanelCompletionCallback(lv_event_t *event);

    // ========== Private Methods ==========
    void SortErrorsBySeverity(); // Sort errors by severity (CRITICAL first, WARNING last)
    void AdvanceToNextError();   // Move to next error and update component

    // ========== Private Data Members ==========
    IGpioProvider *gpio_provider_;
    IDisplayProvider *display_provider_;
    IStyleManager *style_manager_;
    IPanelManager *panel_manager_;
    lv_obj_t *screen_ = nullptr;
    ErrorComponent error_component_; // Error component - static allocation
    bool component_initialized_ = false;
    ComponentLocation center_location_;     // Component positioning
    bool panel_loaded_;                     // Track panel load state
    std::vector<ErrorInfo> current_errors_; // Cache of current error state
    std::string previous_theme_;            // Store previous theme to restore on exit

    // Error cycling functionality
    size_t current_error_index_;           // Index of currently displayed error
    std::vector<ErrorInfo> viewed_errors_; // Track errors that have been viewed and removed
};