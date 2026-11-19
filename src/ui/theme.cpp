#include "ui/theme.h"

#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"

namespace
{
using clarity::core::ThemeId;

// Day: dark grey background, light grey accents
constexpr Palette DAY_PALETTE{
    .background = ColorHex(0x121212),
    .text = ColorHex(0xEEEEEE),
    .gaugeNormal = ColorHex(0xEEEEEE),
    .gaugeDanger = ColorHex(0xB00020),
    .gaugeTicks = ColorHex(0xF0F0E8),
    .needleNormal = ColorHex(0xFFFFFF),
    .needleDanger = ColorHex(0xDC143C),
    .needleHighlight = ColorHex(0xFFFFFF),
    .pivotCenter = ColorHex(0x505050),
    .pivotEdge = ColorHex(0x2A2A2A),
    .pivotBorder = ColorHex(0x1A1A1A),
    .pivotShadow = ColorHex(0x000000),
    .pivotHighlight = ColorHex(0x707070),
    .pivotVisible = true,
    .keyPresent = ColorHex(0x006400),
    .keyNotPresent = ColorHex(0xDC143C),
    .lockEngaged = ColorHex(0xDC143C),
    .menuBackground = ColorHex(0x121212), // The screen background
    .menuTitle = ColorHex(0xCCCCCC),
    .menuHint = ColorHex(0x888888),
    .menuItem = ColorHex(0xEEEEEE),
    .menuSelectedItem = ColorHex(0xFFFFFF),
    .menuSelectedBackground = ColorHex(0x555555),
    .menuSelectedBorder = ColorHex(0x888888),
    .errorCritical = ColorHex(0xFF0000),
    .errorError = ColorHex(0xFFFF00),
    .errorWarning = ColorHex(0xFFFFFF),
};

// Night: black background with red accents to match the dash lighting. The danger colour is the normal one,
// so the gauge icons stay red, and the pivot blends into the background
constexpr Palette NIGHT_PALETTE{
    .background = ColorHex(0x000000),
    .text = ColorHex(0xB00020),
    .gaugeNormal = ColorHex(0xB00020),
    .gaugeDanger = ColorHex(0xB00020),
    .gaugeTicks = ColorHex(0xB00020),
    .needleNormal = ColorHex(0xFFFFFF),
    .needleDanger = ColorHex(0xDC143C),
    .needleHighlight = ColorHex(0xFFFFFF),
    .pivotCenter = ColorHex(0x505050),
    .pivotEdge = ColorHex(0x2A2A2A),
    .pivotBorder = ColorHex(0x1A1A1A),
    .pivotShadow = ColorHex(0x000000),
    .pivotHighlight = ColorHex(0x707070),
    .pivotVisible = false,
    .keyPresent = ColorHex(0x006400),
    .keyNotPresent = ColorHex(0xDC143C),
    .lockEngaged = ColorHex(0xDC143C),
    .menuBackground = ColorHex(0x1A0000),
    .menuTitle = ColorHex(0xFF6666),
    .menuHint = ColorHex(0x993333),
    .menuItem = ColorHex(0xB00020),
    .menuSelectedItem = ColorHex(0xFF0000),
    .menuSelectedBackground = ColorHex(0x4D1F1F),
    .menuSelectedBorder = ColorHex(0x993333),
    .errorCritical = ColorHex(0xFF0000),
    .errorError = ColorHex(0xFFFF00),
    .errorWarning = ColorHex(0xFFFFFF),
};

// Error: high contrast while the error panel is shown. The gauge, key and menu roles are never on screen with
// it; they keep the Day values
constexpr Palette ERROR_PALETTE{
    .background = ColorHex(0x000000),
    .text = ColorHex(0xFFFFFF),
    .gaugeNormal = ColorHex(0xFFFFFF),
    .gaugeDanger = ColorHex(0xFF0000),
    .gaugeTicks = ColorHex(0xFFFFFF),
    .needleNormal = ColorHex(0xFFFFFF),
    .needleDanger = ColorHex(0xFF0000),
    .needleHighlight = ColorHex(0xFFFFFF),
    .pivotCenter = ColorHex(0x505050),
    .pivotEdge = ColorHex(0x2A2A2A),
    .pivotBorder = ColorHex(0x1A1A1A),
    .pivotShadow = ColorHex(0x000000),
    .pivotHighlight = ColorHex(0x707070),
    .pivotVisible = true,
    .keyPresent = ColorHex(0xFFFFFF),
    .keyNotPresent = ColorHex(0xFFFFFF),
    .lockEngaged = ColorHex(0xFF0000),
    .menuBackground = ColorHex(0x000000), // The screen background
    .menuTitle = ColorHex(0xCCCCCC),
    .menuHint = ColorHex(0x888888),
    .menuItem = ColorHex(0xEEEEEE),
    .menuSelectedItem = ColorHex(0xFFFFFF),
    .menuSelectedBackground = ColorHex(0x555555),
    .menuSelectedBorder = ColorHex(0x888888),
    .errorCritical = ColorHex(0xFF0000),
    .errorError = ColorHex(0xFFFF00),
    .errorWarning = ColorHex(0xFFFFFF),
};

// The Display section: brightness, stored but not applied yet (D-05a)
config::ConfigItem brightness_config{
    config_constants::items::BRIGHTNESS, ui_strings::config_labels::BRIGHTNESS,
    config_constants::defaults::DEFAULT_BRIGHTNESS,
    config::ConfigMetadata(config_constants::options::BRIGHTNESS_LEVELS, "%", config::ConfigItemType::Selection)};

// Self-registration at program startup (the Display section was registered by the old StyleManager)
bool theme_config_registered = []()
{
    ConfigurationManager::AddSchema(Theme::RegisterConfigSchema);
    return true;
}();
} // namespace

const Palette &PaletteFor(ThemeId id)
{
    switch (id)
    {
        case ThemeId::Night:
            return NIGHT_PALETTE;
        case ThemeId::Error:
            return ERROR_PALETTE;
        case ThemeId::Day:
            break;
    }
    return DAY_PALETTE;
}

Theme::~Theme()
{
    if (!initialized_)
    {
        return;
    }
    lv_style_reset(&background_style_);
    lv_style_reset(&text_style_);
    lv_style_reset(&gauge_main_style_);
    lv_style_reset(&gauge_indicator_style_);
    lv_style_reset(&gauge_items_style_);
    lv_style_reset(&gauge_danger_section_style_);
}

void Theme::Init()
{
    if (initialized_)
    {
        return;
    }

    lv_style_init(&background_style_);
    lv_style_init(&text_style_);
    lv_style_init(&gauge_main_style_);
    lv_style_init(&gauge_indicator_style_);
    lv_style_init(&gauge_items_style_);
    lv_style_init(&gauge_danger_section_style_);

    // Theme-independent properties
    lv_style_set_bg_opa(&background_style_, LV_OPA_COVER);
    lv_style_set_text_opa(&text_style_, LV_OPA_COVER);
    lv_style_set_arc_width(&gauge_main_style_, 0);
    lv_style_set_length(&gauge_indicator_style_, 25);
    lv_style_set_line_width(&gauge_indicator_style_, 7);
    lv_style_set_length(&gauge_items_style_, 18);
    lv_style_set_line_width(&gauge_items_style_, 2);
    lv_style_set_line_width(&gauge_danger_section_style_, 5);

    initialized_ = true;
    active_ = ThemeId::Day;
    log_t("SetTheme() called");
    SetStyles(DAY_PALETTE);
    log_t("StyleManager: Theme applied: %s", clarity::core::ToString(active_));
}

bool Theme::Apply(ThemeId id)
{
    if (!initialized_ || id == active_)
    {
        return false;
    }

    // The [T] strings are the ones StyleManager::SetTheme() printed (the scenario waits for them)
    log_t("SetTheme() called");
    active_ = id;
    SetStyles(PaletteFor(id));
    log_t("StyleManager: Theme applied: %s", clarity::core::ToString(id));

    // Refresh every object that uses a shared style, on every screen
    lv_obj_report_style_change(nullptr);
    return true;
}

void Theme::SetStyles(const Palette &palette)
{
    lv_style_set_bg_color(&background_style_, palette.background);
    lv_style_set_text_color(&text_style_, palette.text);
    lv_style_set_line_color(&gauge_indicator_style_, palette.gaugeTicks);
    lv_style_set_line_color(&gauge_items_style_, palette.gaugeTicks);
    lv_style_set_line_color(&gauge_danger_section_style_, palette.gaugeDanger);
}

void Theme::RegisterConfigSchema(IConfigurationManager *configuration_manager)
{
    if (!configuration_manager)
    {
        log_e("Theme::RegisterConfigSchema: ConfigurationManager is null - display config registration failed!");
        ErrorManager::Instance().ReportCriticalError("Theme", "ConfigManager null - display config failed");
        return;
    }

    if (configuration_manager->IsSchemaRegistered(config_constants::sections::STYLE_MANAGER))
    {
        return;
    }

    config::ConfigSection section(config_constants::sections::STYLE_MANAGER, config_constants::sections::STYLE_MANAGER,
                                  config_constants::section_names::STYLE_MANAGER);
    section.AddItem(brightness_config);

    configuration_manager->RegisterConfigSection(section);
    log_i("Theme configuration schema registered (static)");
}
