#include "ui/error_view.h"

#include <cstdio>

#include "definitions/constants.h"

namespace
{
constexpr int SCREEN_SIZE = 240;

const char *LevelText(clarity::core::ErrorLevel level)
{
    switch (level)
    {
        case clarity::core::ErrorLevel::Critical:
            return ui_strings::error_ui::LEVEL_CRITICAL;
        case clarity::core::ErrorLevel::Error:
            return ui_strings::error_ui::LEVEL_ERROR;
        case clarity::core::ErrorLevel::Warning:
            return ui_strings::error_ui::LEVEL_WARNING;
    }
    return ui_strings::error_ui::LEVEL_UNKNOWN;
}
} // namespace

// ========== Public Interface Methods ==========

void ErrorView::Build(lv_obj_t *parent, const Palette &palette)
{
    palette_ = &palette;
    drawn_ = false;

    // A circular container with a 2 px border in the error level's colour, over the screen's own background
    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_align(container_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(container_, SCREEN_SIZE / 2, 0);
    lv_obj_set_style_border_width(container_, 2, 0);
    lv_obj_set_style_bg_opa(container_, LV_OPA_TRANSP, 0);

    count_ = lv_label_create(container_);
    lv_obj_align(count_, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_text_font(count_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(count_, lv_color_white(), 0); // white on the dark error background
    lv_label_set_text_static(count_, ui_strings::error_ui::SINGLE_ERROR_COUNT);

    lv_obj_t *content = lv_obj_create(container_);
    lv_obj_set_size(content, 200, 180);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 5, 0);

    level_ = lv_label_create(content);
    lv_obj_align(level_, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(level_, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(level_, lv_color_white(), 0);
    lv_label_set_text_static(level_, ui_strings::error_ui::DEFAULT_ERROR_LEVEL);

    source_ = lv_label_create(content);
    lv_obj_align(source_, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_text_font(source_, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(source_, lv_color_white(), 0);
    lv_label_set_text_static(source_, ui_strings::error_ui::DEFAULT_ERROR_SOURCE);

    message_ = lv_label_create(content);
    lv_obj_set_size(message_, 180, 100);
    lv_obj_align(message_, LV_ALIGN_CENTER, 0, 25);
    lv_obj_set_style_text_font(message_, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(message_, lv_color_white(), 0);
    lv_obj_set_style_text_align(message_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(message_, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(message_, ui_strings::error_ui::LOADING_ERRORS_MESSAGE);

    navigation_ = lv_label_create(container_);
    lv_obj_align(navigation_, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_text_font(navigation_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(navigation_, lv_color_white(), 0);
    lv_label_set_text_static(navigation_, ui_strings::error_ui::LOADING_NAVIGATION);
}

bool ErrorView::Show(const clarity::core::ErrorLog &log, std::size_t index)
{
    if (!container_ || log.Count() == 0)
    {
        return false;
    }

    const std::size_t shown = index < log.Count() ? index : 0;
    if (drawn_ && log.Revision() == revision_ && shown == index_)
    {
        return false;
    }

    drawn_ = true;
    revision_ = log.Revision();
    index_ = shown;

    const clarity::core::ErrorInfo &error = log.At(shown);
    level_shown_ = error.level;

    char count[24];
    std::snprintf(count, sizeof(count), "errors: %zu", log.Count());
    lv_label_set_text(count_, count);

    lv_label_set_text_static(level_, LevelText(error.level));
    lv_label_set_text(source_, error.source);
    lv_label_set_text(message_, error.message);
    // Short press cycles through the errors, long press exits from any of them
    lv_label_set_text_static(navigation_, ui_strings::error_ui::NAVIGATION_INSTRUCTIONS);

    Colour(error.level);
    return true;
}

void ErrorView::Restyle(const Palette &palette)
{
    palette_ = &palette;
    if (drawn_)
    {
        Colour(level_shown_);
    }
}

// ========== Private Methods ==========

void ErrorView::Colour(clarity::core::ErrorLevel level)
{
    const lv_color_t colour = palette_->ErrorColor(level);
    lv_obj_set_style_text_color(level_, colour, 0);
    lv_obj_set_style_border_color(container_, colour, 0);
}
