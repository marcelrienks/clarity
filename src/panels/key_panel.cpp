#include "panels/key_panel.h"

#include <icons/key_solid.h>

#include "utilities/logging.h"

void KeyPanel::SetState(clarity::core::KeyState state)
{
    state_ = state;
    if (screen_)
    {
        ApplyColor();
    }
}

void KeyPanel::Build(lv_obj_t *screen, Theme &theme)
{
    theme_ = &theme;
    icon_.Build(screen, &key_solid);
    ApplyColor();
}

void KeyPanel::OnExit()
{
    theme_ = nullptr;
}

void KeyPanel::ApplyColor()
{
    // Inactive is treated as not present
    if (state_ == clarity::core::KeyState::Present)
    {
        icon_.SetColor(theme_->Colors().keyPresent);
        log_t("Key icon color set to GREEN");
    }
    else
    {
        icon_.SetColor(theme_->Colors().keyNotPresent);
        log_t("Key icon color set to RED");
    }
}

clarity::core::NavIntent KeyPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event != clarity::core::ButtonEvent::Long)
    {
        return clarity::core::NavIntent::None;
    }

    log_i("KeyPanel long press - loading config panel");
    return clarity::core::NavIntent::OpenConfig;
}
