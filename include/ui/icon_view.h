#pragma once

#include <lvgl.h>

/**
 * @class IconView
 * @brief One recoloured icon (A-09): the Key and Lock panels' only widget
 *
 * @details Replaces `KeyComponent` and `LockComponent`. The panel picks the icon and the colour (the key state
 * and the palette are the panel's business), so the view logs nothing and holds no theme.
 *
 * Not used yet: the Key and Lock panels still draw with their components; 5.5 moves them onto this.
 */
class IconView
{
  public:
    /// Creates the icon on parent. The object lives until the panel deletes its screen
    void Build(lv_obj_t *parent, const lv_image_dsc_t *source, lv_align_t align = LV_ALIGN_CENTER, int32_t x_offset = 0,
               int32_t y_offset = 0);

    void SetSource(const lv_image_dsc_t *source);

    /// Recolours the whole icon
    void SetColor(lv_color_t colour);

  private:
    lv_obj_t *image_ = nullptr;
};
