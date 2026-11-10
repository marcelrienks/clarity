#pragma once

#include "components/oem/oem_oil_component.h"
#include "icons/oil_can_regular.h"

/**
 * @class OemOilPressureComponent
 * @brief Oem styled Oil pressure gauge component with danger zone monitoring
 *
 * @details This component specializes the OemOilComponent for oil pressure
 * monitoring. It displays a half circular gauge with pressure readings and
 * highlights dangerous low-pressure conditions. Due to it being Oem styled
 * it should take up the whole of the half of the screen to match the existing oem styled gauges.
 *
 * @specialization Oil Pressure Monitoring
 * @measurement_unit Bar (100 Kpa)
 * @range 0-6.0 Bar (displayed range)
 * @scale 0-60 internal scale with one decimal place precision
 *        Example: value 0 = 0.0 Bar, value 25 = 2.5 Bar, value 55 = 5.5 Bar, value 60 = 6.0 Bar
 * @danger_zone Below 2.0 Bar (value 20) triggers low pressure warning
 * @icon Oil can icon (oil_can_regular.h)
 *
 * @position Intention: Only one half of the screen
 * @size 240x240 pixels: to ensure the arc of the scale occupies the total half of the display, while the other half has
 * no fill and can be overlayed with other components.
 * @data_source OilPressureSensor with delta-based updates
 *
 * @gauge_configuration:
 * - Scale: 0-60 internal units representing 0.0-6.0 Bar (divide by 10 for display)
 * - Rotation: Standard circular gauge
 * - Angle range: Full circle coverage
 * - Hysteresis: Configurable percentile-based deadband (1%, 3%, or 5% of scale range)
 *   • 1% = 0.6 scale units = 0.06 Bar threshold
 *   • 3% = 1.8 scale units = 0.18 Bar threshold (default)
 *   • 5% = 3.0 scale units = 0.30 Bar threshold
 * - Danger zone: 0-2 Bar highlighted in red
 *
 * @visual_feedback:
 * - Normal: white needle and icon
 * - Danger: Red needle and icon for low pressure
 * - Smooth needle animations between values
 *
 * @context While the scale only occupies one half of the screen, it's size is equal to that of the screen to ensure
 * it fills the whole half of the rounded screen to maintain a consistent appearance with other OEM components.
 */
class OemOilPressureComponent : public OemOilComponent
{
  public:
    // ========== Constructors and Destructor ==========
    OemOilPressureComponent(IStyleManager *style_manager);
    ~OemOilPressureComponent() override = default;

  protected:
    // ========== Protected Methods ==========
    const lv_image_dsc_t *GetIcon() const override;
    int32_t GetScaleMin() const override;
    int32_t GetScaleMax() const override;
    int32_t GetDangerZone() const override;
    lv_scale_mode_t GetScaleMode() const override;
    int32_t GetAngleRange() const override;
    bool IsDangerCondition(int32_t value) const override;
    void SetupDangerZone(lv_scale_section_t *section) const override;
    int32_t GetIconYOffset() const override;
    void GetLabelAngles(int32_t &l_angle, int32_t &h_angle) const override;
};