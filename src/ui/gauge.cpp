#include "ui/gauge.h"

#include <math.h>

#include "clarity/core/units.h"
#include "definitions/constants.h"
#include "ui/gauge_specs.h"

// The two specs keep the behaviour of the old pressure and temperature components
static_assert(PRESSURE_GAUGE.InDanger(5) && !PRESSURE_GAUGE.InDanger(6), "pressure: danger at 0.5 bar and below");
static_assert(TEMPERATURE_GAUGE.InDanger(100) && !TEMPERATURE_GAUGE.InDanger(99), "temperature: danger from 100");
static_assert(PRESSURE_GAUGE.Position(40) == 40, "pressure is not mirrored");
static_assert(TEMPERATURE_GAUGE.Position(0) == 120 && TEMPERATURE_GAUGE.Position(120) == 0, "temperature is mirrored");

namespace
{
constexpr int32_t SCALE_SIZE = 240;
constexpr int32_t LABEL_RADIUS = 78;

/// Needle section lengths and widths, tip first: a thin tip, a medium middle and a thick base
constexpr int32_t SECTION_WIDTH[3] = {4, 5, 7};

int32_t SectionLength(int32_t section, int32_t full)
{
    return section == 0 ? full : (section == 1 ? (full * 2) / 3 : full / 3);
}
} // namespace

// ========== Public Interface Methods ==========

Gauge::Gauge(const GaugeSpec &spec, int32_t deadband_percent)
    : spec_(spec), motion_(spec.min, spec.max, clarity::core::DeadbandUnits(spec.min, spec.max, deadband_percent))
{
}

void Gauge::Build(lv_obj_t *parent, Theme &theme)
{
    motion_.Reset();
    danger_ = false;
    palette_ = &theme.Colors();

    CreateScale(parent, theme);
    CreateIcon(*palette_);
    CreateLabels(theme);
    CreateNeedle(*palette_);
}

void Gauge::SetValue(float value)
{
    if (!scale_)
    {
        return;
    }

    const int32_t target = motion_.Clamp(clarity::core::RoundToInt32(value));
    // A running animation would move the needle on from here
    lv_anim_delete(scale_, AnimExec);
    motion_.Jump(target);
    ColourFor(spec_.InDanger(target), *palette_);
    MoveNeedle(target);
}

void Gauge::AnimateTo(float value)
{
    if (!scale_ || Animating())
    {
        return;
    }

    const clarity::core::GaugeMotion::Step step = motion_.Next(clarity::core::RoundToInt32(value));
    if (!step.animate)
    {
        return;
    }

    ColourFor(spec_.InDanger(step.to), *palette_);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, scale_);
    lv_anim_set_exec_cb(&animation, AnimExec);
    lv_anim_set_completed_cb(&animation, AnimDone);
    lv_anim_set_values(&animation, step.from, step.to);
    lv_anim_set_duration(&animation, ANIMATION_MS);
    lv_anim_start(&animation);
}

void Gauge::Restyle(const Palette &palette)
{
    if (!scale_)
    {
        return;
    }

    ColourFor(danger_, palette);
}

void Gauge::SetDeadbandPercent(int32_t percent)
{
    motion_.SetDeadband(clarity::core::DeadbandUnits(spec_.min, spec_.max, percent));
}

bool Gauge::Animating() const
{
    return scale_ && lv_anim_get(scale_, AnimExec) != nullptr;
}

// ========== Animation Callbacks ==========

void Gauge::AnimExec(void *var, int32_t value)
{
    auto *gauge = static_cast<Gauge *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(var)));
    if (gauge)
    {
        gauge->MoveNeedle(value);
    }
}

void Gauge::AnimDone(lv_anim_t *animation)
{
    auto *gauge = static_cast<Gauge *>(lv_obj_get_user_data(static_cast<lv_obj_t *>(animation->var)));
    if (gauge)
    {
        gauge->motion_.Settled(animation->current_value);
    }
}

// ========== Private Methods ==========

void Gauge::CreateScale(lv_obj_t *parent, Theme &theme)
{
    scale_ = lv_scale_create(parent);
    lv_obj_set_size(scale_, SCALE_SIZE, SCALE_SIZE);
    lv_obj_align(scale_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_user_data(scale_, this);

    lv_scale_set_mode(scale_, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_rotation(scale_, spec_.rotation);
    lv_scale_set_angle_range(scale_, spec_.sweep);
    lv_scale_set_range(scale_, spec_.min, spec_.max);
    lv_scale_set_total_tick_count(scale_, 13);
    lv_scale_set_major_tick_every(scale_, 3);
    lv_scale_set_label_show(scale_, false); // L and H are placed by CreateLabels()

    lv_obj_add_style(scale_, &theme.GaugeMainStyle(), MAIN_DEFAULT);
    lv_obj_add_style(scale_, &theme.GaugeIndicatorStyle(), INDICATOR_DEFAULT);
    lv_obj_add_style(scale_, &theme.GaugeItemsStyle(), ITEMS_DEFAULT);

    lv_scale_section_t *section = lv_scale_add_section(scale_);
    if (!section)
    {
        return;
    }

    lv_scale_section_set_style(section, MAIN_DEFAULT, &theme.GaugeMainStyle());
    lv_scale_section_set_style(section, INDICATOR_DEFAULT, &theme.GaugeDangerSectionStyle());
    lv_scale_section_set_style(section, ITEMS_DEFAULT, &theme.GaugeDangerSectionStyle());

    // The danger zone, in scale positions: a mirrored gauge draws it at the mirrored end
    const int32_t from = spec_.Position(spec_.dangerAbove ? spec_.danger : spec_.min);
    const int32_t to = spec_.Position(spec_.dangerAbove ? spec_.max : spec_.danger);
    lv_scale_section_set_range(section, from < to ? from : to, from < to ? to : from);
}

void Gauge::CreateIcon(const Palette &palette)
{
    icon_ = lv_image_create(scale_);
    lv_image_set_src(icon_, spec_.icon);
    lv_image_set_scale(icon_, 50);
    lv_obj_align(icon_, LV_ALIGN_CENTER, 0, spec_.iconYOffset);
    lv_obj_set_style_opa(icon_, LV_OPA_COVER, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor(icon_, palette.gaugeNormal, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(icon_, LV_OPA_COVER, MAIN_DEFAULT);
}

void Gauge::CreateLabels(Theme &theme)
{
    // L sits at the low end of the arc and H at the high end; a mirrored gauge swaps them
    const int32_t start_angle = spec_.rotation;
    const int32_t end_angle = spec_.rotation + spec_.sweep;
    const int32_t low_angle = spec_.reversed ? end_angle : start_angle;
    const int32_t high_angle = spec_.reversed ? start_angle : end_angle;

    const struct
    {
        const char *text;
        int32_t angle;
    } labels[] = {{ui_constants::GAUGE_LOW_LABEL, low_angle}, {ui_constants::GAUGE_HIGH_LABEL, high_angle}};

    for (const auto &entry : labels)
    {
        lv_obj_t *label = lv_label_create(scale_);
        lv_label_set_text_static(label, entry.text);
        lv_obj_add_style(label, &theme.TextStyle(), MAIN_DEFAULT);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_18, MAIN_DEFAULT);

        const double radians = (static_cast<double>(entry.angle) * M_PI) / 180.0;
        const auto x = static_cast<int32_t>(LABEL_RADIUS * cos(radians));
        const auto y = static_cast<int32_t>(LABEL_RADIUS * sin(radians));
        lv_obj_align(label, LV_ALIGN_CENTER, x, y);
    }
}

void Gauge::CreateNeedle(const Palette &palette)
{
    // Three tapered sections with a thin, faint highlight line on each, then the pivot over their base
    for (int32_t i = 0; i < SECTIONS; ++i)
    {
        needle_[i] = lv_line_create(scale_);
        lv_obj_set_style_line_width(needle_[i], SECTION_WIDTH[i], MAIN_DEFAULT);
        lv_obj_set_style_line_rounded(needle_[i], true, MAIN_DEFAULT);
        lv_obj_set_style_line_opa(needle_[i], LV_OPA_COVER, MAIN_DEFAULT);
    }

    for (int32_t i = 0; i < SECTIONS; ++i)
    {
        highlight_[i] = lv_line_create(scale_);
        lv_obj_set_style_line_width(highlight_[i], 1, MAIN_DEFAULT);
        lv_obj_set_style_line_rounded(highlight_[i], true, MAIN_DEFAULT);
        lv_obj_set_style_line_opa(highlight_[i], LV_OPA_20, MAIN_DEFAULT);
    }

    pivot_ = lv_obj_create(scale_);
    lv_obj_set_size(pivot_, 40U, 40U);
    lv_obj_center(pivot_);
    lv_obj_set_style_radius(pivot_, LV_RADIUS_CIRCLE, MAIN_DEFAULT);

    pivot_highlight_ = lv_obj_create(pivot_);
    lv_obj_set_size(pivot_highlight_, 16U, 16U);
    lv_obj_center(pivot_highlight_);
    lv_obj_set_style_radius(pivot_highlight_, LV_RADIUS_CIRCLE, MAIN_DEFAULT);
    lv_obj_set_style_bg_color(pivot_highlight_, palette.pivotHighlight, MAIN_DEFAULT);
    lv_obj_set_style_bg_opa(pivot_highlight_, LV_OPA_80, MAIN_DEFAULT);
    lv_obj_set_style_border_width(pivot_highlight_, 0, MAIN_DEFAULT);

    ColourFor(false, palette);
    MoveNeedle(spec_.min);
}

/// Sets every colour that depends on the palette or on whether the value is in the danger zone: the needle, its
/// highlights, the icon and the pivot. The night palette's danger colour is its normal colour, so the icon stays
/// red there
void Gauge::ColourFor(bool danger, const Palette &palette)
{
    palette_ = &palette;
    danger_ = danger;

    const lv_color_t needle = danger ? palette.needleDanger : palette.needleNormal;
    lv_obj_set_style_line_color(needle_[0], needle, MAIN_DEFAULT);
    lv_obj_set_style_line_color(needle_[1], lv_color_darken(needle, 10), MAIN_DEFAULT);
    lv_obj_set_style_line_color(needle_[2], lv_color_darken(needle, 20), MAIN_DEFAULT);

    if (danger)
    {
        lv_obj_set_style_line_color(highlight_[0], lv_color_lighten(palette.needleDanger, 30), MAIN_DEFAULT);
        lv_obj_set_style_line_color(highlight_[1], lv_color_lighten(palette.needleDanger, 20), MAIN_DEFAULT);
        lv_obj_set_style_line_color(highlight_[2], lv_color_lighten(palette.needleDanger, 10), MAIN_DEFAULT);
    }
    else
    {
        for (lv_obj_t *highlight : highlight_)
        {
            lv_obj_set_style_line_color(highlight, palette.needleNormal, MAIN_DEFAULT);
        }
    }

    lv_obj_set_style_image_recolor(icon_, danger ? palette.gaugeDanger : palette.gaugeNormal, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(icon_, LV_OPA_COVER, MAIN_DEFAULT);

    StylePivot(palette);
}

void Gauge::StylePivot(const Palette &palette)
{
    if (!palette.pivotVisible)
    {
        // Night: the pivot takes the background colour, with no gradient, border, shadow or highlight
        lv_obj_set_style_bg_color(pivot_, palette.background, MAIN_DEFAULT);
        lv_obj_set_style_bg_grad_color(pivot_, palette.background, MAIN_DEFAULT);
        lv_obj_set_style_bg_grad_dir(pivot_, LV_GRAD_DIR_NONE, MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(pivot_, LV_OPA_COVER, MAIN_DEFAULT);
        lv_obj_set_style_border_width(pivot_, 0, MAIN_DEFAULT);
        lv_obj_set_style_border_opa(pivot_, LV_OPA_TRANSP, MAIN_DEFAULT);
        lv_obj_set_style_shadow_opa(pivot_, LV_OPA_TRANSP, MAIN_DEFAULT);
        lv_obj_set_style_outline_width(pivot_, 0, MAIN_DEFAULT);
        lv_obj_add_flag(pivot_highlight_, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        // Day: dark plastic with a radial-looking gradient, a bevelled border and a faint shadow
        lv_obj_set_style_bg_color(pivot_, palette.pivotCenter, MAIN_DEFAULT);
        lv_obj_set_style_bg_grad_color(pivot_, palette.pivotEdge, MAIN_DEFAULT);
        lv_obj_set_style_bg_grad_dir(pivot_, LV_GRAD_DIR_HOR, MAIN_DEFAULT);
        lv_obj_set_style_bg_grad_stop(pivot_, 180, MAIN_DEFAULT);
        lv_obj_set_style_bg_opa(pivot_, LV_OPA_COVER, MAIN_DEFAULT);
        lv_obj_set_style_border_width(pivot_, 2, MAIN_DEFAULT);
        lv_obj_set_style_border_color(pivot_, palette.pivotBorder, MAIN_DEFAULT);
        lv_obj_set_style_border_opa(pivot_, LV_OPA_COVER, MAIN_DEFAULT);
        lv_obj_set_style_shadow_color(pivot_, palette.pivotShadow, MAIN_DEFAULT);
        lv_obj_set_style_shadow_width(pivot_, 3U, MAIN_DEFAULT);
        lv_obj_set_style_shadow_opa(pivot_, LV_OPA_20, MAIN_DEFAULT);
        lv_obj_set_style_shadow_spread(pivot_, 1, MAIN_DEFAULT);
        lv_obj_set_style_shadow_offset_x(pivot_, 1, MAIN_DEFAULT);
        lv_obj_set_style_shadow_offset_y(pivot_, 1, MAIN_DEFAULT);
        lv_obj_set_style_outline_width(pivot_, 0, MAIN_DEFAULT);
        lv_obj_clear_flag(pivot_highlight_, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_invalidate(pivot_);
}

void Gauge::MoveNeedle(int32_t value)
{
    const int32_t position = spec_.Position(motion_.Clamp(value));
    for (int32_t i = 0; i < SECTIONS; ++i)
    {
        lv_scale_set_line_needle_value(scale_, needle_[i], SectionLength(i, NEEDLE_LENGTH), position);
        lv_scale_set_line_needle_value(scale_, highlight_[i], SectionLength(i, NEEDLE_LENGTH) - 2, position);
    }
}
