#include "ui/panel.h"

void Panel::Enter()
{
    if (screen_)
    {
        return;
    }

    screen_ = lv_obj_create(nullptr);
    Build(screen_);
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
