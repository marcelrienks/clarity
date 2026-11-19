#include "panels/lock_panel.h"

#include <icons/lock_alt_solid.h>

#include "utilities/logging.h"

void LockPanel::Build(lv_obj_t *screen, Theme &theme)
{
    icon_.Build(screen, &lock_alt_solid);
    icon_.SetColor(theme.Colors().lockEngaged);
}

clarity::core::NavIntent LockPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event != clarity::core::ButtonEvent::Long)
    {
        return clarity::core::NavIntent::None;
    }

    log_i("LockPanel long press - loading config panel");
    return clarity::core::NavIntent::OpenConfig;
}
