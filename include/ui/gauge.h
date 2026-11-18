#pragma once

#include <cstdint>

#include <lvgl.h>

#include "clarity/core/gauge_motion.h"
#include "ui/theme.h"

/**
 * @struct GaugeSpec
 * @brief Everything that tells one gauge from another (A-09), so one class draws both
 *
 * @details Values are scale units: the pressure gauge uses 0.1 bar (0 to 60), the temperature gauge degrees
 * Celsius (0 to 120). `reversed` mirrors the scale (the needle shows min + max - value, the danger zone and the
 * L and H labels move with it), so the zero end sits at the far end of the arc.
 */
struct GaugeSpec
{
    const lv_image_dsc_t *icon;
    int32_t min;
    int32_t max;
    int32_t danger;   ///< Edge of the danger zone, inclusive
    bool dangerAbove; ///< True: danger from `danger` up to max. False: from min up to `danger`
    int32_t rotation; ///< Degrees: where the scale starts
    int32_t sweep;    ///< Degrees: the arc the scale covers
    bool reversed;
    int32_t iconYOffset;

    constexpr bool InDanger(int32_t value) const
    {
        return dangerAbove ? value >= danger : value <= danger;
    }

    /// The scale position that shows a value
    constexpr int32_t Position(int32_t value) const
    {
        return reversed ? min + max - value : value;
    }
};

/**
 * @class Gauge
 * @brief An oil gauge: scale with a danger zone, a three-section needle, a pivot, an icon and L and H labels (A-09)
 *
 * @details Replaces `OemOilComponent` and its two subclasses. The dead-band and the animation start rules live in
 * `clarity::core::GaugeMotion` (host-tested); this class runs the LVGL animation they describe and colours the
 * needle, icon and pivot from the palette.
 *
 * The animation's var is the scale object, so LVGL removes it when the panel deletes the screen (rule 3 in
 * ui/panel.h). A gauge must not move or be copied after Build(): the scale keeps a pointer to it.
 *
 * Not used yet: the oil panel still draws with `OemOilComponent`; 5.5 moves it onto this.
 */
class Gauge
{
  public:
    static constexpr int32_t DEFAULT_DEADBAND_PERCENT = 3;

    explicit Gauge(const GaugeSpec &spec, int32_t deadband_percent = DEFAULT_DEADBAND_PERCENT);
    Gauge(const Gauge &) = delete;
    Gauge &operator=(const Gauge &) = delete;
    ~Gauge() = default;

    /// Creates the gauge on parent, centred. Forgets every earlier reading, so the first AnimateTo() is a first
    /// reading again. Call once per visit; the objects live until the panel deletes its screen
    void Build(lv_obj_t *parent, Theme &theme);

    /// Moves the needle straight to a value (no animation) and colours it for that value
    void SetValue(float value);

    /// Animates the needle to a value and colours it for that value. Does nothing while an animation is
    /// running (the next call picks the latest reading up) or when the change is inside the dead-band
    void AnimateTo(float value);

    /// Re-applies every locally set colour (needle, icon, pivot), for the value shown last
    void Restyle(const Palette &palette);

    /// Dead-band as a percent of the scale range (1 unit at least)
    void SetDeadbandPercent(int32_t percent);

    bool Animating() const;

  private:
    static constexpr int32_t NEEDLE_LENGTH = 90;
    static constexpr int32_t SECTIONS = 3; ///< Needle sections: tip, middle, base
    static constexpr int32_t ANIMATION_MS = 750;

    static void AnimExec(void *var, int32_t value);
    static void AnimDone(lv_anim_t *animation);

    void CreateScale(lv_obj_t *parent, Theme &theme);
    void CreateIcon(const Palette &palette);
    void CreateLabels(Theme &theme);
    void CreateNeedle(const Palette &palette);
    void ColourFor(bool danger, const Palette &palette);
    void StylePivot(const Palette &palette);
    void MoveNeedle(int32_t value);

    GaugeSpec spec_;
    clarity::core::GaugeMotion motion_;
    const Palette *palette_ = nullptr; ///< The palette the colours were last set from
    bool danger_ = false;              ///< Whether they were set for the danger zone

    lv_obj_t *scale_ = nullptr;
    lv_obj_t *needle_[SECTIONS] = {};
    lv_obj_t *highlight_[SECTIONS] = {};
    lv_obj_t *icon_ = nullptr;
    lv_obj_t *pivot_ = nullptr;
    lv_obj_t *pivot_highlight_ = nullptr;
};
