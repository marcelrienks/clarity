#include "panels/oem_oil_panel.h"
#include "components/oem/oem_oil_pressure_component.h"
#include "components/oem/oem_oil_temperature_component.h"
#include "definitions/constants.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "managers/panel_manager.h"
#include "managers/style_manager.h"
#include "sensors/oil_pressure_sensor.h"
#include "sensors/oil_temperature_sensor.h"
#include "utilities/logging.h"
#include "utilities/unit_converter.h"

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs OEM oil monitoring panel with dual gauge display
 * @param gpio GPIO provider for sensor hardware access
 * @param display Display provider for screen management
 * @param styleManager Style service for theme-based styling
 *
 * Creates automotive oil monitoring panel with pressure and temperature gauges.
 * Initializes stack-allocated components, creates sensor instances, and sets up
 * LVGL animation structures for smooth needle movements.
 */
OemOilPanel::OemOilPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
                         IPanelManager *panel_manager, IConfigurationManager *configuration_manager)
    : gpio_provider_(gpio), display_provider_(display), style_manager_(style_manager), panel_manager_(panel_manager),
      configuration_manager_(configuration_manager), oem_oil_pressure_component_(style_manager),
      oem_oil_temperature_component_(style_manager),
      oem_oil_pressure_sensor_(std::make_shared<OilPressureSensor>(gpio)),
      oem_oil_temperature_sensor_(std::make_shared<OilTemperatureSensor>(gpio)), components_initialized_(false),
      current_oil_pressure_value_(-1), current_oil_temperature_value_(-1), pressure_deadband_(0),
      temperature_deadband_(0), last_animated_pressure_value_(-1), last_animated_temperature_value_(-1), last_theme_("")
{
    log_v("OemOilPanel constructor called");
    // Initialize LVGL animation structures to prevent undefined behavior
    lv_anim_init(&pressure_animation_);
    lv_anim_init(&temperature_animation_);

    // Apply sensor configuration from preferences now that we have configurationManager
    ApplyCurrentSensorSettings();
}

/**
 * @brief Destructor cleans up panel resources and stops animations
 *
 * Safely stops any running LVGL animations, deletes animation objects,
 * cleans up LVGL screen, and stops sensor readings. Ensures proper cleanup
 * of automotive monitoring resources and prevents animation memory leaks.
 */
OemOilPanel::~OemOilPanel()
{
    log_v("~OemOilPanel() destructor called");

    // Stop any running animations before destroying the panel
    if (animation_state_ != AnimationState::IDLE)
    {
        lv_anim_delete(&pressure_animation_, nullptr);
        lv_anim_delete(&temperature_animation_, nullptr);
        animation_state_ = AnimationState::IDLE;
    }

    // Delete all animations that might reference this instance
    lv_anim_delete(this, nullptr);

    if (screen_)
    {
        lv_obj_delete(screen_);
        screen_ = nullptr;
    }

    // Components delete no LVGL objects, so destroying them after the screen is safe

    if (oem_oil_pressure_sensor_)
    {
        oem_oil_pressure_sensor_.reset();
    }

    if (oem_oil_temperature_sensor_)
    {
        oem_oil_temperature_sensor_.reset();
    }
}

// ========== Public Interface Methods ==========

/**
 * @brief Initializes OEM oil panel UI structure and sensor configuration
 *
 * Creates LVGL screen, validates providers, configures oil pressure and
 * temperature sensors, and sets up component positioning. Establishes
 * sensor reading intervals appropriate for automotive monitoring.
 */
void OemOilPanel::Init()
{
    log_v("Init() called");

    if (!display_provider_)
    {
        log_e("OemOilPanel requires display provider");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel", "Missing required display provider");
        return;
    }

    if (!oem_oil_pressure_sensor_ || !oem_oil_temperature_sensor_)
    {
        log_e("OemOilPanel requires pressure and temperature sensors");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel", "Missing required sensors");
        return;
    }

    screen_ = display_provider_->CreateScreen();

    // Apply current theme immediately after screen creation
    if (style_manager_)
    {
        style_manager_->ApplyThemeToScreen(screen_);
    }

    oem_oil_pressure_sensor_->Init();
    current_oil_pressure_value_ = -1;   // Sentinel value to ensure first update
    last_animated_pressure_value_ = -1; // Reset hysteresis tracking

    oem_oil_temperature_sensor_->Init();
    current_oil_temperature_value_ = -1;   // Sentinel value to ensure first update
    last_animated_temperature_value_ = -1; // Reset hysteresis tracking

    // Calculate deadbands from configuration percentiles
    CalculateDeadbands();

    // Cache sensor references to avoid repeated static_pointer_cast operations
    cached_pressure_sensor_ = std::static_pointer_cast<OilPressureSensor>(oem_oil_pressure_sensor_);
    cached_temperature_sensor_ = std::static_pointer_cast<OilTemperatureSensor>(oem_oil_temperature_sensor_);

    log_i("OemOilPanel initialization completed");
}

/**
 * @brief Loads OEM oil panel UI components and starts sensor monitoring
 *
 * Renders oil pressure and temperature components, performs initial sensor
 * readings, starts sensor monitoring, and loads LVGL screen. Critical for
 * establishing real-time automotive oil system monitoring.
 */
void OemOilPanel::Load()
{
    log_v("Load() called");

    // Create components for both pressure and temperature
    if (!style_manager_ || !style_manager_->IsInitialized())
    {
        log_w("StyleService not properly initialized, skipping component creation");
        return;
    }

    // Components are now stack-allocated and initialized in constructor
    // Mark components as initialized for initialization checks
    components_initialized_ = true;

    // Create location parameters with rotational start points for scales
    ComponentLocation pressure_location(210);   // rotation starting at 210 degrees
    ComponentLocation temperature_location(30); // rotation starting at 30 degrees

    // Render both components
    oem_oil_pressure_component_.Render(screen_, pressure_location, display_provider_);
    oem_oil_temperature_component_.Render(screen_, temperature_location, display_provider_);

    // Initialize needle positions to match current values (prevents animation jumps)
    oem_oil_pressure_component_.SetValue(current_oil_pressure_value_);
    oem_oil_temperature_component_.SetValue(current_oil_temperature_value_);

    // Initialize needle colors based on initial values
    oem_oil_pressure_component_.Refresh(Reading{current_oil_pressure_value_});
    oem_oil_temperature_component_.Refresh(Reading{current_oil_temperature_value_});

    // Log heap status before critical operation

    // Validate screen object pointer and basic properties
    if (!screen_)
    {
        log_e("  screen_ is NULL!");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel", "screen_ is NULL during periodic check");
    }

    // Validate component pointers

    // Check if 'this' pointer is still valid

    lv_obj_add_event_cb(screen_, OemOilPanel::ShowPanelCompletionCallback, LV_EVENT_SCREEN_LOADED, this);

    // Final validation before the critical call

    lv_screen_load(screen_);

    // Post-load validation

    if (style_manager_)
    {
        style_manager_->ApplyThemeToScreen(screen_);
        // Update last_theme_ to current theme to sync with theme detection in update()
        last_theme_ = String(style_manager_->GetCurrentTheme().c_str());
    }

    log_t("OemOilPanel loaded successfully");
}

/**
 * @brief Updates OEM oil panel with current sensor readings and animations
 *
 * Monitors for theme changes, applies sensor settings periodically, and updates
 * both oil pressure and temperature readings. Manages LVGL animations for smooth
 * needle movements and coordinates UI state with panel service.
 */
void OemOilPanel::Update()
{
    // Check for theme changes and apply immediately
    const char *current_theme = style_manager_ ? style_manager_->GetCurrentTheme().c_str() : "";
    bool theme_changed = last_theme_.isEmpty() || !last_theme_.equals(current_theme);
    if (theme_changed)
    {
        last_theme_ = String(current_theme);
        if (style_manager_ && screen_)
        {
            style_manager_->ApplyThemeToScreen(screen_);
        }
    }

    // Apply sensor settings periodically (every ~100 updates) instead of every cycle
    // This reduces overhead while still catching preference changes reasonably quickly
    static uint16_t settings_update_counter = 0;
    if (++settings_update_counter % 100 == 0)
    {
        ApplyCurrentSensorSettings();
    }

    // Update sensor readings and components (pass theme change flag)
    UpdateOilPressure(theme_changed);
    UpdateOilTemperature(theme_changed);

    // If no animations were started, updates complete immediately
    if (animation_state_ == AnimationState::IDLE)
    {
        if (panel_manager_)
        {
            panel_manager_->SetUiState(UIState::IDLE);
        }
    }
}

// ========== Private Methods ==========

/**
 * @brief Update the oil pressure reading on the screen
 */
void OemOilPanel::UpdateOilPressure(bool force_refresh)
{
    // Skip if pressure animation is running unless this is a forced refresh
    if (!force_refresh &&
        (animation_state_ == AnimationState::PRESSURE_RUNNING || animation_state_ == AnimationState::BOTH_RUNNING))
    {
        return;
    }

    // Safety check for sensor availability
    if (!oem_oil_pressure_sensor_)
    {
        log_e("Pressure sensor is null - cannot update pressure display!");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel", "Pressure sensor is null - gauge cannot function");
        return;
    }

    // Use delta-based updates for better performance
    auto sensor_value = std::get<int32_t>(oem_oil_pressure_sensor_->GetReading());
    auto value = MapPressureValue(sensor_value);

    // Apply hysteresis/deadband to prevent jittery updates from sensor noise
    // Only animate if change exceeds deadband threshold or this is the first reading
    bool should_animate = false;
    if (last_animated_pressure_value_ == -1)
    {
        // First reading - always animate
        should_animate = true;
    }
    else
    {
        // Check if change exceeds deadband threshold
        int32_t change_magnitude = abs(value - last_animated_pressure_value_);
        should_animate = change_magnitude >= pressure_deadband_;
    }

    // Handle forced refresh (theme changes) or skip if no significant change
    if (!should_animate && !force_refresh)
    {
        // Update current value for tracking but don't animate
        current_oil_pressure_value_ = value;
        return; // No animation needed - change within deadband
    }

    if (force_refresh && value == current_oil_pressure_value_)
    {
        oem_oil_pressure_component_.Refresh(Reading{value});
        return; // Theme refresh but no value change
    }

    log_i("Updating pressure from %d to %d (exceeds deadband of %d) [%.1f to %.1f Bar]", current_oil_pressure_value_,
          value, pressure_deadband_, current_oil_pressure_value_ / 10.0f, value / 10.0f);
    oem_oil_pressure_component_.Refresh(Reading{value});

    // Track this value as the last animated value for hysteresis
    last_animated_pressure_value_ = value;

    // Determine animation start value
    // If current value is -1 (initial state) or outside scale bounds, start from appropriate boundary
    int32_t animation_start_value = current_oil_pressure_value_;
    // PRESSURE SCALE: Component manages scale representing 0.0-6.0 Bar with one decimal place precision
    // Scale unit 1 = 0.1 Bar, so value 55 = 5.5 Bar, value 23 = 2.3 Bar
    // Using same constants that components use to ensure consistency
    const int32_t scale_min = sensor_constants::PRESSURE_DISPLAY_SCALE_MIN; // Pressure scale minimum: 0.0 Bar
    const int32_t scale_max = sensor_constants::PRESSURE_DISPLAY_SCALE_MAX; // Pressure scale maximum: 6.0 Bar

    if (current_oil_pressure_value_ == -1)
    {
        // Initial load - determine start position based on target value
        if (value < scale_min)
        {
            animation_start_value = scale_min; // Start from minimum if value is below scale
        }
        else if (value > scale_max)
        {
            animation_start_value = scale_max; // Start from maximum if value is above scale
        }
        else
        {
            // Value is within scale bounds
            // For pressure, always start from minimum (0.0 Bar) for initial animation
            // This represents the engine starting up and building pressure from 0
            animation_start_value = scale_min;
        }
        log_i("Initial pressure animation: starting from %d to %d", animationStartValue, value);
    }
    else if (current_oil_pressure_value_ < scale_min && value >= scale_min)
    {
        // Coming from below scale minimum into visible range
        animation_start_value = scale_min;
        log_i("Pressure entering scale from below: animating from %d to %d", animationStartValue, value);
    }
    else if (current_oil_pressure_value_ > scale_max && value <= scale_max)
    {
        // Coming from above scale maximum into visible range
        animation_start_value = scale_max;
        log_i("Pressure entering scale from above: animating from %d to %d", animationStartValue, value);
    }

    // Setup animation
    lv_anim_init(&pressure_animation_);
    lv_anim_set_duration(&pressure_animation_, ANIMATION_DURATION);
    lv_anim_set_repeat_count(&pressure_animation_, 0);
    lv_anim_set_playback_duration(&pressure_animation_, 0);
    lv_anim_set_values(&pressure_animation_, animation_start_value, value);
    lv_anim_set_var(&pressure_animation_, this);
    lv_anim_set_user_data(&pressure_animation_, (void *)static_cast<uintptr_t>(OilSensorTypes::Pressure));
    lv_anim_set_exec_cb(&pressure_animation_, OemOilPanel::ExecutePressureAnimationCallback);
    lv_anim_set_completed_cb(&pressure_animation_, OemOilPanel::UpdatePanelCompletionCallback);

    // Update animation state based on current state
    if (animation_state_ == AnimationState::TEMPERATURE_RUNNING)
    {
        animation_state_ = AnimationState::BOTH_RUNNING;
    }
    else
    {
        animation_state_ = AnimationState::PRESSURE_RUNNING;
    }
    // Set ANIMATING state before starting animation
    if (panel_manager_)
    {
        panel_manager_->SetUiState(UIState::BUSY);
    }
    // Start pressure gauge animation
    lv_anim_start(&pressure_animation_);
}

/**
 * @brief Update the oil temperature reading on the screen
 */
void OemOilPanel::UpdateOilTemperature(bool force_refresh)
{

    // Skip update if temperature animation is already running
    if (animation_state_ == AnimationState::TEMPERATURE_RUNNING || animation_state_ == AnimationState::BOTH_RUNNING)
    {
        // Validate that the LVGL animation is actually still running
        // If not, this is a stale flag that needs to be cleared
        if (!lv_anim_get(&temperature_animation_, nullptr))
        {
            log_w("UpdateOilTemperature: Found stale animation flag, clearing it");
            if (animation_state_ == AnimationState::TEMPERATURE_RUNNING)
            {
                animation_state_ = AnimationState::IDLE;
            }
            else if (animation_state_ == AnimationState::BOTH_RUNNING)
            {
                animation_state_ = AnimationState::PRESSURE_RUNNING;
            }
        }
        else
        {
            return;
        }
    }

    // Safety check for sensor availability
    if (!oem_oil_temperature_sensor_)
    {
        log_e("Temperature sensor is null - cannot update temperature display!");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel",
                                                     "Temperature sensor is null - gauge cannot function");
        return;
    }

    // Use delta-based updates for better performance
    auto sensor_value = std::get<int32_t>(oem_oil_temperature_sensor_->GetReading());
    auto value = MapTemperatureValue(sensor_value);

    // Apply hysteresis/deadband to prevent jittery updates from sensor noise
    // Only animate if change exceeds deadband threshold or this is the first reading
    bool should_animate = false;
    if (last_animated_temperature_value_ == -1)
    {
        // First reading - always animate
        should_animate = true;
    }
    else
    {
        // Check if change exceeds deadband threshold
        int32_t change_magnitude = abs(value - last_animated_temperature_value_);
        should_animate = change_magnitude >= temperature_deadband_;
    }

    // Handle forced refresh (theme changes) or skip if no significant change
    if (!should_animate && !force_refresh)
    {
        // Update current value for tracking but don't animate
        current_oil_temperature_value_ = value;
        return; // No animation needed - change within deadband
    }

    if (force_refresh && value == current_oil_temperature_value_)
    {
        oem_oil_temperature_component_.Refresh(Reading{value});
        return; // Theme refresh but no value change
    }

    log_i("Updating temperature from %d to %d (exceeds deadband of %d°C)", current_oil_temperature_value_, value,
          temperature_deadband_);
    oem_oil_temperature_component_.Refresh(Reading{value});

    // Track this value as the last animated value for hysteresis
    last_animated_temperature_value_ = value;

    // Determine animation start value
    // If current value is -1 (initial state) or outside scale bounds, start from appropriate boundary
    int32_t animation_start_value = current_oil_temperature_value_;
    // TEMPERATURE SCALE: Component manages scale representing 0-120°C
    // Using same constants that components use to ensure consistency
    const int32_t scale_min = sensor_constants::TEMPERATURE_DISPLAY_SCALE_MIN; // Temperature scale minimum
    const int32_t scale_max = sensor_constants::TEMPERATURE_DISPLAY_SCALE_MAX; // Temperature scale maximum

    if (current_oil_temperature_value_ == -1)
    {
        // Initial load - determine start position based on target value
        if (value < scale_min)
        {
            animation_start_value = scale_min; // Start from minimum if value is below scale
        }
        else if (value > scale_max)
        {
            animation_start_value = scale_max; // Start from maximum if value is above scale
        }
        else
        {
            // Value is within scale bounds
            // For temperature, always start from minimum (cold) for initial animation
            // This represents a cold engine warming up, which is the typical scenario
            animation_start_value = scale_min;
        }
        log_i("Initial temperature animation: starting from %d to %d", animationStartValue, value);
    }
    else if (current_oil_temperature_value_ < scale_min && value >= scale_min)
    {
        // Coming from below scale minimum into visible range
        animation_start_value = scale_min;
        log_i("Temperature entering scale from below: animating from %d to %d", animationStartValue, value);
    }
    else if (current_oil_temperature_value_ > scale_max && value <= scale_max)
    {
        // Coming from above scale maximum into visible range
        animation_start_value = scale_max;
        log_i("Temperature entering scale from above: animating from %d to %d", animationStartValue, value);
    }

    // Setup animation
    lv_anim_init(&temperature_animation_);
    lv_anim_set_duration(&temperature_animation_, ANIMATION_DURATION);
    lv_anim_set_repeat_count(&temperature_animation_, 0);
    lv_anim_set_playback_duration(&temperature_animation_, 0);
    lv_anim_set_values(&temperature_animation_, animation_start_value, value);
    lv_anim_set_var(&temperature_animation_, this);
    lv_anim_set_user_data(&temperature_animation_, (void *)static_cast<uintptr_t>(OilSensorTypes::Temperature));
    lv_anim_set_exec_cb(&temperature_animation_, OemOilPanel::ExecuteTemperatureAnimationCallback);
    lv_anim_set_completed_cb(&temperature_animation_, OemOilPanel::UpdatePanelCompletionCallback);

    // Update animation state based on current state
    if (animation_state_ == AnimationState::PRESSURE_RUNNING)
    {
        animation_state_ = AnimationState::BOTH_RUNNING;
    }
    else
    {
        animation_state_ = AnimationState::TEMPERATURE_RUNNING;
    }
    // Set ANIMATING state before starting animation
    if (panel_manager_)
    {
        panel_manager_->SetUiState(UIState::BUSY);
    }
    // Start temperature gauge animation
    lv_anim_start(&temperature_animation_);
}

// Legacy Action interface methods (retained for reference)
/*
/// @brief Get action for short button press
/// @return NoAction - oil panel doesn't respond to short presses
Action OemOilPanel::GetShortPressAction()
{
    log_v("GetShortPressAction() called");

    // Short press: No action for oil panel
    return Action(nullptr);
}

/// @brief Get action for long button press
/// @return Action to switch to CONFIG panel
Action OemOilPanel::GetLongPressAction()
{
    log_v("GetLongPressAction() called");

    // Return an action that directly calls PanelService interface
    if (!panel_manager_)
    {
        log_w("OemOilPanel: PanelService not available, returning no action");
        return Action(nullptr);
    }

    return Action(
        [this]()
        {
            panel_manager_->CreateAndLoadPanel(panel_names::CONFIG, true);
        });
}
*/

// ========== Configuration Methods ==========

/**
 * @brief Apply current sensor settings from preferences directly
 */
void OemOilPanel::ApplyCurrentSensorSettings()
{
    // Apply update_rate and units from preferences to sensors if preference service is available
    if (configuration_manager_ && cached_pressure_sensor_ && cached_temperature_sensor_)
    {
        // Get individual preferences using type-safe config system
        int update_rate = 500; // Default
        if (auto rate_value = configuration_manager_->QueryConfig<int>(config_constants::keys::SYSTEM_UPDATE_RATE))
        {
            update_rate = *rate_value;
        }

        std::string pressure_unit = config_constants::defaults::DEFAULT_PRESSURE_UNIT; // Default
        if (auto unit_value = configuration_manager_->QueryConfig<std::string>(OilPressureSensor::CONFIG_UNIT))
        {
            pressure_unit = *unit_value;
        }

        std::string temp_unit = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT; // Default
        if (auto unit_value = configuration_manager_->QueryConfig<std::string>(OilTemperatureSensor::CONFIG_UNIT))
        {
            temp_unit = *unit_value;
        }

        // Only update settings if they have changed
        bool update_rate_changed = (last_update_rate_ != update_rate);
        bool pressure_unit_changed = !last_pressure_unit_.equals(pressure_unit.c_str());
        bool temp_unit_changed = !last_temp_unit_.equals(temp_unit.c_str());

        if (update_rate_changed)
        {
            cached_pressure_sensor_->SetUpdateRate(update_rate);
            cached_temperature_sensor_->SetUpdateRate(update_rate);
            last_update_rate_ = update_rate;
        }

        if (pressure_unit_changed)
        {
            cached_pressure_sensor_->SetTargetUnit(pressure_unit);
            last_pressure_unit_ = String(pressure_unit.c_str());
        }

        if (temp_unit_changed)
        {
            cached_temperature_sensor_->SetTargetUnit(temp_unit);
            last_temp_unit_ = String(temp_unit.c_str());
        }

        // Only log if something actually changed
        if (update_rate_changed || pressure_unit_changed || temp_unit_changed)
        {
            log_t("Sensor settings updated - Rate: %dms, Pressure: %s, Temp: %s", update_rate, pressure_unit.c_str(),
                  temp_unit.c_str());
        }
    }
    else if (!configuration_manager_)
    {
        // Only log this warning once during initialization
        static bool logged = false;
        if (!logged)
        {
            log_w("ApplyCurrentSensorSettings: No ConfigurationManager available - using default sensor settings");
            logged = true;
        }
    }
}

// ========== Static Callback Methods ==========

/**
 * @brief The callback to be run once show panel has completed
 * @param event LVGL event that was used to call this
 */
void OemOilPanel::ShowPanelCompletionCallback(lv_event_t *event)
{
    log_v("ShowPanelCompletionCallback() called");

    if (!event)
    {
        return;
    }

    auto this_instance = static_cast<OemOilPanel *>(lv_event_get_user_data(event));
    if (!this_instance)
    {
        return;
    }

    // Set UI state to IDLE so main loop can start calling Update()
    if (this_instance->panel_manager_)
    {
        this_instance->panel_manager_->SetUiState(UIState::IDLE);
    }

    // Animation completed - no callback needed for interface-based approach
}

/**
 * @brief Callback when animation has completed. aka update complete
 * @param animation the object that was animated
 */
void OemOilPanel::UpdatePanelCompletionCallback(lv_anim_t *animation)
{

    if (animation == nullptr || animation->var == nullptr)
    {
        log_w("Animation or animation->var is null, skipping completion callback");
        return;
    }

    auto this_instance = static_cast<OemOilPanel *>(animation->var);
    auto sensor_type = static_cast<OilSensorTypes>(reinterpret_cast<uintptr_t>(animation->user_data));

    // Determine which animation has completed and update the corresponding value
    if (sensor_type == OilSensorTypes::Pressure)
    {
        this_instance->current_oil_pressure_value_ = animation->current_value;
        // Update animation state when pressure animation completes
        if (this_instance->animation_state_ == AnimationState::PRESSURE_RUNNING)
        {
            this_instance->animation_state_ = AnimationState::IDLE;
        }
        else if (this_instance->animation_state_ == AnimationState::BOTH_RUNNING)
        {
            this_instance->animation_state_ = AnimationState::TEMPERATURE_RUNNING;
        }
    }
    else if (sensor_type == OilSensorTypes::Temperature)
    {
        this_instance->current_oil_temperature_value_ = animation->current_value;
        // Update animation state when temperature animation completes
        if (this_instance->animation_state_ == AnimationState::TEMPERATURE_RUNNING)
        {
            this_instance->animation_state_ = AnimationState::IDLE;
        }
        else if (this_instance->animation_state_ == AnimationState::BOTH_RUNNING)
        {
            this_instance->animation_state_ = AnimationState::PRESSURE_RUNNING;
        }
    }

    // Only set UI state to idle when all animations are complete
    if (this_instance->animation_state_ == AnimationState::IDLE)
    {
        // Set IDLE state when all animations are complete
        if (this_instance->panel_manager_)
        {
            this_instance->panel_manager_->SetUiState(UIState::IDLE);
        }
    }
}

/**
 * @brief callback used by the animation to set the values smoothly until ultimate value is reached
 * @param target the object being animated
 * @param value the next value in a sequence to create a smooth transition
 */
void OemOilPanel::ExecutePressureAnimationCallback(void *target, int32_t value)
{
    log_v("ExecutePressureAnimationCallback() called with value: %d", value);

    lv_anim_t *animation = lv_anim_get(target, ExecutePressureAnimationCallback); // get the animation
    if (animation == nullptr || animation->var == nullptr)
    {
        log_w("Animation or animation->var is null, skipping callback");
        return;
    }

    auto this_instance =
        static_cast<OemOilPanel *>(animation->var); // use the animation to get the var which is this instance
    // Component is now stack-allocated, always available
    this_instance->oem_oil_pressure_component_.SetValue(value);
}

/**
 * @brief callback used by the animation to set the values smoothly until ultimate value is reached
 * @param target the object being animated
 * @param value the next value in a sequence to create a smooth transition
 */
void OemOilPanel::ExecuteTemperatureAnimationCallback(void *target, int32_t value)
{
    log_v("ExecuteTemperatureAnimationCallback() called with value: %d", value);

    lv_anim_t *animation = lv_anim_get(target, ExecuteTemperatureAnimationCallback); // get the animation
    if (animation == nullptr || animation->var == nullptr)
    {
        log_w("Animation or animation->var is null, skipping callback");
        return;
    }

    auto this_instance =
        static_cast<OemOilPanel *>(animation->var); // use the animation to get the var which is this instance
    // Component is now stack-allocated, always available
    this_instance->oem_oil_temperature_component_.SetValue(value);
}

// ========== Value Mapping Methods ==========

/**
 * @brief Map oil pressure sensor value to display scale
 * @param sensorValue Raw sensor value (1-10 Bar)
 * @return Mapped value for display (0-60, representing pressure range)
 */
int32_t OemOilPanel::MapPressureValue(int32_t sensor_value)
{

    if (!configuration_manager_)
    {
        int32_t result = MapBarPressure(sensor_value);
        log_v("MapPressureValue() returning (fallback): %d", result);
        return result;
    }

    std::string pressure_unit = "Bar"; // Default
    if (auto unit_value = configuration_manager_->QueryConfig<std::string>(OilPressureSensor::CONFIG_UNIT))
    {
        pressure_unit = *unit_value;
    }
    int32_t mapped_value = MapPressureByUnit(sensor_value, pressure_unit);

    log_v("MapPressureValue() returning: %d", mappedValue);
    return mapped_value;
}

/**
 * @brief Maps pressure sensor value to display scale based on unit type
 * @param sensorValue Raw sensor reading (in configured unit)
 * @param unit Pressure unit (PSI, kPa, or Bar)
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Converts sensor value to base Bar unit if needed, then maps to display scale.
 * This simplifies the logic by always working with Bar internally.
 */
int32_t OemOilPanel::MapPressureByUnit(int32_t sensor_value, const std::string &unit)
{
    // Convert to base unit (Bar) if needed
    float bar_value;
    if (unit == config_constants::units::PSI_UPPER)
    {
        bar_value = unit_converter::PsiToBar(static_cast<float>(sensor_value));
    }
    else if (unit == config_constants::units::KPA_UPPER)
    {
        bar_value = unit_converter::KpaToBar(static_cast<float>(sensor_value));
    }
    else
    {
        bar_value = static_cast<float>(sensor_value);
    }

    // Map Bar value to display scale
    return MapBarPressure(static_cast<int32_t>(bar_value));
}

/**
 * @brief Maps PSI pressure sensor value to display scale
 * @param sensorValue Raw sensor reading in PSI
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Converts PSI to Bar and uses unified Bar mapping.
 */
int32_t OemOilPanel::MapPSIPressure(int32_t sensor_value)
{
    // Convert PSI to Bar and use unified mapping
    float bar_value = unit_converter::PsiToBar(static_cast<float>(sensor_value));
    return MapBarPressure(static_cast<int32_t>(bar_value));
}

/**
 * @brief Maps kPa pressure sensor value to display scale
 * @param sensorValue Raw sensor reading in kPa
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Converts kPa to Bar and uses unified Bar mapping.
 */
int32_t OemOilPanel::MapkPaPressure(int32_t sensor_value)
{
    // Convert kPa to Bar and use unified mapping
    float bar_value = unit_converter::KpaToBar(static_cast<float>(sensor_value));
    return MapBarPressure(static_cast<int32_t>(bar_value));
}

/**
 * @brief Maps Bar pressure sensor value to display scale
 * @param sensorValue Raw sensor reading in Bar (0-10 range)
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Maps automotive Bar pressure range (1-10 Bar) to gauge display scale.
 * This is the base mapping function used by all unit conversions.
 */
int32_t OemOilPanel::MapBarPressure(int32_t sensor_value)
{
    // Bar: sensor returns 0-10 Bar, map to 0-60 display
    // Clamp to valid range (1-10 Bar)
    int32_t clamped_value = ClampValue(sensor_value, 1, sensor_constants::PRESSURE_MAX_BAR);

    // Map Bar range (1-10) to display scale (0-60)
    return ((clamped_value - 1) * 60) / (sensor_constants::PRESSURE_MAX_BAR - 1);
}

/**
 * @brief Clamps a value to specified minimum and maximum bounds
 * @param value Input value to clamp
 * @param minVal Minimum allowed value
 * @param maxVal Maximum allowed value
 * @return Clamped value within bounds
 *
 * Utility function that ensures sensor values stay within valid operational
 * ranges for automotive applications. Prevents gauge display errors from
 * out-of-range sensor readings.
 */
int32_t OemOilPanel::ClampValue(int32_t value, int32_t min_val, int32_t max_val)
{
    if (value < min_val)
        return min_val;
    if (value > max_val)
        return max_val;
    return value;
}

/**
 * @brief Map oil temperature sensor value to display scale
 * @param sensorValue Temperature value in configured units
 * @return Mapped value for display (0-120 scale)
 */
int32_t OemOilPanel::MapTemperatureValue(int32_t sensor_value)
{
    // Convert to base unit (Celsius) if needed
    float celsius_value;

    if (configuration_manager_)
    {
        std::string temp_unit = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT; // Default
        if (auto unit_value = configuration_manager_->QueryConfig<std::string>(OilTemperatureSensor::CONFIG_UNIT))
        {
            temp_unit = *unit_value;
        }

        if (temp_unit == config_constants::units::FAHRENHEIT)
        {
            // Convert Fahrenheit to Celsius
            celsius_value = unit_converter::FahrenheitToCelsius(static_cast<float>(sensor_value));
        }
        else
        {
            // Already in Celsius
            celsius_value = static_cast<float>(sensor_value);
        }
    }
    else
    {
        // No preference service - assume Celsius
        celsius_value = static_cast<float>(sensor_value);
    }

    // Clamp to valid range (0-120°C) and map directly to display scale
    int32_t mapped_value = static_cast<int32_t>(celsius_value);
    mapped_value =
        ClampValue(mapped_value, sensor_constants::TEMPERATURE_MIN_CELSIUS, sensor_constants::TEMPERATURE_MAX_CELSIUS);

    // Direct 1:1 mapping for Celsius to display scale (0-120)
    log_v("MapTemperatureValue() returning: %d", mappedValue);
    return mapped_value;
}

// ========== IActionService Interface Implementation ==========

// ========== Action Handler Methods ==========

/**
 * @brief Handles short button press during oil monitoring display
 *
 * Currently provides no-op functionality. The OEM oil panel
 * is primarily a monitoring display without direct interaction.
 */
void OemOilPanel::HandleShortPress()
{
    // No action on short press - monitoring panel
    log_v("OemOilPanel short press - no action");
}
void OemOilPanel::HandleLongPress()
{
    if (panel_manager_)
    {
        log_i("OemOilPanel long press - loading config panel");
        panel_manager_->CreateAndLoadPanel(panel_names::CONFIG, true);
    }
    else
    {
        log_w("OemOilPanel: Cannot load config panel - panelManager not available");
    }
}

// ========== Helper Methods ==========

/**
 * @brief Calculate deadband values from configuration percentiles
 *
 * @details Reads deadband percentile configuration (1%, 3%, or 5%) and calculates
 * the actual deadband values based on component scale ranges retrieved dynamically:
 * - Pressure: Component-managed scale representing 0.0-6.0 Bar
 * - Temperature: Component-managed scale representing 0-120°C
 *
 * Scale ranges are obtained from components to ensure accuracy and maintainability.
 * Examples (assuming pressure component returns 0-60 range):
 * - 1% = 0.6 scale units = 0.06 Bar threshold
 * - 3% = 1.8 scale units = 0.18 Bar threshold
 * - 5% = 3.0 scale units = 0.30 Bar threshold
 */
void OemOilPanel::CalculateDeadbands()
{
    // Get pressure deadband percentage from pressure component configuration (default: 3%)
    int32_t pressure_percent = DEFAULT_PRESSURE_DEADBAND_PERCENT;
    if (configuration_manager_)
    {
        if (auto config_value =
                configuration_manager_->QueryConfig<int>(config_constants::keys::OIL_PRESSURE_DEADBAND_PERCENT))
        {
            pressure_percent = *config_value;
            log_i("Using configured pressure deadband: %d%%", pressurePercent);
        }
        else
        {
            log_i("No pressure deadband config found, using default: %d%%", pressurePercent);
        }
    }

    // Get temperature deadband percentage from temperature component configuration (default: 3%)
    int32_t temperature_percent = DEFAULT_TEMPERATURE_DEADBAND_PERCENT;
    if (configuration_manager_)
    {
        if (auto config_value =
                configuration_manager_->QueryConfig<int>(config_constants::keys::OIL_TEMPERATURE_DEADBAND_PERCENT))
        {
            temperature_percent = *config_value;
            log_i("Using configured temperature deadband: %d%%", temperaturePercent);
        }
        else
        {
            log_i("No temperature deadband config found, using default: %d%%", temperaturePercent);
        }
    }

    // Calculate actual deadband values from percentiles using component scale constants
    // Reference the same constants that components use to ensure consistency
    const int32_t pressure_scale_range =
        sensor_constants::PRESSURE_DISPLAY_SCALE_MAX - sensor_constants::PRESSURE_DISPLAY_SCALE_MIN;
    const int32_t temperature_scale_range =
        sensor_constants::TEMPERATURE_DISPLAY_SCALE_MAX - sensor_constants::TEMPERATURE_DISPLAY_SCALE_MIN;

    // Pressure: (percent * component_scale_range) / 100
    pressure_deadband_ = (pressure_percent * pressure_scale_range) / 100;
    if (pressure_deadband_ < 1)
        pressure_deadband_ = 1; // Minimum 1 scale unit

    // Temperature: (percent * component_scale_range) / 100
    temperature_deadband_ = (temperature_percent * temperature_scale_range) / 100;
    if (temperature_deadband_ < 1)
        temperature_deadband_ = 1; // Minimum 1 scale unit

    log_i("Calculated deadbands from component scale constants:");
    log_i("  Pressure: %d-%d range (%d units), %d%% = %d scale units (%.1f Bar threshold)",
          sensor_constants::PRESSURE_DISPLAY_SCALE_MIN, sensor_constants::PRESSURE_DISPLAY_SCALE_MAX,
          pressureScaleRange, pressurePercent, pressure_deadband_, pressure_deadband_ / 10.0f);
    log_i("  Temperature: %d-%d range (%d units), %d%% = %d scale units (%d°C threshold)",
          sensor_constants::TEMPERATURE_DISPLAY_SCALE_MIN, sensor_constants::TEMPERATURE_DISPLAY_SCALE_MAX,
          temperatureScaleRange, temperaturePercent, temperature_deadband_, temperature_deadband_);
}
