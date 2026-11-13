#pragma once

// System/Library Includes
#include <lvgl.h>

// Project Includes
#include "definitions/types.h"
#include "interfaces/i_display_provider.h"

/**
 * @interface IComponent
 * @brief Base interface for all UI components in the Clarity system
 *
 * @details This interface defines the contract for UI components that render
 * visual elements on LVGL screens. Components are the View layer in the MVP
 * architecture, responsible for displaying data received from sensors/models.
 *
 * @design_pattern View in MVP - handles UI rendering and updates
 * @render_lifecycle:
 * 1. render(): Initial component creation and positioning
 * 2. refresh(): Update component with new sensor data
 * 3. setValue(): Direct value updates (optional override)
 *
 * @rendering_strategy:
 * - Components create LVGL objects during render()
 * - refresh() updates existing objects with new data
 * - All components support ComponentLocation positioning
 *
 * @thread_safety Components must be called from LVGL thread only
 * @memory_management Components do not own LVGL objects (screen manages lifecycle)
 *
 * @implementations:
 * - ClarityComponent: Branding/logo display
 * - KeyComponent: Key status indicator
 * - LockComponent: Lock status indicator
 * - OemOilComponent: Oil pressure gauge
 * - OemOilPressureComponent: Dedicated pressure display
 * - OemOilTemperatureComponent: Dedicated temperature display
 *
 * @context This is the base interface for all visual components.
 * Components are created by panels and render specific UI elements like
 * gauges, indicators, and status displays.
 */
class IComponent
{
  public:
    virtual ~IComponent() = default;

    // Core Interface Methods
    virtual void Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display) = 0;
    virtual void Refresh(const Reading &reading) {};
    virtual void SetValue(int32_t value) {};
};