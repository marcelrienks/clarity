#include "ui/icon_view.h"

#include "ui/theme.h"

void IconView::Build(lv_obj_t *parent, const lv_image_dsc_t *source, lv_align_t align, int32_t x_offset,
                     int32_t y_offset)
{
    image_ = lv_image_create(parent);
    lv_image_set_src(image_, source);
    lv_obj_align(image_, align, x_offset, y_offset);
}

void IconView::SetSource(const lv_image_dsc_t *source)
{
    if (image_)
    {
        lv_image_set_src(image_, source);
    }
}

void IconView::SetColor(lv_color_t colour)
{
    if (!image_)
    {
        return;
    }

    lv_obj_set_style_image_recolor(image_, colour, MAIN_DEFAULT);
    lv_obj_set_style_image_recolor_opa(image_, LV_OPA_COVER, MAIN_DEFAULT);
}
