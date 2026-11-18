#include "ui/menu_wheel.h"

#include <cstdlib>

#include "definitions/constants.h"

namespace
{
constexpr int SCREEN_SIZE = 240;
constexpr int MENU_WIDTH = 200;
constexpr int MENU_HEIGHT = 180;
constexpr int ROW_HEIGHT = 24;
constexpr int FIRST_ROW_Y = -60;
} // namespace

// ========== Public Interface Methods ==========

void MenuWheel::Build(lv_obj_t *parent, const Palette &palette)
{
    palette_ = &palette;
    items_.clear();
    index_ = 0;

    container_ = lv_obj_create(parent);
    lv_obj_set_size(container_, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_align(container_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(container_, SCREEN_SIZE / 2, LV_PART_MAIN);

    title_ = lv_label_create(container_);
    lv_obj_set_style_text_font(title_, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_align(title_, LV_ALIGN_TOP_MID, 0, 15);

    // The rows sit in their own container, which covers most of the round screen
    lv_obj_t *rows = lv_obj_create(container_);
    lv_obj_set_size(rows, SCREEN_SIZE, MENU_HEIGHT);
    lv_obj_align(rows, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(rows, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(rows, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(rows, 0, LV_PART_MAIN);
    lv_obj_clear_flag(rows, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < VISIBLE_ROWS; ++i)
    {
        lv_obj_t *row = lv_label_create(rows);
        lv_obj_set_width(row, MENU_WIDTH);
        lv_obj_set_style_text_align(row, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(row, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_align(row, LV_ALIGN_CENTER, 0, FIRST_ROW_Y + (i * ROW_HEIGHT));
        rows_[i] = row;
    }

    hint_ = lv_label_create(container_);
    lv_label_set_text(hint_, ui_strings::hint_text::SHORT_LONG_PRESS);
    lv_obj_set_style_text_font(hint_, &lv_font_montserrat_10, LV_PART_MAIN);
    lv_obj_align(hint_, LV_ALIGN_BOTTOM_MID, 0, -15);

    Restyle(palette);
}

void MenuWheel::SetTitle(const std::string &title)
{
    if (title_)
    {
        lv_label_set_text(title_, title.c_str());
    }
}

void MenuWheel::SetHint(const std::string &hint)
{
    if (hint_)
    {
        lv_label_set_text(hint_, hint.c_str());
    }
}

void MenuWheel::SetItems(const std::vector<std::string> &items)
{
    items_ = items;
    if (index_ >= items_.size())
    {
        index_ = 0;
    }
    Redraw();
}

void MenuWheel::SetIndex(std::size_t index)
{
    if (index >= items_.size())
    {
        return;
    }

    index_ = index;
    Redraw();
}

void MenuWheel::Restyle(const Palette &palette)
{
    palette_ = &palette;
    if (!container_)
    {
        return;
    }

    lv_obj_set_style_bg_color(container_, palette.menuBackground, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_, palette.menuTitle, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint_, palette.menuHint, LV_PART_MAIN);
    Redraw();
}

// ========== Private Methods ==========

/// Rows further from the centre are darker, smaller, fainter and pulled in towards it; the centre row is
/// highlighted. Rows past the end of a short list are hidden
void MenuWheel::Redraw()
{
    if (!container_ || items_.empty())
    {
        return;
    }

    const int count = static_cast<int>(items_.size());
    for (int i = 0; i < VISIBLE_ROWS; ++i)
    {
        lv_obj_t *row = rows_[i];
        const int item = (static_cast<int>(index_) - CENTER_ROW + i + count) % count;
        lv_label_set_text(row, items_[item].c_str());

        const int distance = std::abs(i - CENTER_ROW);
        if (distance == 0)
        {
            StyleCenterRow(row);
        }
        else
        {
            lv_obj_set_style_text_color(row, RowColour(distance), LV_PART_MAIN);

            const lv_font_t *font = &lv_font_montserrat_8;
            if (distance == 1)
            {
                font = &lv_font_montserrat_14;
            }
            else if (distance == 2)
            {
                font = &lv_font_montserrat_12;
            }
            lv_obj_set_style_text_font(row, font, LV_PART_MAIN);
            lv_obj_set_style_text_opa(row, static_cast<lv_opa_t>(LV_OPA_100 - (distance * 35)), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(row, LV_OPA_0, LV_PART_MAIN);
            lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
        }

        int y = FIRST_ROW_Y + (i * ROW_HEIGHT);
        if (distance == 1)
        {
            y += i < CENTER_ROW ? -6 : 6; // room around the selected row
        }
        else if (distance >= 2)
        {
            y += i < CENTER_ROW ? -4 : 4; // the outer rows tuck in a little
        }
        lv_obj_align(row, LV_ALIGN_CENTER, 0, y);

        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
}

void MenuWheel::StyleCenterRow(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, palette_->menuSelectedItem, LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_opa(label, LV_OPA_100, LV_PART_MAIN);
    lv_obj_set_style_bg_color(label, palette_->menuSelectedBackground, LV_PART_MAIN);
    lv_obj_set_style_border_color(label, palette_->menuSelectedBorder, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_radius(label, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(label, 6, LV_PART_MAIN);
    lv_obj_set_style_border_width(label, 1, LV_PART_MAIN);
}

/// The unselected row colour: the palette's menu item colour, 30% darker per row of distance, at least 20% left
lv_color_t MenuWheel::RowColour(int distance) const
{
    float intensity = 1.0f - (static_cast<float>(distance) * 0.3f);
    if (intensity < 0.2f)
    {
        intensity = 0.2f;
    }

    const lv_color_t base = palette_->menuItem;
    return lv_color_make(static_cast<uint8_t>(static_cast<float>(base.red) * intensity),
                         static_cast<uint8_t>(static_cast<float>(base.green) * intensity),
                         static_cast<uint8_t>(static_cast<float>(base.blue) * intensity));
}
