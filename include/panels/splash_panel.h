#pragma once

#include <cstdint>

#include "clarity/core/types.h"
#include "definitions/constants.h"
#include "interfaces/i_configuration_manager.h"
#include "ui/panel.h"

/**
 * @class SplashPanel
 * @brief Startup splash screen with animated Clarity branding
 *
 * @details A long-lived Panel (A-05) driven by Update(now), with no lv_timer (rule 4). The phases are
 * FadeIn → Hold (500 ms) → FadeOut → blank pause (200 ms) → Done. Each fade takes (duration - 500 ms) / 2, so
 * 500 ms at the default 1500 ms duration. Only the label fades, so each frame redraws the label area, not the
 * whole screen. A short press skips to Done (Q-08), a long press opens Config. The screen manager loads the
 * target panel once Finished() is true.
 *
 * @context This is the first panel shown on startup.
 */
class SplashPanel : public Panel
{
  public:
    explicit SplashPanel(IConfigurationManager &configuration);

    static constexpr const char *NAME = panel_names::SPLASH;

    /// Advances the phase when its time has passed. The first call after Enter() starts the clock
    void Update(uint32_t now) override;

    /// Short press skips the rest of the splash (Q-08); long press opens Config
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

    /// True once the blank pause ends or a short press skips it; the screen manager then loads the target panel
    bool Finished() const override
    {
        return phase_ == Phase::Done;
    }

    // Static schema registration for self-registering pattern
    static void RegisterConfigSchema(IConfigurationManager *configuration_manager);

    // ========== Configuration Constants ==========
    // Note: show_splash is managed by system settings in main.cpp
    static constexpr const char *CONFIG_SECTION = config_constants::sections::SPLASH_PANEL;
    static constexpr const char *CONFIG_DURATION = config_constants::keys::SPLASH_PANEL_DURATION;

    // ========== Configuration Items ==========
    inline static config::ConfigItem duration_config{
        config_constants::items::DURATION, timing_constants::splash::DURATION_LABEL,
        std::string(timing_constants::splash::DEFAULT_DURATION),
        config::ConfigMetadata(timing_constants::splash::DURATION_OPTIONS, timing_constants::splash::DURATION_UNIT,
                               config::ConfigItemType::Selection)};

  protected:
    void Build(lv_obj_t *screen, Theme &theme) override;
    void OnExit() override;

  private:
    enum class Phase : uint8_t
    {
        FadeIn,
        Hold,
        FadeOut, ///< Includes the blank pause that follows the fade
        Done,
    };

    int GetAnimationTime() const;
    void FadeLabel(lv_opa_t from, lv_opa_t to);

    IConfigurationManager &configuration_;

    lv_obj_t *label_ = nullptr; ///< Owned by screen_
    Phase phase_ = Phase::FadeIn;
    int fade_ms_ = 0; ///< Read from the configuration in Build()
    uint32_t phase_start_ms_ = 0;
    bool clock_started_ = false;
#ifdef CLARITY_TEST_SPLASH_INTERRUPT
    bool interrupted_ = false;
#endif
};
