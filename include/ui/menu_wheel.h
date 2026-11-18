#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include <lvgl.h>

#include "ui/theme.h"

/**
 * @class MenuWheel
 * @brief The Config menu's 5-row fading list on a round screen (A-09)
 *
 * @details Replaces the drawing half of `ConfigComponent`. It shows a title, five rows centred on the selected
 * item (wrapping around the list; rows further from the centre are smaller, fainter and darker) and a hint. It
 * knows nothing about what the items do: the Config panel maps a press on the selected index to its action and
 * returns the NavIntent. The wheel holds only the labels it draws.
 *
 * Not used yet: the Config panel still draws with `ConfigComponent`; 5.5 moves it onto this.
 */
class MenuWheel
{
  public:
    static constexpr int VISIBLE_ROWS = 5;
    static constexpr int CENTER_ROW = 2;

    /// Creates the wheel on parent, coloured from the palette. The objects live until the panel deletes its screen
    void Build(lv_obj_t *parent, const Palette &palette);

    void SetTitle(const std::string &title);
    void SetHint(const std::string &hint);

    /// Replaces the items and redraws; the selection is kept when it is still in range, else it goes to the first
    void SetItems(const std::vector<std::string> &items);

    /// Selects an item and redraws. An out-of-range index is ignored
    void SetIndex(std::size_t index);

    /// Recolours every row, the title, the hint and the background from a palette
    void Restyle(const Palette &palette);

  private:
    void Redraw();
    void StyleCenterRow(lv_obj_t *label);
    lv_color_t RowColour(int distance) const;

    const Palette *palette_ = nullptr;
    lv_obj_t *container_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *hint_ = nullptr;
    std::array<lv_obj_t *, VISIBLE_ROWS> rows_ = {};

    std::vector<std::string> items_;
    std::size_t index_ = 0;
};
