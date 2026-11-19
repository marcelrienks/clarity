#include "ui/panel.h"

void Panel::Enter(Theme &theme)
{
    if (screen_)
    {
        return;
    }

    screen_ = lv_obj_create(nullptr);
    theme.ApplyBackground(screen_);
    Build(screen_, theme);
    lv_screen_load(screen_);
}

void Panel::Exit()
{
    if (!screen_)
    {
        return;
    }

    OnExit();

    lv_obj_t *screen = screen_;
    screen_ = nullptr;
    lv_obj_delete(screen);
}
