#include "panels/splash_panel.h"
#include "definitions/constants.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "managers/panel_manager.h"
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

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs a splash panel with required dependencies
 * @param gpio GPIO provider interface for hardware interaction
 * @param display Display provider interface for screen creation
 * @param theme Theme for the shared styles and the palette
 * @param notificationService Notification service for panel lifecycle events
 *
 * Initializes the splash panel with dependency injection pattern. Creates the
 * splash component and sets up notification service with fallback to default.
 * The component is stack-allocated for better memory management.
 */
SplashPanel::SplashPanel(IGpioProvider *gpio, IDisplayProvider *display, Theme *theme, IPanelManager *panel_manager,
                         IConfigurationManager *configuration_manager)
    : gpio_provider_(gpio), display_provider_(display), theme_(theme), panel_manager_(panel_manager),
      configuration_manager_(configuration_manager), component_(theme), component_initialized_(false)
{
    log_v("SplashPanel constructor called");
}

/**
 * @brief Destructor that cleans up LVGL screen resources
 *
 * Deletes the screen, which deletes the label and its running fade animation with it.
 * The component is stack-allocated and automatically destroyed.
 * Ensures no LVGL memory leaks when panel is destroyed.
 */
SplashPanel::~SplashPanel()
{
    log_v("~SplashPanel() destructor called");

    // A pending timer would fire with a dangling panel pointer
    if (timer_)
    {
        lv_timer_del(timer_);
        timer_ = nullptr;
    }

    // The screen is loaded without a screen animation, so LVGL holds no pending load that references it, and
    // the next panel's screen is already active when the splash is destroyed
    if (screen_)
    {
        lv_obj_del(screen_);
        screen_ = nullptr;
    }

    // Components delete no LVGL objects, so destroying them after the screens is safe
}

// ========== Public Interface Methods ==========

/**
 * @brief Initializes the screen the splash component is rendered on
 *
 * Validates display provider availability and reports critical errors if
 * dependencies are missing. Part of the panel lifecycle initialization phase.
 */
void SplashPanel::Init()
{
    log_v("Init() called");

    if (!display_provider_)
    {
        log_e("SplashPanel requires display provider");
        ErrorManager::Instance().ReportCriticalError(panel_names::SPLASH,
                                                     "Cannot initialize - display provider is null");
        return;
    }

    screen_ = display_provider_->CreateScreen();
}

/**
 * @brief Shows the screen and fades the label in
 *
 * Renders the splash component at the center of the screen, loads the screen at once and
 * fades the label in. Creates LVGL timers to manage the animation lifecycle including
 * fade-in, display, and fade-out phases. The animation duration is configurable through
 * preferences.
 *
 * Only the label fades, not the screen (A-05): a screen fade redraws the whole display every
 * frame, which made fade frames so slow that a short press during the fade was not sampled.
 */
void SplashPanel::Load()
{
    log_v("Load() called");
    // Component is now stack-allocated and initialized in constructor
    component_initialized_ = true;

    // Create location parameters for the splash component
    ComponentLocation splash_location(LV_ALIGN_CENTER, 0, 0);

    // Render the component
    component_.Render(screen_, splash_location, display_provider_);

    lv_screen_load(screen_);
    FadeLabel(LV_OPA_TRANSP, LV_OPA_COVER);

    // Schedule the fade-in animation
    timer_ = lv_timer_create(SplashPanel::FadeInTimerCallback, GetAnimationTime(), this);

#ifdef CLARITY_TEST_SPLASH_INTERRUPT
    // Test only (D-02): destroy this panel while its fade-in timer is pending
    lv_timer_t *interrupt = lv_timer_create(
        [](lv_timer_t *t)
        {
            auto *panel = static_cast<SplashPanel *>(lv_timer_get_user_data(t));
            log_t("Test: interrupting splash mid fade-in");
            panel->panel_manager_->CreateAndLoadPanel(panel_names::OIL, true);
        },
        GetAnimationTime() / 2, this);
    lv_timer_set_repeat_count(interrupt, 1);
#endif

    log_t("SplashPanel loaded successfully");
}

/**
 * @brief Updates the panel state (minimal for splash panel)
 *
 * Splash panel doesn't require regular content updates as it's animation-driven.
 * Simply notifies the notification service that the update cycle is complete.
 * The animation timers handle the panel's lifecycle independently.
 */
void SplashPanel::Update()
{
    log_v("Update() called");
    // Splash panel doesn't need regular updates - animation handles its own state
}

// ========== Static Callback Methods ==========

/**
 * @brief Callback function for fade-in animation completion
 * @param fadeInTimer The LVGL timer that has completed
 *
 * Called when the fade-in animation finishes. Schedules the display
 * timer for the next phase. Cleans up the completed fade-in timer.
 */
void SplashPanel::FadeInTimerCallback(lv_timer_t *fade_in_timer)
{
    log_v("fade_in_timer_callback() called");
    // Get the screen pointer that was added to the user data
    auto *panel = static_cast<SplashPanel *>(lv_timer_get_user_data(fade_in_timer));

    if (!panel)
    {
        log_e("SplashPanel::fade_in_timer_callback: Panel is null from timer user data!");
        ErrorManager::Instance().ReportCriticalError(panel_names::SPLASH,
                                                     "Timer callback received null panel - splash fade-in broken");
        lv_timer_del(fade_in_timer);
        return;
    }

    // This callback now owns the firing timer, so the destructor must not delete it
    panel->timer_ = nullptr;

    // Create a timer for the callback
    panel->timer_ =
        lv_timer_create(SplashPanel::DisplayTimerCallback, timing_constants::splash::DISPLAY_TIME_MS, panel);

    // Remove the fade_in_timer after transition
    lv_timer_del(fade_in_timer);
}

/**
 * @brief Callback for display timer completion after splash screen display period
 * @param fadeOutTimer The LVGL timer that has completed
 *
 * Called when the display period expires. Fades the label out and schedules the final fade-out timer.
 */
void SplashPanel::DisplayTimerCallback(lv_timer_t *fade_out_timer)
{
    log_v("display_timer_callback() called");
    // Get the splash panel instance
    auto *panel = static_cast<SplashPanel *>(lv_timer_get_user_data(fade_out_timer));

    if (!panel)
    {
        log_e("SplashPanel::display_timer_callback: Panel is null from timer user data!");
        ErrorManager::Instance().ReportCriticalError(panel_names::SPLASH,
                                                     "Timer callback received null panel - splash display broken");
        lv_timer_del(fade_out_timer);
        return;
    }

    // This callback now owns the firing timer, so the destructor must not delete it
    panel->timer_ = nullptr;

    panel->FadeLabel(LV_OPA_COVER, LV_OPA_TRANSP);

    // Complete after the fade-out plus a short blank pause before the target panel appears
    panel->timer_ = lv_timer_create(SplashPanel::FadeOutTimerCallback,
                                    panel->GetAnimationTime() + timing_constants::splash::DELAY_TIME_MS, panel);

    // Delete the animation_timer
    lv_timer_del(fade_out_timer);
}

/**
 * @brief Callback function for fade-out animation completion
 * @param fadeOutTimer The LVGL timer that has completed
 *
 * Called when the fade-out animation finishes. Notifies the notification
 * service that panel loading is complete, which triggers transition to the
 * target panel. Cleans up the fade-out timer to complete the animation cycle.
 */
void SplashPanel::FadeOutTimerCallback(lv_timer_t *fade_out_timer)
{
    log_v("fade_out_timer_callback() called");
    // Get the splash panel instance
    auto *panel = static_cast<SplashPanel *>(lv_timer_get_user_data(fade_out_timer));

    if (!panel)
    {
        log_e("SplashPanel::fade_out_timer_callback: Panel is null from timer user data!");
        ErrorManager::Instance().ReportCriticalError(panel_names::SPLASH,
                                                     "Timer callback received null panel - splash animation broken");
        lv_timer_del(fade_out_timer);
        return;
    }

    // OnPanelLoadComplete below may destroy the panel; this callback deletes its own timer
    panel->timer_ = nullptr;

    if (!panel->panel_manager_)
    {
        log_e("SplashPanel::fade_out_timer_callback: PanelManager is null!");
        ErrorManager::Instance().ReportCriticalError(panel_names::SPLASH,
                                                     "PanelManager is null - cannot complete splash transition");
        lv_timer_del(fade_out_timer);
        return;
    }

    static_cast<PanelManager *>(panel->panel_manager_)->OnPanelLoadComplete(panel);

    // Remove the fade_out_timer after transition, this replaces having to set a repeat on the animation_timer
    lv_timer_del(fade_out_timer);
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
 * @brief Instance method for backward compatibility during migration
 *
 * This method maintains backward compatibility during migration.
 * New code path uses static RegisterConfigSchema instead.
 * Can be removed once migration is complete.
 */
void SplashPanel::RegisterConfiguration()
{
    // During migration, just delegate to static method
    RegisterConfigSchema(configuration_manager_);
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
    if (configuration_manager_)
    {
        auto duration_value = configuration_manager_->QueryConfig<std::string>(CONFIG_DURATION);
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
    lv_obj_t *label = component_.GetLabel();
    if (!label)
    {
        return;
    }

    lv_obj_set_style_opa(label, from, 0);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, label);
    lv_anim_set_values(&anim, from, to);
    lv_anim_set_duration(&anim, static_cast<uint32_t>(GetAnimationTime()));
    lv_anim_set_exec_cb(&anim, [](void *obj, int32_t value)
                        { lv_obj_set_style_opa(static_cast<lv_obj_t *>(obj), static_cast<lv_opa_t>(value), 0); });
    lv_anim_start(&anim);
}

// ========== IActionService Interface Implementation ==========

// ========== Action Handler Methods ==========

/**
 * @brief Handles short button press by skipping the rest of the splash (Q-08)
 *
 * Completes the splash as the fade-out timer would, so the target panel loads on the next
 * ApplyPendingTransition(). The pending timer and any running screen animation are cleaned up when
 * the splash is destroyed.
 */
void SplashPanel::HandleShortPress()
{
    if (!panel_manager_)
    {
        log_w("SplashPanel short press - no panel manager, cannot skip");
        return;
    }

    log_t("SplashPanel short press - skipping to target panel");
    static_cast<PanelManager *>(panel_manager_)->OnPanelLoadComplete(this);
}
/**
 * @brief Handles long press action by loading configuration panel
 *
 * Provides immediate access to the configuration panel during splash screen
 * animation. Marks the panel load as trigger-driven to ensure proper restoration
 * behavior. Reports warning if panel service is not available.
 */
void SplashPanel::HandleLongPress()
{
    if (panel_manager_)
    {
        log_i("SplashPanel long press - loading config panel");
        panel_manager_->CreateAndLoadPanel(panel_names::CONFIG, true);
    }
    else
    {
        log_w("SplashPanel: Cannot load config panel - panelManager not available");
    }
}