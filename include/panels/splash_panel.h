#pragma once

// Project Includes
#include "components/clarity_component.h"
#include "definitions/constants.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "ui/theme.h"
#include <memory>
/**
 * @class SplashPanel
 * @brief Startup splash screen with animated Clarity branding
 *
 * @details This panel displays the Clarity logo with fade-in/fade-out animations
 * during system startup. It provides a professional bootup experience while
 * the system initializes in the background.
 *
 * @presenter_role Coordinates the ClarityComponent for branding display
 * @animation_sequence Label fade-in → Display (500ms) → Label fade-out → Blank (200ms) → Next panel. Each fade
 * takes (duration - 500ms) / 2, so 500ms at the default 1500ms duration. A short press skips to the next panel
 * (Q-08), a long press opens Config.
 *
 * @transitions_to OemOilPanel (or user-configured default panel)
 * @memory_usage Minimal - single component with timer-based animations
 *
 * @ui_layout:
 * - ClarityComponent centered on screen
 * - Only the label fades, so each frame redraws the label area, not the whole screen
 * - Phases chained with one-shot LVGL timers
 *
 * @context This is the first panel shown on startup. It uses timer-based
 * animations for smooth transitions and automatically proceeds to the main
 * application panel when animation completes.
 */
class SplashPanel : public IPanel
{
  public:
    // ========== Constructors and Destructor ==========
    SplashPanel(IGpioProvider *gpio, IDisplayProvider *display, Theme *theme,
                IConfigurationManager *configuration_manager = nullptr);
    ~SplashPanel() override;

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::SPLASH;
    void Init() override;
    void Load() override;
    void Update() override;
    /// Short press skips the rest of the splash (Q-08); long press opens Config
    clarity::core::NavIntent OnButton(clarity::core::ButtonEvent event) override;

    /// True once the fade-out ends or a short press skips it; the screen manager then loads the target panel
    bool Finished() const override
    {
        return done_;
    }

    // Configuration management
    void RegisterConfiguration();

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

  private:
    // ========== Private Methods ==========
    int GetAnimationTime() const;
    void FadeLabel(lv_opa_t from, lv_opa_t to);

    // ========== Static Methods ==========
    static void FadeInTimerCallback(lv_timer_t *timer);
    static void DisplayTimerCallback(lv_timer_t *timer);
    static void FadeOutTimerCallback(lv_timer_t *timer);

    // ========== Private Data Members ==========
    // Dependencies
    IGpioProvider *gpio_provider_;
    IDisplayProvider *display_provider_;
    Theme *theme_;
    IConfigurationManager *configuration_manager_ = nullptr;

    // Components - static allocation
    lv_obj_t *screen_ = nullptr;
    ClarityComponent component_;
    bool component_initialized_ = false;
    lv_timer_t *timer_ = nullptr; // The one pending phase timer; each callback hands it on or clears it
    bool done_ = false;
};