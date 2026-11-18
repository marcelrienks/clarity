#pragma once

#include <cstddef>
#include <cstdint>

#include <lvgl.h>

#include "clarity/core/error_log.h"
#include "ui/theme.h"

/**
 * @class ErrorView
 * @brief Draws one ErrorLog entry on a round screen (A-09): level, source, message, count and the key hint
 *
 * @details Show() reads the entry straight from the log and redraws only when the log's revision or the index
 * differs from the last draw (D-19), so calling it every loop pass costs two comparisons. It keeps no copy of the
 * log and does no sorting: the index is the position in the log's own order, which the error panel chooses.
 *
 * Not used yet: the error panel still draws with `ErrorComponent`; 5.5 moves it onto this.
 */
class ErrorView
{
  public:
    /// Creates the view on parent. The objects live until the panel deletes its screen. Forgets the last draw
    void Build(lv_obj_t *parent, const Palette &palette);

    /// Draws the entry at index (0 when out of range). Does nothing when the log is empty, so the "0 errors"
    /// screen never shows while the panel is closing, or when neither the log revision nor the index changed.
    /// Returns true when it redrew
    bool Show(const clarity::core::ErrorLog &log, std::size_t index);

    /// Recolours the level text and the border from a palette, for the entry shown last
    void Restyle(const Palette &palette);

  private:
    void Colour(clarity::core::ErrorLevel level);

    const Palette *palette_ = nullptr;
    lv_obj_t *container_ = nullptr;
    lv_obj_t *count_ = nullptr;
    lv_obj_t *level_ = nullptr;
    lv_obj_t *source_ = nullptr;
    lv_obj_t *message_ = nullptr;
    lv_obj_t *navigation_ = nullptr;

    bool drawn_ = false;
    uint32_t revision_ = 0;
    std::size_t index_ = 0;
    clarity::core::ErrorLevel level_shown_ = clarity::core::ErrorLevel::Warning;
};
