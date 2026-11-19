#include "panels/error_panel.h"

#include "utilities/logging.h"

ErrorPanel::ErrorPanel(clarity::core::ErrorLog &log) : log_(log)
{
}

void ErrorPanel::Build(lv_obj_t *screen, Theme &theme)
{
    log_i("Error panel activated - switching to error display mode");

    view_.Build(screen, theme.Colors());
    view_.Show(log_, NextToView());
}

void ErrorPanel::Update(uint32_t now)
{
    (void)now;
    if (!screen_)
    {
        return;
    }

    // Draws only when the log or the entry changed. Warnings that expire while the panel is open do not close
    // it: it leaves on a press (2.6)
    view_.Show(log_, NextToView());
}

std::size_t ErrorPanel::NextToView() const
{
    std::size_t best = log_.Count();
    for (std::size_t i = 0; i < log_.Count(); ++i)
    {
        const clarity::core::ErrorInfo &entry = log_.At(i);
        if (entry.acknowledged)
        {
            continue;
        }
        if (best == log_.Count() || entry.level > log_.At(best).level)
        {
            best = i;
        }
    }
    return best;
}

clarity::core::NavIntent ErrorPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event == clarity::core::ButtonEvent::Short)
    {
        return HandleShortPress();
    }
    if (event == clarity::core::ButtonEvent::Long)
    {
        return HandleLongPress();
    }
    return clarity::core::NavIntent::None;
}

clarity::core::NavIntent ErrorPanel::HandleShortPress()
{
    log_t("ErrorPanel::HandleShortPress() - Manual cycling to next error");

    const std::size_t index = NextToView();
    if (index >= log_.Count())
    {
        log_w("ErrorPanel::HandleShortPress() - No errors to cycle through");
        return clarity::core::NavIntent::ClearErrors;
    }

    log_t("ErrorPanel::HandleShortPress() - Current error index: %zu/%zu", index, log_.Count());
    log_t("ErrorPanel::HandleShortPress() - Advancing to next error");

    log_i("Removing viewed error: %s - %s", log_.At(index).source, log_.At(index).message);
    log_.DismissAt(index);

    if (NextToView() >= log_.Count())
    {
        // The last error was viewed: clear what is left (acknowledged critical errors)
        log_.Clear();
        return clarity::core::NavIntent::ClearErrors;
    }

    view_.Show(log_, NextToView());
    return clarity::core::NavIntent::None;
}

clarity::core::NavIntent ErrorPanel::HandleLongPress()
{
    log_t("ErrorPanel: Long press - clearing all errors and exiting panel");

    log_.Clear();

    // The screen manager restores: it shows a key or lock override that is still active, otherwise the user panel
    return clarity::core::NavIntent::ClearErrors;
}
