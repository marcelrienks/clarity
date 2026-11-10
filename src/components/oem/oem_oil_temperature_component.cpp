#include "components/oem/oem_oil_temperature_component.h"
#include "definitions/constants.h"
#include "managers/error_manager.h"
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

OemOilTemperatureComponent::OemOilTemperatureComponent(IStyleManager *style_manager) : OemOilComponent(style_manager)
{
    log_v("OemOilTemperatureComponent() constructor called");
    // Constructor delegates initialization to base OemOilComponent
}

// ========== Protected Methods ==========

/**
 * @brief Gets the icon for the oil temperature component.
 * @return Pointer to the icon image descriptor.
 */
const lv_image_dsc_t *OemOilTemperatureComponent::GetIcon() const
{
    log_v("get_icon() called");
    return &oil_temp_regular;
}

/**
 * @brief Gets the minimum scale value for the oil temperature component.
 * @return The minimum scale value.
 */
int32_t OemOilTemperatureComponent::GetScaleMin() const
{
    return sensor_constants::TEMPERATURE_DISPLAY_SCALE_MIN;
}

/**
 * @brief Gets the maximum scale value for the oil temperature component.
 * @return The maximum scale value.
 */
int32_t OemOilTemperatureComponent::GetScaleMax() const
{
    return sensor_constants::TEMPERATURE_DISPLAY_SCALE_MAX;
}

/**
 * @brief Gets the danger zone value for the oil temperature component.
 * @return The danger zone value.
 */
int32_t OemOilTemperatureComponent::GetDangerZone() const
{
    return 100;
}

/**
 * @brief Gets the scale mode for the oil temperature component.
 * @return The scale mode.
 */
lv_scale_mode_t OemOilTemperatureComponent::GetScaleMode() const
{
    log_v("get_scale_mode() called");
    return LV_SCALE_MODE_ROUND_INNER;
}

/**
 * @brief Gets the angle range for the oil temperature component.
 * @return The angle range.
 */
int32_t OemOilTemperatureComponent::GetAngleRange() const
{
    log_v("get_angle_range() called");
    return 120;
}

/**
 * @brief Checks if the given value is in the danger zone.
 * @param value The value to check.
 */
/// @return True if the value is in the danger zone, false otherwise.
bool OemOilTemperatureComponent::IsDangerCondition(int32_t value) const
{
    return value >= OemOilTemperatureComponent::GetDangerZone();
}

/**
 * @brief Maps the value for display on the oil temperature component.
 * @param value The value to map.
 */
/// @return The mapped value.
int32_t OemOilTemperatureComponent::MapValueForDisplay(int32_t value) const
{
    // Maps the value from the original scale (0-120) to the reversed display scale (120-0).
    // Required for LVGL animation compatibility with reversed scales.

    // Map from [0,120] to [120,0] reverse the scale
    int32_t max_val = GetScaleMax();
    log_v("map_value_for_display: max_val=%d", max_val);

    int32_t mapped_value = max_val - value;
    log_v("map_value_for_display: returning mapped value=%d", mapped_value);

    return mapped_value;
}

/**
 * @brief Sets up the danger zone section on the scale.
 * @param section The scale section to configure.
 */
void OemOilTemperatureComponent::SetupDangerZone(lv_scale_section_t *section) const
{
    log_v("setup_danger_zone() called");

    if (!section)
    {
        log_e("setup_danger_zone: section is null");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "OemOilTemperatureComponent",
                                             "Cannot setup danger zone - section is null");
        return;
    }

    // Map danger zone to reversed scale for display
    int32_t max_val = GetScaleMax();
    int32_t danger_val = GetDangerZone();
    int32_t mapped_max = MapValueForDisplay(max_val);
    int32_t mapped_danger = MapValueForDisplay(danger_val);

    // Danger zone mapping completed

    lv_scale_section_set_range(section, mapped_max, mapped_danger);
}

/**
 * @brief Gets the Y offset for the oil temperature icon.
 * @return The Y offset value.
 */
int32_t OemOilTemperatureComponent::GetIconYOffset() const
{
    log_v("get_icon_y_offset() called");
    return 55;
}

/**
 * @brief Gets the label angles for L and H labels with swapped positioning.
 * @param lAngle Reference to store the L label angle.
 */
/// @param hAngle Reference to store the H label angle.
void OemOilTemperatureComponent::GetLabelAngles(int32_t &l_angle, int32_t &h_angle) const
{
    log_v("get_label_angles() called");
    // Swap L and H positioning due to reversed scale mapping
    // H label: At _scale_rotation angle (where L would normally be)
    h_angle = scale_rotation_;

    // L label: At _scale_rotation + angle_range (where H would normally be)
    l_angle = scale_rotation_ + GetAngleRange();
}