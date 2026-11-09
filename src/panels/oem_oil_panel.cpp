#include "panels/oem_oil_panel.h"
#include "components/oem/oem_oil_pressure_component.h"
#include "components/oem/oem_oil_temperature_component.h"
#include "sensors/oil_pressure_sensor.h"
#include "sensors/oil_temperature_sensor.h"
#include "definitions/constants.h"
#include "managers/error_manager.h"
#include "managers/panel_manager.h"
#include "managers/style_manager.h"
#include "utilities/unit_converter.h"
#include "utilities/logging.h"
#include "managers/configuration_manager.h"


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
OemOilPanel::OemOilPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *styleManager,
                         IPanelManager *panelManager, IConfigurationManager *configurationManager)
    : gpioProvider_(gpio), displayProvider_(display), styleManager_(styleManager), panelManager_(panelManager),
      configurationManager_(configurationManager),
      oemOilPressureComponent_(styleManager), oemOilTemperatureComponent_(styleManager),
      oemOilPressureSensor_(std::make_shared<OilPressureSensor>(gpio)),
      oemOilTemperatureSensor_(std::make_shared<OilTemperatureSensor>(gpio)),
      componentsInitialized_(false), currentOilPressureValue_(-1),
      currentOilTemperatureValue_(-1), pressureDeadband_(0), temperatureDeadband_(0),
      lastAnimatedPressureValue_(-1), lastAnimatedTemperatureValue_(-1), lastTheme_("")
{
    log_v("OemOilPanel constructor called");
    // Initialize LVGL animation structures to prevent undefined behavior
    lv_anim_init(&pressureAnimation_);
    lv_anim_init(&temperatureAnimation_);

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
    if (animationState_ != AnimationState::IDLE)
    {
        lv_anim_delete(&pressureAnimation_, nullptr);
        lv_anim_delete(&temperatureAnimation_, nullptr);
        animationState_ = AnimationState::IDLE;
    }

    // Delete all animations that might reference this instance
    lv_anim_delete(this, nullptr);

    if (screen_)
    {
        lv_obj_delete(screen_);
        screen_ = nullptr;
    }

    // Components delete no LVGL objects, so destroying them after the screen is safe

    if (oemOilPressureSensor_)
    {
        oemOilPressureSensor_.reset();
    }

    if (oemOilTemperatureSensor_)
    {
        oemOilTemperatureSensor_.reset();
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

    if (!displayProvider_)
    {
        log_e("OemOilPanel requires display provider");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel", "Missing required display provider");
        return;
    }

    if (!oemOilPressureSensor_ || !oemOilTemperatureSensor_)
    {
        log_e("OemOilPanel requires pressure and temperature sensors");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel", "Missing required sensors");
        return;
    }

    screen_ = displayProvider_->CreateScreen();

    // Apply current theme immediately after screen creation
    if (styleManager_)
    {
        styleManager_->ApplyThemeToScreen(screen_);
    }

    oemOilPressureSensor_->Init();
    currentOilPressureValue_ = -1; // Sentinel value to ensure first update
    lastAnimatedPressureValue_ = -1; // Reset hysteresis tracking

    oemOilTemperatureSensor_->Init();
    currentOilTemperatureValue_ = -1; // Sentinel value to ensure first update
    lastAnimatedTemperatureValue_ = -1; // Reset hysteresis tracking

    // Calculate deadbands from configuration percentiles
    calculateDeadbands();

    // Cache sensor references to avoid repeated static_pointer_cast operations
    cachedPressureSensor_ = std::static_pointer_cast<OilPressureSensor>(oemOilPressureSensor_);
    cachedTemperatureSensor_ = std::static_pointer_cast<OilTemperatureSensor>(oemOilTemperatureSensor_);
    
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
    if (!styleManager_ || !styleManager_->IsInitialized())
    {
        log_w("StyleService not properly initialized, skipping component creation");
        return;
    }

    // Components are now stack-allocated and initialized in constructor
    // Mark components as initialized for initialization checks
    componentsInitialized_ = true;

    // Create location parameters with rotational start points for scales
    ComponentLocation pressureLocation(210);   // rotation starting at 210 degrees
    ComponentLocation temperatureLocation(30); // rotation starting at 30 degrees

    // Render both components
    oemOilPressureComponent_.Render(screen_, pressureLocation, displayProvider_);
    oemOilTemperatureComponent_.Render(screen_, temperatureLocation, displayProvider_);
    
    // Initialize needle positions to match current values (prevents animation jumps)
    oemOilPressureComponent_.SetValue(currentOilPressureValue_);
    oemOilTemperatureComponent_.SetValue(currentOilTemperatureValue_);
    
    // Initialize needle colors based on initial values
    oemOilPressureComponent_.Refresh(Reading{currentOilPressureValue_});
    oemOilTemperatureComponent_.Refresh(Reading{currentOilTemperatureValue_});
    
    
    // Log heap status before critical operation
    
    // Validate screen object pointer and basic properties
    if (screen_) {
        // Check if LVGL object is valid by accessing safe properties
        lv_obj_t* parent = lv_obj_get_parent(screen_);
        
        // Check object coordinates (safe to read)
        lv_coord_t x = lv_obj_get_x(screen_);
        lv_coord_t y = lv_obj_get_y(screen_);
        
        // Check object size
        lv_coord_t w = lv_obj_get_width(screen_);
        lv_coord_t h = lv_obj_get_height(screen_);
    } else {
        log_e("  screen_ is NULL!");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel",
                                                     "screen_ is NULL during periodic check");
    }
    
    // Validate component pointers
    
    // Check if 'this' pointer is still valid
    
    lv_obj_add_event_cb(screen_, OemOilPanel::ShowPanelCompletionCallback, LV_EVENT_SCREEN_LOADED, this);
    
    // Final validation before the critical call
    

    lv_screen_load(screen_);
    
    // Post-load validation

    if (styleManager_)
    {
        styleManager_->ApplyThemeToScreen(screen_);
        // Update lastTheme_ to current theme to sync with theme detection in update()
        lastTheme_ = String(styleManager_->GetCurrentTheme().c_str());
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
    const char *currentTheme = styleManager_ ? styleManager_->GetCurrentTheme().c_str() : "";
    bool themeChanged = lastTheme_.isEmpty() || !lastTheme_.equals(currentTheme);
    if (themeChanged)
    {
        lastTheme_ = String(currentTheme);
        if (styleManager_ && screen_)
        {
            styleManager_->ApplyThemeToScreen(screen_);
        }
    }

    // Apply sensor settings periodically (every ~100 updates) instead of every cycle
    // This reduces overhead while still catching preference changes reasonably quickly
    static uint16_t settingsUpdateCounter = 0;
    if (++settingsUpdateCounter % 100 == 0)
    {
        ApplyCurrentSensorSettings();
    }

    // Update sensor readings and components (pass theme change flag)
    UpdateOilPressure(themeChanged);
    UpdateOilTemperature(themeChanged);

    // If no animations were started, updates complete immediately
    if (animationState_ == AnimationState::IDLE)
    {
        if (panelManager_)
        {
            panelManager_->SetUiState(UIState::IDLE);
        }
    }
}

// ========== Private Methods ==========

/**
 * @brief Update the oil pressure reading on the screen
 */
void OemOilPanel::UpdateOilPressure(bool forceRefresh)
{
    // Skip if pressure animation is running unless this is a forced refresh
    if (!forceRefresh && (animationState_ == AnimationState::PRESSURE_RUNNING || animationState_ == AnimationState::BOTH_RUNNING))
    {
        return;
    }

    // Safety check for sensor availability
    if (!oemOilPressureSensor_)
    {
        log_e("Pressure sensor is null - cannot update pressure display!");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel",
                                                     "Pressure sensor is null - gauge cannot function");
        return;
    }

    // Use delta-based updates for better performance
    auto sensorValue = std::get<int32_t>(oemOilPressureSensor_->GetReading());
    auto value = MapPressureValue(sensorValue);

    // Apply hysteresis/deadband to prevent jittery updates from sensor noise
    // Only animate if change exceeds deadband threshold or this is the first reading
    bool shouldAnimate = false;
    if (lastAnimatedPressureValue_ == -1) {
        // First reading - always animate
        shouldAnimate = true;
    } else {
        // Check if change exceeds deadband threshold
        int32_t changeMagnitude = abs(value - lastAnimatedPressureValue_);
        shouldAnimate = changeMagnitude >= pressureDeadband_;
    }

    // Handle forced refresh (theme changes) or skip if no significant change
    if (!shouldAnimate && !forceRefresh)
    {
        // Update current value for tracking but don't animate
        currentOilPressureValue_ = value;
        return; // No animation needed - change within deadband
    }

    if (forceRefresh && value == currentOilPressureValue_)
    {
        oemOilPressureComponent_.Refresh(Reading{value});
        return; // Theme refresh but no value change
    }

    log_i("Updating pressure from %d to %d (exceeds deadband of %d) [%.1f to %.1f Bar]",
          currentOilPressureValue_, value, pressureDeadband_,
          currentOilPressureValue_ / 10.0f, value / 10.0f);
    oemOilPressureComponent_.Refresh(Reading{value});

    // Track this value as the last animated value for hysteresis
    lastAnimatedPressureValue_ = value;

    // Determine animation start value
    // If current value is -1 (initial state) or outside scale bounds, start from appropriate boundary
    int32_t animationStartValue = currentOilPressureValue_;
    // PRESSURE SCALE: Component manages scale representing 0.0-6.0 Bar with one decimal place precision
    // Scale unit 1 = 0.1 Bar, so value 55 = 5.5 Bar, value 23 = 2.3 Bar
    // Using same constants that components use to ensure consistency
    const int32_t scaleMin = SensorConstants::PRESSURE_DISPLAY_SCALE_MIN;  // Pressure scale minimum: 0.0 Bar
    const int32_t scaleMax = SensorConstants::PRESSURE_DISPLAY_SCALE_MAX;  // Pressure scale maximum: 6.0 Bar
    
    if (currentOilPressureValue_ == -1)
    {
        // Initial load - determine start position based on target value
        if (value < scaleMin)
        {
            animationStartValue = scaleMin; // Start from minimum if value is below scale
        }
        else if (value > scaleMax)
        {
            animationStartValue = scaleMax; // Start from maximum if value is above scale
        }
        else
        {
            // Value is within scale bounds
            // For pressure, always start from minimum (0.0 Bar) for initial animation
            // This represents the engine starting up and building pressure from 0
            animationStartValue = scaleMin;
        }
        log_i("Initial pressure animation: starting from %d to %d", animationStartValue, value);
    }
    else if (currentOilPressureValue_ < scaleMin && value >= scaleMin)
    {
        // Coming from below scale minimum into visible range
        animationStartValue = scaleMin;
        log_i("Pressure entering scale from below: animating from %d to %d", animationStartValue, value);
    }
    else if (currentOilPressureValue_ > scaleMax && value <= scaleMax)
    {
        // Coming from above scale maximum into visible range
        animationStartValue = scaleMax;
        log_i("Pressure entering scale from above: animating from %d to %d", animationStartValue, value);
    }

    // Setup animation
    lv_anim_init(&pressureAnimation_);
    lv_anim_set_duration(&pressureAnimation_, _animation_duration);
    lv_anim_set_repeat_count(&pressureAnimation_, 0);
    lv_anim_set_playback_duration(&pressureAnimation_, 0);
    lv_anim_set_values(&pressureAnimation_, animationStartValue, value);
    lv_anim_set_var(&pressureAnimation_, this);
    lv_anim_set_user_data(&pressureAnimation_, (void *)static_cast<uintptr_t>(OilSensorTypes::Pressure));
    lv_anim_set_exec_cb(&pressureAnimation_, OemOilPanel::ExecutePressureAnimationCallback);
    lv_anim_set_completed_cb(&pressureAnimation_, OemOilPanel::UpdatePanelCompletionCallback);

    // Update animation state based on current state
    if (animationState_ == AnimationState::TEMPERATURE_RUNNING) {
        animationState_ = AnimationState::BOTH_RUNNING;
    } else {
        animationState_ = AnimationState::PRESSURE_RUNNING;
    }
    // Set ANIMATING state before starting animation
    if (panelManager_)
    {
        panelManager_->SetUiState(UIState::BUSY);
    }
    // Start pressure gauge animation
    lv_anim_start(&pressureAnimation_);
}

/**
 * @brief Update the oil temperature reading on the screen
 */
void OemOilPanel::UpdateOilTemperature(bool forceRefresh)
{

    // Skip update if temperature animation is already running
    if (animationState_ == AnimationState::TEMPERATURE_RUNNING || animationState_ == AnimationState::BOTH_RUNNING)
    {
        // Validate that the LVGL animation is actually still running
        // If not, this is a stale flag that needs to be cleared
        if (!lv_anim_get(&temperatureAnimation_, nullptr))
        {
            log_w("UpdateOilTemperature: Found stale animation flag, clearing it");
            if (animationState_ == AnimationState::TEMPERATURE_RUNNING) {
                animationState_ = AnimationState::IDLE;
            } else if (animationState_ == AnimationState::BOTH_RUNNING) {
                animationState_ = AnimationState::PRESSURE_RUNNING;
            }
        }
        else
        {
            return;
        }
    }

    // Safety check for sensor availability
    if (!oemOilTemperatureSensor_)
    {
        log_e("Temperature sensor is null - cannot update temperature display!");
        ErrorManager::Instance().ReportCriticalError("OemOilPanel",
                                                     "Temperature sensor is null - gauge cannot function");
        return;
    }

    // Use delta-based updates for better performance
    auto sensorValue = std::get<int32_t>(oemOilTemperatureSensor_->GetReading());
    auto value = MapTemperatureValue(sensorValue);

    // Apply hysteresis/deadband to prevent jittery updates from sensor noise
    // Only animate if change exceeds deadband threshold or this is the first reading
    bool shouldAnimate = false;
    if (lastAnimatedTemperatureValue_ == -1) {
        // First reading - always animate
        shouldAnimate = true;
    } else {
        // Check if change exceeds deadband threshold
        int32_t changeMagnitude = abs(value - lastAnimatedTemperatureValue_);
        shouldAnimate = changeMagnitude >= temperatureDeadband_;
    }

    // Handle forced refresh (theme changes) or skip if no significant change
    if (!shouldAnimate && !forceRefresh)
    {
        // Update current value for tracking but don't animate
        currentOilTemperatureValue_ = value;
        return; // No animation needed - change within deadband
    }

    if (forceRefresh && value == currentOilTemperatureValue_)
    {
        oemOilTemperatureComponent_.Refresh(Reading{value});
        return; // Theme refresh but no value change
    }

    log_i("Updating temperature from %d to %d (exceeds deadband of %d°C)", currentOilTemperatureValue_, value, temperatureDeadband_);
    oemOilTemperatureComponent_.Refresh(Reading{value});

    // Track this value as the last animated value for hysteresis
    lastAnimatedTemperatureValue_ = value;

    // Determine animation start value
    // If current value is -1 (initial state) or outside scale bounds, start from appropriate boundary
    int32_t animationStartValue = currentOilTemperatureValue_;
    // TEMPERATURE SCALE: Component manages scale representing 0-120°C
    // Using same constants that components use to ensure consistency
    const int32_t scaleMin = SensorConstants::TEMPERATURE_DISPLAY_SCALE_MIN;   // Temperature scale minimum
    const int32_t scaleMax = SensorConstants::TEMPERATURE_DISPLAY_SCALE_MAX;   // Temperature scale maximum
    
    if (currentOilTemperatureValue_ == -1)
    {
        // Initial load - determine start position based on target value
        if (value < scaleMin)
        {
            animationStartValue = scaleMin; // Start from minimum if value is below scale
        }
        else if (value > scaleMax)
        {
            animationStartValue = scaleMax; // Start from maximum if value is above scale
        }
        else
        {
            // Value is within scale bounds
            // For temperature, always start from minimum (cold) for initial animation
            // This represents a cold engine warming up, which is the typical scenario
            animationStartValue = scaleMin;
        }
        log_i("Initial temperature animation: starting from %d to %d", animationStartValue, value);
    }
    else if (currentOilTemperatureValue_ < scaleMin && value >= scaleMin)
    {
        // Coming from below scale minimum into visible range
        animationStartValue = scaleMin;
        log_i("Temperature entering scale from below: animating from %d to %d", animationStartValue, value);
    }
    else if (currentOilTemperatureValue_ > scaleMax && value <= scaleMax)
    {
        // Coming from above scale maximum into visible range
        animationStartValue = scaleMax;
        log_i("Temperature entering scale from above: animating from %d to %d", animationStartValue, value);
    }

    // Setup animation
    lv_anim_init(&temperatureAnimation_);
    lv_anim_set_duration(&temperatureAnimation_, _animation_duration);
    lv_anim_set_repeat_count(&temperatureAnimation_, 0);
    lv_anim_set_playback_duration(&temperatureAnimation_, 0);
    lv_anim_set_values(&temperatureAnimation_, animationStartValue, value);
    lv_anim_set_var(&temperatureAnimation_, this);
    lv_anim_set_user_data(&temperatureAnimation_, (void *)static_cast<uintptr_t>(OilSensorTypes::Temperature));
    lv_anim_set_exec_cb(&temperatureAnimation_, OemOilPanel::ExecuteTemperatureAnimationCallback);
    lv_anim_set_completed_cb(&temperatureAnimation_, OemOilPanel::UpdatePanelCompletionCallback);

    // Update animation state based on current state
    if (animationState_ == AnimationState::PRESSURE_RUNNING) {
        animationState_ = AnimationState::BOTH_RUNNING;
    } else {
        animationState_ = AnimationState::TEMPERATURE_RUNNING;
    }
    // Set ANIMATING state before starting animation
    if (panelManager_)
    {
        panelManager_->SetUiState(UIState::BUSY);
    }
    // Start temperature gauge animation
    lv_anim_start(&temperatureAnimation_);
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
    if (!panelManager_)
    {
        log_w("OemOilPanel: PanelService not available, returning no action");
        return Action(nullptr);
    }

    return Action(
        [this]()
        {
            panelManager_->CreateAndLoadPanel(PanelNames::CONFIG, true);
        });
}
*/

// ========== Configuration Methods ==========

/**
 * @brief Apply current sensor settings from preferences directly
 */
void OemOilPanel::ApplyCurrentSensorSettings()
{
    // Apply updateRate and units from preferences to sensors if preference service is available
    if (configurationManager_ && cachedPressureSensor_ && cachedTemperatureSensor_)
    {
        // Get individual preferences using type-safe config system
        int updateRate = 500; // Default
        if (auto rateValue = configurationManager_->QueryConfig<int>(ConfigConstants::Keys::SYSTEM_UPDATE_RATE)) {
            updateRate = *rateValue;
        }

        std::string pressureUnit = ConfigConstants::Defaults::DEFAULT_PRESSURE_UNIT; // Default
        if (auto unitValue = configurationManager_->QueryConfig<std::string>(OilPressureSensor::CONFIG_UNIT)) {
            pressureUnit = *unitValue;
        }

        std::string tempUnit = ConfigConstants::Defaults::DEFAULT_TEMPERATURE_UNIT; // Default
        if (auto unitValue = configurationManager_->QueryConfig<std::string>(OilTemperatureSensor::CONFIG_UNIT)) {
            tempUnit = *unitValue;
        }

        // Only update settings if they have changed
        bool updateRateChanged = (lastUpdateRate_ != updateRate);
        bool pressureUnitChanged = !lastPressureUnit_.equals(pressureUnit.c_str());
        bool tempUnitChanged = !lastTempUnit_.equals(tempUnit.c_str());

        if (updateRateChanged)
        {
            cachedPressureSensor_->SetUpdateRate(updateRate);
            cachedTemperatureSensor_->SetUpdateRate(updateRate);
            lastUpdateRate_ = updateRate;
        }

        if (pressureUnitChanged)
        {
            cachedPressureSensor_->SetTargetUnit(pressureUnit);
            lastPressureUnit_ = String(pressureUnit.c_str());
        }

        if (tempUnitChanged)
        {
            cachedTemperatureSensor_->SetTargetUnit(tempUnit);
            lastTempUnit_ = String(tempUnit.c_str());
        }

        // Only log if something actually changed
        if (updateRateChanged || pressureUnitChanged || tempUnitChanged)
        {
            log_t("Sensor settings updated - Rate: %dms, Pressure: %s, Temp: %s",
                  updateRate, pressureUnit.c_str(), tempUnit.c_str());
        }
    }
    else if (!configurationManager_)
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

    auto thisInstance = static_cast<OemOilPanel *>(lv_event_get_user_data(event));
    if (!thisInstance)
    {
        return;
    }

    // Set UI state to IDLE so main loop can start calling Update()
    if (thisInstance->panelManager_)
    {
        thisInstance->panelManager_->SetUiState(UIState::IDLE);
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

    auto thisInstance = static_cast<OemOilPanel *>(animation->var);
    auto sensorType = static_cast<OilSensorTypes>(reinterpret_cast<uintptr_t>(animation->user_data));

    // Determine which animation has completed and update the corresponding value
    if (sensorType == OilSensorTypes::Pressure)
    {
        thisInstance->currentOilPressureValue_ = animation->current_value;
        // Update animation state when pressure animation completes
        if (thisInstance->animationState_ == AnimationState::PRESSURE_RUNNING) {
            thisInstance->animationState_ = AnimationState::IDLE;
        } else if (thisInstance->animationState_ == AnimationState::BOTH_RUNNING) {
            thisInstance->animationState_ = AnimationState::TEMPERATURE_RUNNING;
        }
    }
    else if (sensorType == OilSensorTypes::Temperature)
    {
        thisInstance->currentOilTemperatureValue_ = animation->current_value;
        // Update animation state when temperature animation completes
        if (thisInstance->animationState_ == AnimationState::TEMPERATURE_RUNNING) {
            thisInstance->animationState_ = AnimationState::IDLE;
        } else if (thisInstance->animationState_ == AnimationState::BOTH_RUNNING) {
            thisInstance->animationState_ = AnimationState::PRESSURE_RUNNING;
        }
    }

    // Only set UI state to idle when all animations are complete
    if (thisInstance->animationState_ == AnimationState::IDLE)
    {
        // Set IDLE state when all animations are complete
        if (thisInstance->panelManager_)
        {
            thisInstance->panelManager_->SetUiState(UIState::IDLE);
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

    auto thisInstance =
        static_cast<OemOilPanel *>(animation->var); // use the animation to get the var which is this instance
    // Component is now stack-allocated, always available
    thisInstance->oemOilPressureComponent_.SetValue(value);
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

    auto thisInstance =
        static_cast<OemOilPanel *>(animation->var); // use the animation to get the var which is this instance
    // Component is now stack-allocated, always available
    thisInstance->oemOilTemperatureComponent_.SetValue(value);
}

// ========== Value Mapping Methods ==========

/**
 * @brief Map oil pressure sensor value to display scale
 * @param sensorValue Raw sensor value (1-10 Bar)
 * @return Mapped value for display (0-60, representing pressure range)
 */
int32_t OemOilPanel::MapPressureValue(int32_t sensorValue)
{

    if (!configurationManager_)
    {
        int32_t result = MapBarPressure(sensorValue);
        log_v("MapPressureValue() returning (fallback): %d", result);
        return result;
    }

    std::string pressureUnit = "Bar"; // Default
    if (auto unitValue = configurationManager_->QueryConfig<std::string>(OilPressureSensor::CONFIG_UNIT)) {
        pressureUnit = *unitValue;
    }
    int32_t mappedValue = MapPressureByUnit(sensorValue, pressureUnit);
    
    log_v("MapPressureValue() returning: %d", mappedValue);
    return mappedValue;
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
int32_t OemOilPanel::MapPressureByUnit(int32_t sensorValue, const std::string& unit)
{
    // Convert to base unit (Bar) if needed
    float barValue;
    if (unit == ConfigConstants::Units::PSI_UPPER) {
        barValue = UnitConverter::PsiToBar(static_cast<float>(sensorValue));
    } else if (unit == ConfigConstants::Units::KPA_UPPER) {
        barValue = UnitConverter::KpaToBar(static_cast<float>(sensorValue));
    } else {
        barValue = static_cast<float>(sensorValue);
    }

    // Map Bar value to display scale
    return MapBarPressure(static_cast<int32_t>(barValue));
}

/**
 * @brief Maps PSI pressure sensor value to display scale
 * @param sensorValue Raw sensor reading in PSI
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Converts PSI to Bar and uses unified Bar mapping.
 */
int32_t OemOilPanel::MapPSIPressure(int32_t sensorValue)
{
    // Convert PSI to Bar and use unified mapping
    float barValue = UnitConverter::PsiToBar(static_cast<float>(sensorValue));
    return MapBarPressure(static_cast<int32_t>(barValue));
}

/**
 * @brief Maps kPa pressure sensor value to display scale
 * @param sensorValue Raw sensor reading in kPa
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Converts kPa to Bar and uses unified Bar mapping.
 */
int32_t OemOilPanel::MapkPaPressure(int32_t sensorValue)
{
    // Convert kPa to Bar and use unified mapping
    float barValue = UnitConverter::KpaToBar(static_cast<float>(sensorValue));
    return MapBarPressure(static_cast<int32_t>(barValue));
}

/**
 * @brief Maps Bar pressure sensor value to display scale
 * @param sensorValue Raw sensor reading in Bar (0-10 range)
 * @return Mapped value for gauge display (0-60 scale)
 *
 * Maps automotive Bar pressure range (1-10 Bar) to gauge display scale.
 * This is the base mapping function used by all unit conversions.
 */
int32_t OemOilPanel::MapBarPressure(int32_t sensorValue)
{
    // Bar: sensor returns 0-10 Bar, map to 0-60 display
    // Clamp to valid range (1-10 Bar)
    int32_t clampedValue = ClampValue(sensorValue, 1, SensorConstants::PRESSURE_MAX_BAR);

    // Map Bar range (1-10) to display scale (0-60)
    return ((clampedValue - 1) * 60) / (SensorConstants::PRESSURE_MAX_BAR - 1);
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
int32_t OemOilPanel::ClampValue(int32_t value, int32_t minVal, int32_t maxVal)
{
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

/**
 * @brief Map oil temperature sensor value to display scale
 * @param sensorValue Temperature value in configured units
 * @return Mapped value for display (0-120 scale)
 */
int32_t OemOilPanel::MapTemperatureValue(int32_t sensorValue)
{
    // Convert to base unit (Celsius) if needed
    float celsiusValue;

    if (configurationManager_)
    {
        std::string tempUnit = ConfigConstants::Defaults::DEFAULT_TEMPERATURE_UNIT; // Default
        if (auto unitValue = configurationManager_->QueryConfig<std::string>(OilTemperatureSensor::CONFIG_UNIT)) {
            tempUnit = *unitValue;
        }

        if (tempUnit == ConfigConstants::Units::FAHRENHEIT)
        {
            // Convert Fahrenheit to Celsius
            celsiusValue = UnitConverter::FahrenheitToCelsius(static_cast<float>(sensorValue));
        }
        else
        {
            // Already in Celsius
            celsiusValue = static_cast<float>(sensorValue);
        }
    }
    else
    {
        // No preference service - assume Celsius
        celsiusValue = static_cast<float>(sensorValue);
    }

    // Clamp to valid range (0-120°C) and map directly to display scale
    int32_t mappedValue = static_cast<int32_t>(celsiusValue);
    mappedValue = ClampValue(mappedValue, SensorConstants::TEMPERATURE_MIN_CELSIUS, SensorConstants::TEMPERATURE_MAX_CELSIUS);

    // Direct 1:1 mapping for Celsius to display scale (0-120)
    log_v("MapTemperatureValue() returning: %d", mappedValue);
    return mappedValue;
}

// ========== IActionService Interface Implementation ==========

/**
 * @brief Static function for handling short button press during oil monitoring
 * @param panelContext Pointer to the oil panel instance (unused)
 *
 * Currently provides no action for short button presses during oil monitoring.
 * The OEM oil panel is primarily a monitoring display without direct short press
 * interaction functionality.
 */
static void OemOilPanelShortPress(void* panelContext)
{
    log_v("OemOilPanelShortPress() called");
    // No action for short press on oil panel
}

/**
 * @brief Static function for handling long button press during oil monitoring
 * @param panelContext Pointer to the oil panel instance
 *
 * Handles long button press events by navigating to the configuration panel.
 * Provides safe casting and null pointer checking before delegating to the
 * instance method for panel navigation and settings access.
 */
static void OemOilPanelLongPress(void* panelContext)
{
    log_v("OemOilPanelLongPress() called");
    auto* panel = static_cast<OemOilPanel*>(panelContext);
    
    if (panel)
    {
        // Call public method to handle the long press
        panel->HandleLongPress();
    }
}

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
    if (panelManager_)
    {
        log_i("OemOilPanel long press - loading config panel");
        panelManager_->CreateAndLoadPanel(PanelNames::CONFIG, true);
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
void OemOilPanel::calculateDeadbands()
{
    // Get pressure deadband percentage from pressure component configuration (default: 3%)
    int32_t pressurePercent = DEFAULT_PRESSURE_DEADBAND_PERCENT;
    if (configurationManager_) {
        if (auto configValue = configurationManager_->QueryConfig<int>(ConfigConstants::Keys::OIL_PRESSURE_DEADBAND_PERCENT)) {
            pressurePercent = *configValue;
            log_i("Using configured pressure deadband: %d%%", pressurePercent);
        } else {
            log_i("No pressure deadband config found, using default: %d%%", pressurePercent);
        }
    }

    // Get temperature deadband percentage from temperature component configuration (default: 3%)
    int32_t temperaturePercent = DEFAULT_TEMPERATURE_DEADBAND_PERCENT;
    if (configurationManager_) {
        if (auto configValue = configurationManager_->QueryConfig<int>(ConfigConstants::Keys::OIL_TEMPERATURE_DEADBAND_PERCENT)) {
            temperaturePercent = *configValue;
            log_i("Using configured temperature deadband: %d%%", temperaturePercent);
        } else {
            log_i("No temperature deadband config found, using default: %d%%", temperaturePercent);
        }
    }

    // Calculate actual deadband values from percentiles using component scale constants
    // Reference the same constants that components use to ensure consistency
    const int32_t pressureScaleRange = SensorConstants::PRESSURE_DISPLAY_SCALE_MAX - SensorConstants::PRESSURE_DISPLAY_SCALE_MIN;
    const int32_t temperatureScaleRange = SensorConstants::TEMPERATURE_DISPLAY_SCALE_MAX - SensorConstants::TEMPERATURE_DISPLAY_SCALE_MIN;

    // Pressure: (percent * component_scale_range) / 100
    pressureDeadband_ = (pressurePercent * pressureScaleRange) / 100;
    if (pressureDeadband_ < 1) pressureDeadband_ = 1; // Minimum 1 scale unit

    // Temperature: (percent * component_scale_range) / 100
    temperatureDeadband_ = (temperaturePercent * temperatureScaleRange) / 100;
    if (temperatureDeadband_ < 1) temperatureDeadband_ = 1; // Minimum 1 scale unit

    log_i("Calculated deadbands from component scale constants:");
    log_i("  Pressure: %d-%d range (%d units), %d%% = %d scale units (%.1f Bar threshold)",
          SensorConstants::PRESSURE_DISPLAY_SCALE_MIN, SensorConstants::PRESSURE_DISPLAY_SCALE_MAX,
          pressureScaleRange, pressurePercent, pressureDeadband_, pressureDeadband_ / 10.0f);
    log_i("  Temperature: %d-%d range (%d units), %d%% = %d scale units (%d°C threshold)",
          SensorConstants::TEMPERATURE_DISPLAY_SCALE_MIN, SensorConstants::TEMPERATURE_DISPLAY_SCALE_MAX,
          temperatureScaleRange, temperaturePercent, temperatureDeadband_, temperatureDeadband_);
}

