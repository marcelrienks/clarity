#pragma once

// Project Includes
#include "components/clarity_component.h"
#include "interfaces/i_action_handler.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_style_manager.h"
#include "definitions/constants.h"
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
    SplashPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *styleManager,
                IPanelManager* panelManager, IConfigurationManager* configurationManager = nullptr);
    ~SplashPanel();

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = PanelNames::SPLASH;
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
    static void RegisterConfigSchema(IConfigurationManager* configurationManager);

    // ========== Configuration Constants ==========
    // Note: show_splash is managed by system settings in main.cpp
    static constexpr const char* CONFIG_SECTION = ConfigConstants::Sections::SPLASH_PANEL;
    static constexpr const char* CONFIG_DURATION = ConfigConstants::Keys::SPLASH_PANEL_DURATION;

    // ========== Configuration Items ==========
    inline static Config::ConfigItem durationConfig_{ConfigConstants::Items::DURATION, TimingConstants::Splash::DURATION_LABEL,
                                                      std::string(TimingConstants::Splash::DEFAULT_DURATION),
                                                      Config::ConfigMetadata(TimingConstants::Splash::DURATION_OPTIONS,
                                                                            TimingConstants::Splash::DURATION_UNIT,
                                                                            Config::ConfigItemType::Selection)};

  private:
    // ========== Private Methods ==========
    int GetAnimationTime() const;

    // ========== Static Methods ==========
    static void fade_in_timer_callback(lv_timer_t *timer);
    static void display_timer_callback(lv_timer_t *timer);
    static void fade_out_timer_callback(lv_timer_t *timer);

    // ========== Private Data Members ==========
    // Panel specific constants
    static constexpr const int _DISPLAY_TIME = 500;
    static constexpr const int _DELAY_TIME = 200; //NOTE: the delay time is essential, to give LVGL and the code time to cleanup, else memory becomes corrupted

    // Dependencies
    IGpioProvider *gpioProvider_;
    IDisplayProvider *displayProvider_;
    IStyleManager *styleManager_;
    IPanelManager *panelManager_;
    IConfigurationManager *configurationManager_ = nullptr;

    // Components - static allocation
    lv_obj_t* screen_ = nullptr;
    ClarityComponent component_;
    bool componentInitialized_ = false;
    lv_obj_t *blankScreen_ = nullptr;
    lv_timer_t *timer_ = nullptr; // The one pending phase timer; each callback hands it on or clears it

};