#include "components/oem/oem_oil_pressure_component.h"
#include "definitions/constants.h"
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

OemOilPressureComponent::OemOilPressureComponent(IStyleManager *style_manager) : OemOilComponent(style_manager)
{
    // Constructor delegates initialization to base OemOilComponent
}

// ========== Protected Methods ==========

/**
 * @brief Gets the icon for the oil pressure component.
 * @return Pointer to the icon image descriptor.
 */
const lv_image_dsc_t *OemOilPressureComponent::GetIcon() const
{
    return &oil_can_regular;
}

/**
 * @brief Gets the minimum scale value for the oil pressure component.
 * @return The minimum scale value.
 */
int32_t OemOilPressureComponent::GetScaleMin() const
{
    return sensor_constants::PRESSURE_DISPLAY_SCALE_MIN;
}

/**
 * @brief Gets the maximum scale value for the oil pressure component.
 * @return The maximum scale value.
 */
int32_t OemOilPressureComponent::GetScaleMax() const
{
    return sensor_constants::PRESSURE_DISPLAY_SCALE_MAX;
}

/**
 * @brief Gets the danger zone value for the oil pressure component.
 * @return The danger zone value.
 */
int32_t OemOilPressureComponent::GetDangerZone() const
{
    return 5;
}

/**
 * @brief Gets the scale mode for the oil pressure component.
 * @return The scale mode.
 */
lv_scale_mode_t OemOilPressureComponent::GetScaleMode() const
{
    return LV_SCALE_MODE_ROUND_INNER;
}

/**
 * @brief Gets the angle range for the oil pressure component.
 * @return The angle range.
 */
int32_t OemOilPressureComponent::GetAngleRange() const
{
    return 120;
}

/**
 * @brief Checks if the given value is in the danger zone.
 * @param value The value to check.
 */
/// @return True if the value is in the danger zone, false otherwise.
bool OemOilPressureComponent::IsDangerCondition(int32_t value) const
{
    return value <= OemOilPressureComponent::GetDangerZone();
}

/**
 * @brief Sets up the danger zone section on the scale.
 * @param section The scale section to configure.
 */
void OemOilPressureComponent::SetupDangerZone(lv_scale_section_t *section) const
{
    lv_scale_section_set_range(section, OemOilPressureComponent::GetScaleMin(),
                               OemOilPressureComponent::GetDangerZone());
}

/**
 * @brief Gets the Y offset for the oil pressure icon.
 * @return The Y offset value.
 */
int32_t OemOilPressureComponent::GetIconYOffset() const
{
    return -55;
}

/**
 * @brief Gets the label angles for L and H labels.
 * @param lAngle Reference to store the L label angle.
 */
/// @param hAngle Reference to store the H label angle.
void OemOilPressureComponent::GetLabelAngles(int32_t &l_angle, int32_t &h_angle) const
{
    // Standard positioning for pressure component
    // L label: At _scale_rotation angle (low pressure start)
    l_angle = scale_rotation_;

    // H label: At _scale_rotation + angle_range (high pressure end)
    h_angle = scale_rotation_ + GetAngleRange();
}