#pragma once

// Project Includes
#include "components/clarity_component.h"
#include "definitions/constants.h"
#include "interfaces/i_action_handler.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_style_manager.h"
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
 * @animation_sequence Initial delay (100ms) → Fade-in (1000ms) → Display (configurable) → Fade-out (1000ms) → Complete
 * (100ms delay) → Next panel
 * @timing_total ~2200ms + configurable display time
 *
 * @transitions_to OemOilPanel (or user-configured default panel)
 * @memory_usage Minimal - single component with timer-based animations
 *
 * @ui_layout:
 * - ClarityComponent centered on screen
 * - Black background with animated opacity
 * - Smooth fade transitions using LVGL timers
 *
 * @context This is the first panel shown on startup. It uses timer-based
 * animations for smooth transitions and automatically proceeds to the main
 * application panel when animation completes.
 */
class SplashPanel : public IPanel
{
  public:
    // ========== Constructors and Destructor ==========
    SplashPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
                IPanelManager *panel_manager, IConfigurationManager *configuration_manager = nullptr);
    ~SplashPanel();

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::SPLASH;
    void Init() override;
    void Load() override;
    void Update() override;
    // IActionService Interface Implementation (inherited through IPanel)
    // Old function pointer methods removed - using direct HandleShortPress/HandleLongPress

    // Public action handlers
    void HandleShortPress() override;
    void HandleLongPress() override;

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

    // ========== Static Methods ==========
    static void FadeInTimerCallback(lv_timer_t *timer);
    static void DisplayTimerCallback(lv_timer_t *timer);
    static void FadeOutTimerCallback(lv_timer_t *timer);

    // ========== Private Data Members ==========
    // Panel specific constants
    static constexpr const int DISPLAY_TIME = 500;
    static constexpr const int DELAY_TIME = 200; // NOTE: the delay time is essential, to give LVGL and the code time
                                                 // to cleanup, else memory becomes corrupted

    // Dependencies
    IGpioProvider *gpio_provider_;
    IDisplayProvider *display_provider_;
    IStyleManager *style_manager_;
    IPanelManager *panel_manager_;
    IConfigurationManager *configuration_manager_ = nullptr;

    // Components - static allocation
    lv_obj_t *screen_ = nullptr;
    ClarityComponent component_;
    bool component_initialized_ = false;
    lv_obj_t *blank_screen_ = nullptr;
    lv_timer_t *timer_ = nullptr; // The one pending phase timer; each callback hands it on or clears it
};