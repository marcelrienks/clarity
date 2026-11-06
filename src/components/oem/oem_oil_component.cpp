#include "components/oem/oem_oil_component.h"
#include "managers/error_manager.h"
#include "managers/style_manager.h" // For MAIN_DEFAULT, ITEMS_DEFAULT, INDICATOR_DEFAULT constants
#include "definitions/constants.h"
#include "definitions/styles.h"
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
OemOilComponent::OemOilComponent(IStyleManager *styleManager)
    : styleManager_(styleManager), scale_(nullptr), needleLine_(nullptr), needleMiddle_(nullptr), needleBase_(nullptr),
      needleHighlightLine_(nullptr), needleHighlightMiddle_(nullptr), needleHighlightBase_(nullptr), oilIcon_(nullptr),
      lowLabel_(nullptr), highLabel_(nullptr), pivotCircle_(nullptr), pivotHighlight_(nullptr), scaleRotation_(0)
{
    // Validate styleManager dependency
    if (!styleManager_)
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

    create_scale(location.rotation);
    create_icon();
    create_labels();
    create_needle();

    // Component rendering complete
}

/**
 * @brief Updates the rendered oil component.
 * @param reading The Reading value to update the component with.
 */
void OemOilComponent::Refresh(const Reading &reading)
{
    int32_t value = std::get<int32_t>(reading);
    const ThemeColors &colours = styleManager_->GetThemeColors();

    const std::string& currentTheme = styleManager_->GetCurrentTheme();
    // Component refresh completed

    // Icon color logic - in night mode, always use gaugeNormal (red)
    // In day mode, use gaugeNormal normally, gaugeDanger when in danger
    bool isNightTheme = (currentTheme == Themes::NIGHT);
    lv_color_t iconColour;
    if (isNightTheme)
    {
        // Night mode: icons are always red regardless of danger condition
        iconColour = colours.gaugeNormal;
    }
    else
    {
        // Day mode: normal color switching based on danger condition
        iconColour = is_danger_condition(value) ? colours.gaugeDanger : colours.gaugeNormal;
    }

    // Update needle and icon colors (all three needle sections with 3D gradient)
    if (is_danger_condition(value))
    {
        // Danger mode - bright danger color with gradient
        lv_obj_set_style_line_color(needleLine_, colours.needleDanger, MAIN_DEFAULT); // Bright danger tip
        lv_obj_set_style_line_color(needleMiddle_, lv_color_darken(colours.needleDanger, 10),
                                    MAIN_DEFAULT); // Medium danger middle
        lv_obj_set_style_line_color(needleBase_, lv_color_darken(colours.needleDanger, 20),
                                    MAIN_DEFAULT); // Darker danger base

        // Highlight lines in danger mode - bright danger highlights
        lv_obj_set_style_line_color(needleHighlightLine_, lv_color_lighten(colours.needleDanger, 30), MAIN_DEFAULT);
        lv_obj_set_style_line_color(needleHighlightMiddle_, lv_color_lighten(colours.needleDanger, 20), MAIN_DEFAULT);
        lv_obj_set_style_line_color(needleHighlightBase_, lv_color_lighten(colours.needleDanger, 10), MAIN_DEFAULT);
    }
    else
    {
        // Normal mode - bright white gradient for 3D effect
        lv_obj_set_style_line_color(needleLine_, colours.needleNormal, MAIN_DEFAULT); // Bright white tip
        lv_obj_set_style_line_color(needleMiddle_, lv_color_darken(colours.needleNormal, 10),
                                    MAIN_DEFAULT); // Medium white middle
        lv_obj_set_style_line_color(needleBase_, lv_color_darken(colours.needleNormal, 20),
                                    MAIN_DEFAULT); // Darker white base

        // Highlight lines in normal mode - bright white highlights
        lv_obj_set_style_line_color(needleHighlightLine_, colours.needleNormal, MAIN_DEFAULT);
        lv_obj_set_style_line_color(needleHighlightMiddle_, colours.needleNormal, MAIN_DEFAULT);
        lv_obj_set_style_line_color(needleHighlightBase_, colours.needleNormal, MAIN_DEFAULT);
    }
    lv_obj_set_style_image_recolor(oilIcon_, iconColour, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(oilIcon_, LV_OPA_COVER, MAIN_DEFAULT);

    // Update pivot styling based on current theme
    update_pivot_styling();

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
    const int32_t scaleMin = get_scale_min();
    const int32_t scaleMax = get_scale_max();
    
    int32_t clampedValue = value;
    if (clampedValue < scaleMin)
    {
        clampedValue = scaleMin;
    }
    else if (clampedValue > scaleMax)
    {
        clampedValue = scaleMax;
    }

    // Now allow derived classes to map values if needed (e.g., for reversed scales)
    // Temperature component will reverse the clamped value: 0->120, 120->0
    int32_t mappedValue = map_value_for_display(clampedValue);
    

    // Update all three needle sections for smooth tapered appearance
    lv_scale_set_line_needle_value(scale_, needleLine_, NEEDLE_LENGTH, mappedValue);             // Full length (tip)
    
    lv_scale_set_line_needle_value(scale_, needleMiddle_, (NEEDLE_LENGTH * 2) / 3, mappedValue); // 2/3 length (middle)
    
    lv_scale_set_line_needle_value(scale_, needleBase_, NEEDLE_LENGTH / 3, mappedValue);         // 1/3 length (base)

    // Update highlight lines for 3D effect
    lv_scale_set_line_needle_value(scale_, needleHighlightLine_, NEEDLE_LENGTH - 2,
                                   mappedValue); // Slightly shorter for highlight effect
    
    lv_scale_set_line_needle_value(scale_, needleHighlightMiddle_, ((NEEDLE_LENGTH * 2) / 3) - 2,
                                   mappedValue); // 2/3 length highlight
    
    lv_scale_set_line_needle_value(scale_, needleHighlightBase_, (NEEDLE_LENGTH / 3) - 2,
                                   mappedValue); // 1/3 length highlight
    
}

/**
 * @brief Maps the value for display on the oil component.
 * @param value The original value to map.
 * @return The mapped value for display.
 */
int32_t OemOilComponent::map_value_for_display(int32_t value) const
{
    // Default implementation - no mapping
    // Derived classes can override for special mapping (e.g., temperature component)
    return value;
}

/**
 * @brief Updates pivot styling based on current theme
 */
void OemOilComponent::update_pivot_styling()
{
    if (pivotCircle_)
    {
        const ThemeColors &colours = styleManager_->GetThemeColors();

        if (styleManager_->GetCurrentTheme() == Themes::NIGHT)
        {
            // Night mode - make pivot completely invisible by matching background exactly
            lv_obj_set_style_bg_color(pivotCircle_, colours.background, MAIN_DEFAULT);
            lv_obj_set_style_bg_grad_color(pivotCircle_, colours.background, MAIN_DEFAULT);
            lv_obj_set_style_bg_grad_dir(pivotCircle_, LV_GRAD_DIR_NONE, MAIN_DEFAULT); // Disable gradient completely
            lv_obj_set_style_bg_opa(pivotCircle_, LV_OPA_COVER, MAIN_DEFAULT);          // Full opacity
            lv_obj_set_style_border_width(pivotCircle_, 0, MAIN_DEFAULT);               // No border
            lv_obj_set_style_border_opa(pivotCircle_, LV_OPA_TRANSP, MAIN_DEFAULT);     // No border opacity
            lv_obj_set_style_shadow_opa(pivotCircle_, LV_OPA_TRANSP, MAIN_DEFAULT);     // No shadow
            lv_obj_set_style_outline_width(pivotCircle_, 0, MAIN_DEFAULT);              // No outline

            // Hide highlight in night mode
            if (pivotHighlight_)
            {
                lv_obj_add_flag(pivotHighlight_, LV_OBJ_FLAG_HIDDEN);
            }
        }
        else
        {
            // Day mode - visible plastic appearance with radial gradient
            lv_obj_set_style_bg_color(pivotCircle_, lv_color_hex(Colors::PIVOT_CIRCLE_CENTER), MAIN_DEFAULT);      // Medium gray center
            lv_obj_set_style_bg_grad_color(pivotCircle_, lv_color_hex(Colors::PIVOT_CIRCLE_EDGE), MAIN_DEFAULT); // Dark gray edge
            lv_obj_set_style_bg_grad_dir(pivotCircle_, LV_GRAD_DIR_HOR,
                                         MAIN_DEFAULT);                        // Horizontal for radial-like effect
            lv_obj_set_style_bg_grad_stop(pivotCircle_, 180, MAIN_DEFAULT);    // Gradient more toward edge
            lv_obj_set_style_bg_opa(pivotCircle_, LV_OPA_COVER, MAIN_DEFAULT); // Full opacity

            // Dark beveled border (darker than main body)
            lv_obj_set_style_border_width(pivotCircle_, 2, MAIN_DEFAULT);
            lv_obj_set_style_border_color(pivotCircle_, lv_color_hex(Colors::PIVOT_CIRCLE_BORDER), MAIN_DEFAULT); // Very dark border
            lv_obj_set_style_border_opa(pivotCircle_, LV_OPA_COVER, MAIN_DEFAULT);             // Solid border

            // Subtle shadow for depth
            lv_obj_set_style_shadow_color(pivotCircle_, lv_color_hex(Colors::PIVOT_CIRCLE_SHADOW), MAIN_DEFAULT);
            lv_obj_set_style_shadow_width(pivotCircle_, 3U, MAIN_DEFAULT);      // Moderate shadow
            lv_obj_set_style_shadow_opa(pivotCircle_, LV_OPA_20, MAIN_DEFAULT); // Subtle
            lv_obj_set_style_shadow_spread(pivotCircle_, 1, MAIN_DEFAULT);
            lv_obj_set_style_shadow_offset_x(pivotCircle_, 1, MAIN_DEFAULT);
            lv_obj_set_style_shadow_offset_y(pivotCircle_, 1, MAIN_DEFAULT);
            lv_obj_set_style_outline_width(pivotCircle_, 0, MAIN_DEFAULT); // No outline

            // Show highlight in day mode
            if (pivotHighlight_)
            {
                lv_obj_clear_flag(pivotHighlight_, LV_OBJ_FLAG_HIDDEN);
            }
        }

        // Force object invalidation to ensure immediate visual update
        lv_obj_invalidate(pivotCircle_);
    }
}

// Private Methods

/**
 * @brief Creates the oil icon for the oil component.
 */
void OemOilComponent::create_icon()
{
    const ThemeColors &colours = styleManager_->GetThemeColors();

    oilIcon_ = lv_image_create(scale_);
    lv_image_set_src(oilIcon_, get_icon());
    lv_image_set_scale(oilIcon_, 50);
    lv_obj_align(oilIcon_, LV_ALIGN_CENTER, 0, get_icon_y_offset());
    lv_obj_set_style_opa(oilIcon_, LV_OPA_COVER, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor(oilIcon_, colours.gaugeNormal, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(oilIcon_, LV_OPA_COVER, MAIN_DEFAULT);
}

/**
 * @brief Creates L and H labels positioned relative to the scale rotation and angle range.
 * Labels automatically follow when scale rotation changes.
 */
void OemOilComponent::create_labels()
{
    const ThemeColors &colours = styleManager_->GetThemeColors();

    // Create "L" label for low end
    lowLabel_ = lv_label_create(scale_);
    lv_label_set_text(lowLabel_, UIConstants::GAUGE_LOW_LABEL);
    if (styleManager_ && styleManager_->IsInitialized())
    {
        lv_obj_add_style(lowLabel_, &styleManager_->GetTextStyle(), MAIN_DEFAULT);
    }
    lv_obj_set_style_text_font(lowLabel_, &lv_font_montserrat_18, MAIN_DEFAULT);

    // Create "H" label for high end
    highLabel_ = lv_label_create(scale_);
    lv_label_set_text(highLabel_, UIConstants::GAUGE_HIGH_LABEL);
    if (styleManager_ && styleManager_->IsInitialized())
    {
        lv_obj_add_style(highLabel_, &styleManager_->GetTextStyle(), MAIN_DEFAULT);
    }
    lv_obj_set_style_text_font(highLabel_, &lv_font_montserrat_18, MAIN_DEFAULT);

    // Calculate label positions based on scale rotation and angle range
    // Allow derived classes to customize label positioning (e.g., for reversed scales)
    int32_t lAngle, hAngle;
    get_label_angles(lAngle, hAngle);

    // Use same radius for both labels - they should be equidistant from pivot
    int32_t radius = 78;

    // L label position
    double lAngleRad = (lAngle * M_PI) / 180.0;
    int32_t lX = (int32_t)(radius * cos(lAngleRad));
    int32_t lY = (int32_t)(radius * sin(lAngleRad));
    lv_obj_align(lowLabel_, LV_ALIGN_CENTER, lX, lY);

    // H label position
    double hAngleRad = (hAngle * M_PI) / 180.0;
    int32_t hX = (int32_t)(radius * cos(hAngleRad));
    int32_t hY = (int32_t)(radius * sin(hAngleRad));
    lv_obj_align(highLabel_, LV_ALIGN_CENTER, hX, hY);
}

/**
 * @brief Creates the needle line for the oil component.
 */
void OemOilComponent::create_needle()
{
    const ThemeColors &colours = styleManager_->GetThemeColors();

    // Create realistic 3-section tapered needle (based on actual car dashboard reference)

    // Section 1: Tip section with enhanced 3D effect - thinnest (outer third)
    needleLine_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needleLine_, colours.needleNormal, MAIN_DEFAULT); // Bright white from theme
    lv_obj_set_style_line_width(needleLine_, 4, MAIN_DEFAULT);                    // Slightly thicker tip
    lv_obj_set_style_line_rounded(needleLine_, true, MAIN_DEFAULT);               // Rounded ends
    lv_obj_set_style_line_opa(needleLine_, LV_OPA_COVER, MAIN_DEFAULT);

    // Section 2: Middle section with enhanced 3D effect - medium thickness (middle third)
    needleMiddle_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needleMiddle_, lv_color_darken(colours.needleNormal, 10),
                                MAIN_DEFAULT);                   // Slightly darker for gradient
    lv_obj_set_style_line_width(needleMiddle_, 5, MAIN_DEFAULT); // Thicker medium section
    lv_obj_set_style_line_rounded(needleMiddle_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needleMiddle_, LV_OPA_COVER, MAIN_DEFAULT);

    // Section 3: Base section with enhanced 3D effect - thickest (inner third near pivot)
    needleBase_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needleBase_, lv_color_darken(colours.needleNormal, 20),
                                MAIN_DEFAULT);                 // Darkest for gradient
    lv_obj_set_style_line_width(needleBase_, 7, MAIN_DEFAULT); // Thickest base section
    lv_obj_set_style_line_rounded(needleBase_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needleBase_, LV_OPA_COVER, MAIN_DEFAULT);

    // Add subtle highlight lines for enhanced 3D effect (very subtle to avoid artifacts)

    // Highlight for tip section - very subtle white highlight
    needleHighlightLine_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needleHighlightLine_, lv_color_hex(Colors::NEEDLE_HIGHLIGHT), MAIN_DEFAULT); // Pure white highlight
    lv_obj_set_style_line_width(needleHighlightLine_, 1, MAIN_DEFAULT);                      // Thin highlight line
    lv_obj_set_style_line_rounded(needleHighlightLine_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needleHighlightLine_, LV_OPA_20, MAIN_DEFAULT); // Very subtle

    // Highlight for middle section
    needleHighlightMiddle_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needleHighlightMiddle_, lv_color_hex(Colors::NEEDLE_HIGHLIGHT), MAIN_DEFAULT); // Pure white highlight
    lv_obj_set_style_line_width(needleHighlightMiddle_, 1, MAIN_DEFAULT);                      // Thin highlight line
    lv_obj_set_style_line_rounded(needleHighlightMiddle_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needleHighlightMiddle_, LV_OPA_20, MAIN_DEFAULT); // Very subtle

    // Highlight for base section
    needleHighlightBase_ = lv_line_create(scale_);
    lv_obj_set_style_line_color(needleHighlightBase_, lv_color_hex(Colors::NEEDLE_HIGHLIGHT), MAIN_DEFAULT); // Pure white highlight
    lv_obj_set_style_line_width(needleHighlightBase_, 1, MAIN_DEFAULT);                      // Thin highlight line
    lv_obj_set_style_line_rounded(needleHighlightBase_, true, MAIN_DEFAULT);
    lv_obj_set_style_line_opa(needleHighlightBase_, LV_OPA_20, MAIN_DEFAULT); // Very subtle

    // Realistic dark plastic pivot point (based on actual car dashboard reference)
    pivotCircle_ = lv_obj_create(scale_);
    lv_obj_set_size(pivotCircle_, 40U, 40U); // Even larger for better visibility and proportion
    lv_obj_center(pivotCircle_);
    lv_obj_set_style_radius(pivotCircle_, LV_RADIUS_CIRCLE, MAIN_DEFAULT);

    // Center light pickup highlight (where light hits the plastic)
    pivotHighlight_ = lv_obj_create(pivotCircle_);
    lv_obj_set_size(pivotHighlight_, 16U, 16U); // Proportional to larger pivot
    lv_obj_center(pivotHighlight_);
    lv_obj_set_style_radius(pivotHighlight_, LV_RADIUS_CIRCLE, MAIN_DEFAULT);
    lv_obj_set_style_bg_color(pivotHighlight_, lv_color_hex(Colors::PIVOT_HIGHLIGHT), MAIN_DEFAULT); // Light gray highlight
    lv_obj_set_style_bg_opa(pivotHighlight_, LV_OPA_80, MAIN_DEFAULT);                // More opaque for plastic look
    lv_obj_set_style_border_width(pivotHighlight_, 0, MAIN_DEFAULT);

    // Style pivot based on theme
    update_pivot_styling();
}

/**
 * @brief Sets up the scale properties for the oil component.
 */
void OemOilComponent::create_scale(int32_t rotation)
{
    if (!scale_)
    {
        return;
    }

    // Store rotation for label positioning
    scaleRotation_ = rotation;

    // Set scale properties from derived class
    lv_scale_set_mode(scale_, get_scale_mode());
    lv_scale_set_rotation(scale_, rotation);
    lv_scale_set_angle_range(scale_, get_angle_range());
    lv_scale_set_range(scale_, get_scale_min(), get_scale_max());

    // Set tick properties to match fuel gauge pattern
    lv_scale_set_total_tick_count(scale_, 13);
    lv_scale_set_major_tick_every(scale_, 3);
    lv_scale_set_label_show(scale_, false); // Disable built-in labels, use custom L/H positioning

    if (styleManager_ && styleManager_->IsInitialized())
    {
        lv_obj_add_style(scale_, &styleManager_->GetGaugeMainStyle(), MAIN_DEFAULT);
        lv_obj_add_style(scale_, &styleManager_->GetGaugeIndicatorStyle(), INDICATOR_DEFAULT);
        lv_obj_add_style(scale_, &styleManager_->GetGaugeItemsStyle(), ITEMS_DEFAULT);
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

    if (styleManager_ && styleManager_->IsInitialized())
    {
        lv_scale_section_set_style(section, MAIN_DEFAULT, &styleManager_->GetGaugeMainStyle());
        lv_scale_section_set_style(section, INDICATOR_DEFAULT, &styleManager_->GetGaugeDangerSectionStyle());
        lv_scale_section_set_style(section, ITEMS_DEFAULT, &styleManager_->GetGaugeDangerSectionStyle());
    }
    else
    {
        log_w("StyleService not available for danger zone style application");
    }

    // Set danger zone range - derived classes will handle specific ranges
    setup_danger_zone(section);
}