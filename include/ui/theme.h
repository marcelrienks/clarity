#pragma once

#include <cstdint>

#include <lvgl.h>

#include "clarity/core/types.h"
#include "definitions/enums.h"

class IConfigurationManager;

inline constexpr lv_style_selector_t MAIN_DEFAULT = static_cast<lv_style_selector_t>(LV_PART_MAIN) | LV_STATE_DEFAULT;
inline constexpr lv_style_selector_t ITEMS_DEFAULT = static_cast<lv_style_selector_t>(LV_PART_ITEMS) | LV_STATE_DEFAULT;
inline constexpr lv_style_selector_t INDICATOR_DEFAULT =
    static_cast<lv_style_selector_t>(LV_PART_INDICATOR) | LV_STATE_DEFAULT;

/// lv_color_hex() as a constant expression, so the palettes are constexpr
constexpr lv_color_t ColorHex(uint32_t rgb)
{
    return lv_color_t{.blue = static_cast<uint8_t>(rgb & 0xFF),
                      .green = static_cast<uint8_t>((rgb >> 8) & 0xFF),
                      .red = static_cast<uint8_t>((rgb >> 16) & 0xFF)};
}

/**
 * @struct Palette
 * @brief Every colour role of one theme (A-08), including the Config menu and gauge pivot roles that were
 * separate Day and Night constants before
 */
struct Palette
{
    // Screen
    lv_color_t background;
    lv_color_t text;

    // Gauges
    lv_color_t gaugeNormal; ///< Scale icon and normal readings
    lv_color_t gaugeDanger; ///< Icon and danger section in the danger zone
    lv_color_t gaugeTicks;
    lv_color_t needleNormal;
    lv_color_t needleDanger;
    lv_color_t needleHighlight;
    lv_color_t pivotCenter;
    lv_color_t pivotEdge;
    lv_color_t pivotBorder;
    lv_color_t pivotShadow;
    lv_color_t pivotHighlight;
    bool pivotVisible; ///< False: the pivot takes the background colour, with no border, shadow or highlight

    // Key and lock icons
    lv_color_t keyPresent;
    lv_color_t keyNotPresent;
    lv_color_t lockEngaged;

    // Config menu
    lv_color_t menuBackground;
    lv_color_t menuTitle;
    lv_color_t menuHint;
    lv_color_t menuItem; ///< Unselected rows, darkened with the distance from the selected row
    lv_color_t menuSelectedItem;
    lv_color_t menuSelectedBackground;
    lv_color_t menuSelectedBorder;

    // Error panel, by error level
    lv_color_t errorCritical;
    lv_color_t errorError;
    lv_color_t errorWarning;

    lv_color_t ErrorColor(ErrorLevel level) const
    {
        switch (level)
        {
            case ErrorLevel::CRITICAL:
                return errorCritical;
            case ErrorLevel::ERROR:
                return errorError;
            default: // WARNING and anything unknown
                return errorWarning;
        }
    }
};

/// The palette of a theme; an unknown id gives the Day palette
const Palette &PaletteFor(clarity::core::ThemeId id);

/**
 * @class Theme
 * @brief The one theme state (A-08): the applied ThemeId, its palette and the shared LVGL styles
 *
 * @details The caller passes EffectiveTheme(request, inputs) to Apply() (Q-05: the lights signal selects Day or
 * Night, the error panel forces Error). Apply() does nothing when the theme is unchanged. Otherwise it rewrites
 * the shared styles and calls lv_obj_report_style_change(nullptr), so every object using them is refreshed on
 * every screen. Colours a widget sets locally (needle, pivot, icons, Config menu) are read from Colors() when it
 * builds or restyles.
 */
class Theme
{
  public:
    Theme() = default;
    Theme(const Theme &) = delete;
    Theme &operator=(const Theme &) = delete;
    ~Theme();

    /// Initialises the shared styles and applies Day. Call once, after lv_init()
    void Init();

    /// Applies a theme. Returns false, and does nothing, if it is already the applied one
    bool Apply(clarity::core::ThemeId id);

    /// The applied theme
    clarity::core::ThemeId Active() const
    {
        return active_;
    }

    /// The palette of the applied theme
    const Palette &Colors() const
    {
        return PaletteFor(active_);
    }

    /// Gives a screen the shared background style (it follows every later Apply())
    void ApplyBackground(lv_obj_t *screen)
    {
        lv_obj_add_style(screen, &background_style_, MAIN_DEFAULT);
    }

    lv_style_t &TextStyle()
    {
        return text_style_;
    }

    lv_style_t &GaugeMainStyle()
    {
        return gauge_main_style_;
    }

    lv_style_t &GaugeIndicatorStyle()
    {
        return gauge_indicator_style_;
    }

    lv_style_t &GaugeItemsStyle()
    {
        return gauge_items_style_;
    }

    lv_style_t &GaugeDangerSectionStyle()
    {
        return gauge_danger_section_style_;
    }

    /// Registers the Display section (brightness) that StyleManager registered; phase 6 replaces it
    static void RegisterConfigSchema(IConfigurationManager *configuration_manager);

  private:
    void SetStyles(const Palette &palette);

    bool initialized_ = false;
    clarity::core::ThemeId active_ = clarity::core::ThemeId::Day;

    lv_style_t background_style_{};           ///< Screen background
    lv_style_t text_style_{};                 ///< General text
    lv_style_t gauge_main_style_{};           ///< Gauge scale main part
    lv_style_t gauge_indicator_style_{};      ///< Gauge major ticks
    lv_style_t gauge_items_style_{};          ///< Gauge minor ticks
    lv_style_t gauge_danger_section_style_{}; ///< Gauge danger zone
};
