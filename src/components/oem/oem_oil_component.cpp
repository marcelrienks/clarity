#include "components/oem/oem_oil_component.h"
#include "definitions/constants.h"
#include "definitions/styles.h"
#include "managers/error_manager.h"
#include "managers/style_manager.h" // For MAIN_DEFAULT, ITEMS_DEFAULT, INDICATOR_DEFAULT constants
#include <Arduino.h>
#include <cstring>
#include <esp32-hal-log.h>
#include <math.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs an OEM oil component with style service dependency
 * @param styleManager Style service for theme and styling management
 *
 * Initializes all LVGL object pointers to nullptr and validates the required
 * style service dependency. Reports critical error if style service is missing.
 */
OemOilComponent::OemOilComponent(IStyleManager *style_manager)
    : style_manager_(style_manager), scale_(nullptr), needle_line_(nullptr), needle_middle_(nullptr),
      needle_base_(nullptr), needle_highlight_line_(nullptr), needle_highlight_middle_(nullptr),
      needle_highlight_base_(nullptr), oil_icon_(nullptr), low_label_(nullptr), high_label_(nullptr),
      pivot_circle_(nullptr), pivot_highlight_(nullptr), scale_rotation_(0)
{
    // Validate styleManager dependency
    if (!style_manager_)
    {
        log_e("StyleService is required but was null");
        ErrorManager::Instance().ReportCriticalError("OemOilComponent",
                                                     "Cannot create - StyleService dependency is null");
    }
}

/**
 * @brief Destructor; deletes no LVGL objects
 *
 * Every widget is a descendant of the panel's screen and is freed when the
 * panel deletes that screen. Deleting any of them here would double-delete.
 * Styles are managed by StyleManager and don't require cleanup here.
 */
OemOilComponent::~OemOilComponent() = default;

// ========== IComponent Implementation ==========

/**
 * @brief This method initializes the scale, needle, and icon for the oil component with location parameters.
 * @param screen The screen object to render the component on.
 * @param location The location parameters for positioning the component.
 */
void OemOilComponent::Render(lv_obj_t *screen, const ComponentLocation &location, IDisplayProvider *display)
{
    if (!screen || !display)
    {
        log_e("OemOilComponent requires screen and display provider");
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "OemOilComponent",
                                             "Cannot render - screen or display provider is null");
        return;
    }

    // Create the scale using display provider
    scale_ = display->CreateScale(screen);
    if (!scale_)
    {
        return;
    }

    lv_obj_set_size(scale_, 240, 240);

    // Apply location settings
    lv_obj_align(scale_, location.align, location.x_offset, location.y_offset);

    CreateScale(location.rotation);
    CreateIcon();
    CreateLabels();
    CreateNeedle();

    // Component rendering complete
}

/**
 * @brief Updates the rendered oil component.
 * @param reading The Reading value to update the component with.
 */
void OemOilComponent::Refresh(const Reading &reading)
{
    int32_t value = std::get<int32_t>(reading);
    const ThemeColors &colours = style_manager_->GetThemeColors();

    const std::string &current_theme = style_manager_->GetCurrentTheme();
    // Component refresh completed

    // Icon color logic - in night mode, always use gaugeNormal (red)
    // In day mode, use gaugeNormal normally, gaugeDanger when in danger
    bool is_night_theme = (current_theme == themes::NIGHT);
    lv_color_t icon_colour;
    if (is_night_theme)
    {
        // Night mode: icons are always red regardless of danger condition
        icon_colour = colours.gaugeNormal;
    }
    else
    {
        // Day mode: normal color switching based on danger condition
        icon_colour = IsDangerCondition(value) ? colours.gaugeDanger : colours.gaugeNormal;
    }

    // Update needle and icon colors (all three needle sections with 3D gradient)
    if (IsDangerCondition(value))
    {
        // Danger mode - bright danger color with gradient
        lv_obj_set_style_line_color(needle_line_, colours.needleDanger, MAIN_DEFAULT); // Bright danger tip
        lv_obj_set_style_line_color(needle_middle_, lv_color_darken(colours.needleDanger, 10),
                                    MAIN_DEFAULT); // Medium danger middle
        lv_obj_set_style_line_color(needle_base_, lv_color_darken(colours.needleDanger, 20),
                                    MAIN_DEFAULT); // Darker danger base

        // Highlight lines in danger mode - bright danger highlights
        lv_obj_set_style_line_color(needle_highlight_line_, lv_color_lighten(colours.needleDanger, 30), MAIN_DEFAULT);
        lv_obj_set_style_line_color(needle_highlight_middle_, lv_color_lighten(colours.needleDanger, 20), MAIN_DEFAULT);
        lv_obj_set_style_line_color(needle_highlight_base_, lv_color_lighten(colours.needleDanger, 10), MAIN_DEFAULT);
    }
    else
    {
        // Normal mode - bright white gradient for 3D effect
        lv_obj_set_style_line_color(needle_line_, colours.needleNormal, MAIN_DEFAULT); // Bright white tip
        lv_obj_set_style_line_color(needle_middle_, lv_color_darken(colours.needleNormal, 10),
                                    MAIN_DEFAULT); // Medium white middle
        lv_obj_set_style_line_color(needle_base_, lv_color_darken(colours.needleNormal, 20),
                                    MAIN_DEFAULT); // Darker white base

        // Highlight lines in normal mode - bright white highlights
        lv_obj_set_style_line_color(needle_highlight_line_, colours.needleNormal, MAIN_DEFAULT);
        lv_obj_set_style_line_color(needle_highlight_middle_, colours.needleNormal, MAIN_DEFAULT);
        lv_obj_set_style_line_color(needle_highlight_base_, colours.needleNormal, MAIN_DEFAULT);
    }
    lv_obj_set_style_image_recolor(oil_icon_, icon_colour, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(oil_icon_, LV_OPA_COVER, MAIN_DEFAULT);

    // Update pivot styling based on current theme
    UpdatePivotStyling();

    // Component update complete
}

/**
 * @brief Sets the value of the oil component and updates needle position
 * @param value Oil pressure or temperature value to display
 *
 * Clamps the input value to scale boundaries and maps it to needle rotation angle.
 * Updates the needle rotation to visually represent the new value on the gauge.
 */
void OemOilComponent::SetValue(int32_t value)
{

    // Clamp the input value to logical scale boundaries BEFORE mapping
    // This ensures values stay within the component's logical range
    const int32_t scale_min = GetScaleMin();
    const int32_t scale_max = GetScaleMax();

    int32_t clamped_value = value;
    if (clamped_value < scale_min)
    {
        clamped_value = scale_min;
    }
    else if (clamped_value > scale_max)
    {
        clamped_value = scale_max;
    }

    // Now allow derived classes to map values if needed (e.g., for reversed scales)
    // Temperature component will reverse the clamped value: 0->120, 120->0
    int32_t mapped_value = MapValueForDisplay(clamped_value);

    // Update all three needle sections for smooth tapered appearance
    lv_scale_set_line_needle_value(scale_, needle_line_, NEEDLE_LENGTH, mapped_value); // Full length (tip)

    lv_scale_set_line_needle_value(scale_, needle_middle_, (NEEDLE_LENGTH * 2) / 3,
                                   mapped_value); // 2/3 length (middle)

    lv_scale_set_line_needle_value(scale_, needle_base_, NEEDLE_LENGTH / 3, mapped_value); // 1/3 length (base)

    // Update highlight lines for 3D effect
    lv_scale_set_line_needle_value(scale_, needle_highlight_line_, NEEDLE_LENGTH - 2,
                                   mapped_value); // Slightly shorter for highlight effect

    lv_scale_set_line_needle_value(scale_, needle_highlight_middle_, ((NEEDLE_LENGTH * 2) / 3) - 2,
                                   mapped_value); // 2/3 length highlight

    lv_scale_set_line_needle_value(scale_, needle_highlight_base_, (NEEDLE_LENGTH / 3) - 2,
                                   mapped_value); // 1/3 length highlight
}

/**
 * @brief Maps the value for display on the oil component.
 * @param value The original value to map.
 * @return The mapped value for display.
 */
int32_t OemOilComponent::MapValueForDisplay(int32_t value) const
{
    // Default implementation - no mapping
    // Derived classes can override for special mapping (e.g., temperature component)
    return value;
}

/**
 * @brief Updates pivot styling based on current theme
 */
void OemOilComponent::UpdatePivotStyling()
{
    if (pivot_circle_)
    {
        const ThemeColors &colours = style_manager_->GetThemeColors();

        if (style_manager_->GetCurrentTheme() == themes::NIGHT)
        {
            // Night mode - make pivot completely invisible by matching background exactly
            lv_obj_set_style_bg_color(pivot_circle_, colours.background, MAIN_DEFAULT);
            lv_obj_set_style_bg_grad_color(pivot_circle_, colours.background, MAIN_DEFAULT);
            lv_obj_set_style_bg_grad_dir(pivot_circle_, LV_GRAD_DIR_NONE, MAIN_DEFAULT); // Disable gradient completely
            lv_obj_set_style_bg_opa(pivot_circle_, LV_OPA_COVER, MAIN_DEFAULT);          // Full opacity
            lv_obj_set_style_border_width(pivot_circle_, 0, MAIN_DEFAULT);               // No border
            lv_obj_set_style_border_opa(pivot_circle_, LV_OPA_TRANSP, MAIN_DEFAULT);     // No border opacity
            lv_obj_set_style_shadow_opa(pivot_circle_, LV_OPA_TRANSP, MAIN_DEFAULT);     // No shadow
            lv_obj_set_style_outline_width(pivot_circle_, 0, MAIN_DEFAULT);              // No outline

            // Hide highlight in night mode
            if (pivot_highlight_)
            {
                lv_obj_add_flag(pivot_highlight_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            // Day mode - visible plastic appearance with radial gradient
            lv_obj_set_style_bg_color(pivot_circle_, lv_color_hex(colors::PIVOT_CIRCLE_CENTER),
                                      MAIN_DEFAULT); // Medium gray center
            lv_obj_set_style_bg_grad_color(pivot_circle_, lv_color_hex(colors::PIVOT_CIRCLE_EDGE),
                                           MAIN_DEFAULT); // Dark gray edge
            lv_obj_set_style_bg_grad_dir(pivot_circle_, LV_GRAD_DIR_HOR,
                                         MAIN_DEFAULT);                         // Horizontal for radial-like effect
            lv_obj_set_style_bg_grad_stop(pivot_circle_, 180, MAIN_DEFAULT);    // Gradient more toward edge
            lv_obj_set_style_bg_opa(pivot_circle_, LV_OPA_COVER, MAIN_DEFAULT); // Full opacity

            // Dark beveled border (darker than main body)
            lv_obj_set_style_border_width(pivot_circle_, 2, MAIN_DEFAULT);
            lv_obj_set_style_border_color(pivot_circle_, lv_color_hex(colors::PIVOT_CIRCLE_BORDER),
                                          MAIN_DEFAULT);                            // Very dark border
            lv_obj_set_style_border_opa(pivot_circle_, LV_OPA_COVER, MAIN_DEFAULT); // Solid border

            // Subtle shadow for depth
            lv_obj_set_style_shadow_color(pivot_circle_, lv_color_hex(colors::PIVOT_CIRCLE_SHADOW), MAIN_DEFAULT);
            lv_obj_set_style_shadow_width(pivot_circle_, 3U, MAIN_DEFAULT);      // Moderate shadow
            lv_obj_set_style_shadow_opa(pivot_circle_, LV_OPA_20, MAIN_DEFAULT); // Subtle
            lv_obj_set_style_shadow_spread(pivot_circle_, 1, MAIN_DEFAULT);
            lv_obj_set_style_shadow_offset_x(pivot_circle_, 1, MAIN_DEFAULT);
            lv_obj_set_style_shadow_offset_y(pivot_circle_, 1, MAIN_DEFAULT);
            lv_obj_set_style_outline_width(pivot_circle_, 0, MAIN_DEFAULT); // No outline

            // Show highlight in day mode
            if (pivot_highlight_)
            {
                lv_obj_clear_flag(pivot_highlight_, LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Force object invalidation to ensure immediate visual update
        lv_obj_invalidate(pivot_circle_);
    }
}

// Private Methods

/**
 * @brief Creates the oil icon for the oil component.
 */
void OemOilComponent::CreateIcon()
{
    const ThemeColors &colours = style_manager_->GetThemeColors();

    oil_icon_ = lv_image_create(scale_);
    lv_image_set_src(oil_icon_, GetIcon());
    lv_image_set_scale(oil_icon_, 50);
    lv_obj_align(oil_icon_, LV_ALIGN_CENTER, 0, GetIconYOffset());
    lv_obj_set_style_opa(oil_icon_, LV_OPA_COVER, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor(oil_icon_, colours.gaugeNormal, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(oil_icon_, LV_OPA_COVER, MAIN_DEFAULT);
}

/**
 * @brief Creates L and H labels positioned relative to the scale rotation and angle range.
 * Labels automatically follow when scale rotation changes.
 */
void OemOilComponent::CreateLabels()
{
    // Create "L" label for low end
    low_label_ = lv_label_create(scale_);
    lv_label_set_text(low_label_, ui_constants::GAUGE_LOW_LABEL);
    if (style_manager_ && style_manager_->IsInitialized())
    {
        lv_obj_add_style(low_label_, &style_manager_->GetTextStyle(), MAIN_DEFAULT);
    }
    lv_obj_set_style_text_font(low_label_, &lv_font_montserrat_18, MAIN_DEFAULT);

    // Create "H" label for high end
    high_label_ = lv_label_create(scale_);
    lv_label_set_text(high_label_, ui_constants::GAUGE_HIGH_LABEL);
    if (style_manager_ && style_manager_->IsInitialized())
    {
        lv_obj_add_style(high_label_, &style_manager_->GetTextStyle(), MAIN_DEFAULT);
    }
    lv_obj_set_style_text_font(high_label_, &lv_font_montserrat_18, MAIN_DEFAULT);

    // Calculate label positions based on scale rotation and angle range
    // Allow derived classes to customize label positioning (e.g., for reversed scales)
    int32_t l_angle, h_angle;
    GetLabelAngles(l_angle, h_angle);

    // Use same radius for both labels - they should be equidistant from pivot
    int32_t radius = 78;

    // L label position
    double l_angle_rad = (l_angle * M_PI) / 180.0;
    int32_t l_x = (int32_t)(radius * cos(l_angle_rad));
    int32_t l_y = (int32_t)(radius * sin(l_angle_rad));
    lv_obj_align(low_label_, LV_ALIGN_CENTER, l_x, l_y);

    // H label position
    double h_angle_rad = (h_angle * M_PI) / 180.0;
    int32_t h_x = (int32_t)(radius * cos(h_angle_rad));
    int32_t h_y = (int32_t)(radius * sin(h_angle_rad));
    lv_obj_align(high_label_, LV_ALIGN_CENTER, h_x, h_y);
}

/**
 * @brief Creates the needle line for the oil component.
 */
void OemOilComponent::CreateNeedle()
{
    const ThemeColors &colours = style_manager_->GetThemeColors();

    // Create realistic 3-section tapered needle (based on actual car dashboard reference)

    // Section 1: Tip section with enhanced 3D effect - thinnest (outer third)
    needle_line_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needle_line_, colours.needleNormal, MAIN_DEFAULT); // Bright white from theme
    lv_obj_set_style_line_width(needle_line_, 4, MAIN_DEFAULT);                    // Slightly thicker tip
    lv_obj_set_style_line_rounded(needle_line_, true, MAIN_DEFAULT);               // Rounded ends
    lv_obj_set_style_line_opa(needle_line_, LV_OPA_COVER, MAIN_DEFAULT);

    // Section 2: Middle section with enhanced 3D effect - medium thickness (middle third)
    needle_middle_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needle_middle_, lv_color_darken(colours.needleNormal, 10),
                                MAIN_DEFAULT);                    // Slightly darker for gradient
    lv_obj_set_style_line_width(needle_middle_, 5, MAIN_DEFAULT); // Thicker medium section
    lv_obj_set_style_line_rounded(needle_middle_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needle_middle_, LV_OPA_COVER, MAIN_DEFAULT);

    // Section 3: Base section with enhanced 3D effect - thickest (inner third near pivot)
    needle_base_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needle_base_, lv_color_darken(colours.needleNormal, 20),
                                MAIN_DEFAULT);                  // Darkest for gradient
    lv_obj_set_style_line_width(needle_base_, 7, MAIN_DEFAULT); // Thickest base section
    lv_obj_set_style_line_rounded(needle_base_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needle_base_, LV_OPA_COVER, MAIN_DEFAULT);

    // Add subtle highlight lines for enhanced 3D effect (very subtle to avoid artifacts)

    // Highlight for tip section - very subtle white highlight
    needle_highlight_line_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needle_highlight_line_, lv_color_hex(colors::NEEDLE_HIGHLIGHT),
                                MAIN_DEFAULT);                            // Pure white highlight
    lv_obj_set_style_line_width(needle_highlight_line_, 1, MAIN_DEFAULT); // Thin highlight line
    lv_obj_set_style_line_rounded(needle_highlight_line_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needle_highlight_line_, LV_OPA_20, MAIN_DEFAULT); // Very subtle

    // Highlight for middle section
    needle_highlight_middle_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needle_highlight_middle_, lv_color_hex(colors::NEEDLE_HIGHLIGHT),
                                MAIN_DEFAULT);                              // Pure white highlight
    lv_obj_set_style_line_width(needle_highlight_middle_, 1, MAIN_DEFAULT); // Thin highlight line
    lv_obj_set_style_line_rounded(needle_highlight_middle_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needle_highlight_middle_, LV_OPA_20, MAIN_DEFAULT); // Very subtle

    // Highlight for base section
    needle_highlight_base_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needle_highlight_base_, lv_color_hex(colors::NEEDLE_HIGHLIGHT),
                                MAIN_DEFAULT);                            // Pure white highlight
    lv_obj_set_style_line_width(needle_highlight_base_, 1, MAIN_DEFAULT); // Thin highlight line
    lv_obj_set_style_line_rounded(needle_highlight_base_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needle_highlight_base_, LV_OPA_20, MAIN_DEFAULT); // Very subtle

    // Realistic dark plastic pivot point (based on actual car dashboard reference)
    pivot_circle_ = lv_obj_create(scale_);
    lv_obj_set_size(pivot_circle_, 40U, 40U); // Even larger for better visibility and proportion
    lv_obj_center(pivot_circle_);
    lv_obj_set_style_radius(pivot_circle_, LV_RADIUS_CIRCLE, MAIN_DEFAULT);

    // Center light pickup highlight (where light hits the plastic)
    pivot_highlight_ = lv_obj_create(pivot_circle_);
    lv_obj_set_size(pivot_highlight_, 16U, 16U); // Proportional to larger pivot
    lv_obj_center(pivot_highlight_);
    lv_obj_set_style_radius(pivot_highlight_, LV_RADIUS_CIRCLE, MAIN_DEFAULT);
    lv_obj_set_style_bg_color(pivot_highlight_, lv_color_hex(colors::PIVOT_HIGHLIGHT),
                              MAIN_DEFAULT);                            // Light gray highlight
    lv_obj_set_style_bg_opa(pivot_highlight_, LV_OPA_80, MAIN_DEFAULT); // More opaque for plastic look
    lv_obj_set_style_border_width(pivot_highlight_, 0, MAIN_DEFAULT);

    // Style pivot based on theme
    UpdatePivotStyling();
}

/**
 * @brief Sets up the scale properties for the oil component.
 */
void OemOilComponent::CreateScale(int32_t rotation)
{
    if (!scale_)
    {
        return;
    }

    // Store rotation for label positioning
    scale_rotation_ = rotation;

    // Set scale properties from derived class
    lv_scale_set_mode(scale_, GetScaleMode());
    lv_scale_set_rotation(scale_, rotation);
    lv_scale_set_angle_range(scale_, GetAngleRange());
    lv_scale_set_range(scale_, GetScaleMin(), GetScaleMax());

    // Set tick properties to match fuel gauge pattern
    lv_scale_set_total_tick_count(scale_, 13);
    lv_scale_set_major_tick_every(scale_, 3);
    lv_scale_set_label_show(scale_, false); // Disable built-in labels, use custom L/H positioning

    if (style_manager_ && style_manager_->IsInitialized())
    {
        lv_obj_add_style(scale_, &style_manager_->GetGaugeMainStyle(), MAIN_DEFAULT);
        lv_obj_add_style(scale_, &style_manager_->GetGaugeIndicatorStyle(), INDICATOR_DEFAULT);
        lv_obj_add_style(scale_, &style_manager_->GetGaugeItemsStyle(), ITEMS_DEFAULT);
    }
    else
    {
        log_w("StyleService not available for gauge style application");
    }

    lv_scale_section_t *section = lv_scale_add_section(scale_);
    if (!section)
    {
        return;
    }

    if (style_manager_ && style_manager_->IsInitialized())
    {
        lv_scale_section_set_style(section, MAIN_DEFAULT, &style_manager_->GetGaugeMainStyle());
        lv_scale_section_set_style(section, INDICATOR_DEFAULT, &style_manager_->GetGaugeDangerSectionStyle());
        lv_scale_section_set_style(section, ITEMS_DEFAULT, &style_manager_->GetGaugeDangerSectionStyle());
    }
    else
    {
        log_w("StyleService not available for danger zone style application");
    }

    // Set danger zone range - derived classes will handle specific ranges
    SetupDangerZone(section);
}