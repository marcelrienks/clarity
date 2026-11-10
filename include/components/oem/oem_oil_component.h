#pragma once

// System/Library Includes
#include <lvgl.h>

// Project Includes
#include "definitions/types.h"
#include "interfaces/i_component.h"
#include "interfaces/i_style_manager.h"

/**
 * @class OemOilComponent
 * @brief Abstract base class for oem styled oil monitoring gauge components
 *
 * @details This abstract class provides the common functionality for oem styled oil-related
 * gauge components (pressure, temperature). It implements the Template Method
 * design pattern, allowing derived classes to customize specific behaviors
 * while sharing common gauge rendering logic.
 *
 * @design_pattern Template Method - defines gauge creation algorithm
 * @view_role Renders circular gauges with needles, scales, and danger zones
 * @ui_elements Scale, needle, center icon, danger zone sections
 * @positioning Supports all ComponentLocation alignment options
 *
 * @gauge_specifications:
 * - Size: 240x240 pixels (configurable via ComponentLocation)
 * - Needle length: 90 pixels
 * - Scale ticks: 13 total, major every 2
 * - Danger zone: Red highlighting for critical values
 *
 * @derived_classes:
 * - OemOilPressureComponent: Oil pressure monitoring (0-100 PSI)
 * - OemOilTemperatureComponent: Oil temperature monitoring (mapped range)
 *
 * @virtual_methods Subclasses must implement:
 * - GetIcon(): Component-specific icon
 * - GetScaleMin/max(): Value range
 * - GetDangerZone(): Critical threshold
 * - SetupDangerZone(): Configure danger highlighting
 * - IsDangerCondition(): Determine if value is critical
 *
 * @context This is the base class for oem styled oil gauges. Currently used by
 * pressure and temperature components. The components are positioned on opposite sides
 * on the screen to maintain a consistent appearance with OEM styling.
 */
class OemOilComponent : public IComponent
{
  public:
    // ========== Constructors and Destructor ==========
    OemOilComponent(IStyleManager *style_manager);
    virtual ~OemOilComponent();

    // ========== Public Interface Methods ==========
    void Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display) override;
    void Refresh(const Reading &reading) override;
    void SetValue(int32_t value) override;

  protected:
    // ========== Protected Methods ==========
    virtual const lv_image_dsc_t *GetIcon() const = 0;
    virtual int32_t GetScaleMin() const = 0;
    virtual int32_t GetScaleMax() const = 0;
    virtual int32_t GetDangerZone() const = 0;
    virtual lv_scale_mode_t GetScaleMode() const = 0;
    virtual int32_t GetAngleRange() const = 0;
    virtual bool IsDangerCondition(int32_t value) const = 0;
    virtual int32_t MapValueForDisplay(int32_t value) const;
    virtual void SetupDangerZone(lv_scale_section_t *section) const = 0;
    virtual int32_t GetIconYOffset() const = 0;
    virtual void GetLabelAngles(int32_t &l_angle, int32_t &h_angle) const = 0;

    // ========== Protected Data Members ==========
    IStyleManager *style_manager_;

    // LVGL objects
    lv_obj_t *scale_;
    lv_obj_t *needle_line_;             // Tip section - thinnest
    lv_obj_t *needle_middle_;           // Middle section - medium thickness
    lv_obj_t *needle_base_;             // Base section - thickest for smooth tapered appearance
    lv_obj_t *needle_highlight_line_;   // Highlight line for 3D effect - tip
    lv_obj_t *needle_highlight_middle_; // Highlight line for 3D effect - middle
    lv_obj_t *needle_highlight_base_;   // Highlight line for 3D effect - base
    lv_obj_t *oil_icon_;
    lv_obj_t *low_label_;       // "L" label for low end
    lv_obj_t *high_label_;      // "H" label for high end
    lv_obj_t *pivot_circle_;    // Main pivot circle
    lv_obj_t *pivot_highlight_; // Highlight on pivot for 3D effect

    // Common constants
    static constexpr int32_t NEEDLE_LENGTH = 90;

    // Scale rotation tracking for label positioning
    int32_t scale_rotation_;

  private:
    // ========== Private Methods ==========
    void CreateIcon();
    void CreateLabels();
    void CreateNeedle();
    void CreateScale(int32_t rotation);
    void UpdatePivotStyling();
};