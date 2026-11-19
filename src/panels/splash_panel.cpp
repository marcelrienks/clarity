#include "panels/splash_panel.h"
#include "definitions/constants.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "ui/theme.h"
#include "utilities/logging.h"
#include <algorithm>
#include <charconv>
#include <esp32-hal-log.h>

// Self-registration at program startup
static bool splash_panel_registered = []()
{
    ConfigurationManager::AddSchema(SplashPanel::RegisterConfigSchema);
    return true;
}();

SplashPanel::SplashPanel(IConfigurationManager &configuration) : configuration_(configuration)
{
}

void SplashPanel::Build(lv_obj_t *screen, Theme &theme)
{
    phase_ = Phase::FadeIn;
    clock_started_ = false;
    fade_ms_ = GetAnimationTime();
#ifdef CLARITY_TEST_SPLASH_INTERRUPT
    interrupted_ = false;
#endif

    label_ = lv_label_create(screen);
    lv_label_set_text(label_, ui_constants::APP_NAME);
    lv_obj_add_style(label_, &theme.TextStyle(), MAIN_DEFAULT);
    lv_obj_align(label_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(label_, &lv_font_montserrat_20, 0);

    // Only the label fades, not the screen (A-05): a screen fade redraws the whole display every frame, which
    // made fade frames so slow that a short press during the fade was not sampled
    FadeLabel(LV_OPA_TRANSP, LV_OPA_COVER);
}

void SplashPanel::OnExit()
{
    // The label and its animation are deleted with the screen
    label_ = nullptr;
}

void SplashPanel::Update(uint32_t now)
{
    if (!screen_ || phase_ == Phase::Done)
    {
        return;
    }

    if (!clock_started_)
    {
        clock_started_ = true;
        phase_start_ms_ = now;
    }
    const uint32_t elapsed = now - phase_start_ms_; // unsigned: correct across the millis() wrap

#ifdef CLARITY_TEST_SPLASH_INTERRUPT
    // Test only (D-02): finish early, so the screen manager exits this panel while its fade-in is running
    if (!interrupted_ && elapsed >= static_cast<uint32_t>(fade_ms_ / 2) && phase_ == Phase::FadeIn)
    {
        interrupted_ = true;
        log_t("Test: interrupting splash mid fade-in");
        phase_ = Phase::Done;
        return;
    }
#endif

    switch (phase_)
    {
        case Phase::FadeIn:
            if (elapsed >= static_cast<uint32_t>(fade_ms_))
            {
                phase_ = Phase::Hold;
                phase_start_ms_ = now;
            }
            break;
        case Phase::Hold:
            if (elapsed >= timing_constants::splash::DISPLAY_TIME_MS)
            {
                FadeLabel(LV_OPA_COVER, LV_OPA_TRANSP);
                phase_ = Phase::FadeOut;
                phase_start_ms_ = now;
            }
            break;
        case Phase::FadeOut:
            // The fade, then a short blank pause before the target panel appears
            if (elapsed >= static_cast<uint32_t>(fade_ms_) + timing_constants::splash::DELAY_TIME_MS)
            {
                phase_ = Phase::Done;
            }
            break;
        case Phase::Done:
            break;
    }
}

// ========== Configuration Methods ==========

/**
 * @brief Static method to register configuration schema without instance
 * @param configurationManager Service to register schema with
 *
 * Called automatically at program startup through ConfigRegistry.
 * Registers the SplashPanel configuration schema without
 * requiring a panel instance to exist.
 */
void SplashPanel::RegisterConfigSchema(IConfigurationManager *configuration_manager)
{
    if (!configuration_manager)
    {
        log_e("SplashPanel::RegisterConfigSchema: ConfigurationManager is null - splash config registration failed!");
        ErrorManager::Instance().ReportCriticalError(
            panel_names::SPLASH, "ConfigurationManager is null - splash panel configuration cannot be registered");
        return;
    }

    // Check if already registered to prevent duplicates
    if (configuration_manager->IsSchemaRegistered(CONFIG_SECTION))
    {
        return;
    }

    using namespace config;

    ConfigSection section(config_constants::sections::SPLASH_PANEL, CONFIG_SECTION,
                          config_constants::section_names::SPLASH_PANEL);

    // Splash duration selection (show_splash is managed by system settings)
    section.AddItem(duration_config);

    configuration_manager->RegisterConfigSection(section);
    log_i("SplashPanel configuration schema registered (static)");
}

/**
 * @brief Calculates animation time based on configurable splash duration
 * @return Animation duration in milliseconds for fade-in and fade-out
 *
 * Reads the splash duration from preferences and calculates the time for
 * fade-in and fade-out animations. The display time is subtracted and the
 * remaining time is split equally between fade-in and fade-out phases.
 */
int SplashPanel::GetAnimationTime() const
{
    log_v("GetAnimationTime() called");

    // Get splash duration using type-safe config system
    // std::stoi would throw (abort) on a corrupt stored value, so parse without exceptions
    const auto parse_duration = [](const std::string &text, int &out)
    {
        int value = 0;
        const char *end = text.data() + text.size();
        auto result = std::from_chars(text.data(), end, value);
        if (result.ec != std::errc() || result.ptr != end)
        {
            return false;
        }
        out = value;
        return true;
    };

    int splash_duration = 0;
    parse_duration(timing_constants::splash::DEFAULT_DURATION, splash_duration); // Default value
    auto duration_value = configuration_.QueryConfig<std::string>(CONFIG_DURATION);
#ifdef CLARITY_TEST_SEED_INVALID_SPLASH
    // Test only (D-23): storage validation rejects a non-numeric duration before it gets here, so hand the
    // parser one directly to exercise its fallback
    duration_value = std::string("abc");
#endif
    if (duration_value)
    {
        int parsed = 0;
        if (parse_duration(*duration_value, parsed))
        {
            splash_duration = parsed;
        }
        else
        {
            log_w("Invalid splash duration '%s', using default", duration_value->c_str());
            log_t("Invalid splash duration '%s', using default", duration_value->c_str());
        }
    }

    int anim_time = (splash_duration - static_cast<int>(timing_constants::splash::DISPLAY_TIME_MS)) / 2;

    return std::max(anim_time, 0);
}

/**
 * @brief Animates the label opacity over the animation time
 * @param from Opacity at the start
 * @param to Opacity at the end
 *
 * Only the label area is redrawn each frame. The animation is deleted with the label when the screen is deleted.
 */
void SplashPanel::FadeLabel(lv_opa_t from, lv_opa_t to)
{
    if (!label_)
    {
        return;
    }

    lv_obj_set_style_opa(label_, from, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label_);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_duration(&anim, static_cast<uint32_t>(fade_ms_));
    lv_anim_set_exec_cb(&anim, [](void *obj, int32_t value)
                        { lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(value), 0); });
    lv_anim_start(&anim);
}

/**
 * @brief Handles button presses: a short press skips the rest of the splash (Q-08), a long press opens Config
 *
 * The skip completes the splash as the end of the blank pause would, so the screen manager loads the target
 * panel on its next Apply(). The label's animation is deleted with the screen.
 */
clarity::core::NavIntent SplashPanel::OnButton(clarity::core::ButtonEvent event)
{
    if (event == clarity::core::ButtonEvent::Short)
    {
        log_t("SplashPanel short press - skipping to target panel");
        phase_ = Phase::Done;
        return clarity::core::NavIntent::None;
    }
    if (event == clarity::core::ButtonEvent::Long)
    {
        log_i("SplashPanel long press - loading config panel");
        return clarity::core::NavIntent::OpenConfig;
    }
    return clarity::core::NavIntent::None;
}
