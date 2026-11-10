#pragma once

#include "components/oem/oem_oil_pressure_component.h"
#include "components/oem/oem_oil_temperature_component.h"
#include "definitions/configs.h"
#include "definitions/constants.h"
#include "definitions/types.h"
#include "interfaces/i_action_handler.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_style_manager.h"
#include "sensors/oil_pressure_sensor.h"
#include "sensors/oil_temperature_sensor.h"
#include <Arduino.h>

// Forward declarations
// (Direct component usage - no factory needed)

/**
 * @class OemOilPanel
 * @brief Main oil monitoring dashboard panel
 *
 * @details This panel serves as the primary monitoring interface for engine oil
 * systems. It coordinates two specialized gauge components for pressure and
 * temperature monitoring, positioned side-by-side for optimal visibility.
 *
 * @presenter_role Coordinates OemOilPressureComponent and OemOilTemperatureComponent
 * @data_sources OilPressureSensor and OilTemperatureSensor with delta-based updates
 * @update_strategy Smart caching with animation-based smooth transitions
 *
 * @ui_layout:
 * - Oil Pressure Gauge: Left side (LV_ALIGN_LEFT_MID) - 120x120px
 * - Oil Temperature Gauge: Right side (LV_ALIGN_RIGHT_MID) - 120x120px
 * - Both gauges feature danger zone indicators and smooth needle animations
 *
 * @animation_system:
 * - Dual independent animations for pressure and temperature
 * - Prevents conflicts with separate animation objects
 * - Completion callbacks ensure proper synchronization
 *
 * @performance_optimizations:
 * - Delta-based updates (skips unchanged values)
 * - Cached previous values for comparison
 * - Efficient animation state tracking
 *
 * @context The components are currently set to 240x240 size in order to ensure
 * that they maintain a consistent appearance with OEM styling by being shown on either side of the screen.
 */
class OemOilPanel : public IPanel
{
  public:
    // ========== Constructors and Destructor ==========
    OemOilPanel(IGpioProvider *gpio, IDisplayProvider *display, IStyleManager *style_manager,
                IPanelManager *panel_manager, IConfigurationManager *configuration_manager);
    ~OemOilPanel();

    // ========== Public Interface Methods ==========
    static constexpr const char *NAME = panel_names::OIL;
    void Init() override;
    void Load() override;
    void Update() override;

    void ApplyCurrentSensorSettings();

    // IActionService Interface Implementation (inherited through IPanel)
    // Old function pointer methods removed - using direct HandleShortPress/HandleLongPress

    // Public action handlers
    void HandleShortPress() override;
    void HandleLongPress() override;

    // ========== Public Data Members ==========
    static constexpr int32_t ANIMATION_DURATION = 750;

  private:
    // ========== Private Methods ==========
    void UpdateOilPressure(bool force_refresh = false);
    void UpdateOilTemperature(bool force_refresh = false);

    // Value mapping methods
    int32_t MapPressureValue(int32_t sensor_value);

    // Helper methods for pressure mapping
    int32_t MapPressureByUnit(int32_t sensor_value, const std::string &unit);
    int32_t MapPSIPressure(int32_t sensor_value);
    int32_t MapkPaPressure(int32_t sensor_value);
    int32_t MapBarPressure(int32_t sensor_value);
    int32_t ClampValue(int32_t value, int32_t min_val, int32_t max_val);

    int32_t MapTemperatureValue(int32_t sensor_value);

    // ========== Static Methods ==========
    static void ShowPanelCompletionCallback(lv_event_t *event);
    static void UpdatePanelCompletionCallback(lv_anim_t *animation);
    static void ExecutePressureAnimationCallback(void *target, int32_t value);
    static void ExecuteTemperatureAnimationCallback(void *target, int32_t value);

    // ========== Private Data Members ==========
    IGpioProvider *gpio_provider_;
    IDisplayProvider *display_provider_;
    IStyleManager *style_manager_;
    IPanelManager *panel_manager_;
    IConfigurationManager *configuration_manager_ = nullptr;
    // Instance Data Members - UI Objects
    lv_obj_t *screen_ = nullptr;

    // Instance Data Members - Components and Sensors (static allocation)
    OemOilPressureComponent oem_oil_pressure_component_;
    OemOilTemperatureComponent oem_oil_temperature_component_;
    std::shared_ptr<ISensor> oem_oil_pressure_sensor_;
    std::shared_ptr<ISensor> oem_oil_temperature_sensor_;
    bool components_initialized_ = false;

    // Instance Data Members - State Variables
    int32_t current_oil_pressure_value_;
    int32_t current_oil_temperature_value_;

    // Hysteresis/Deadband configuration - Percentile-based
    // PRESSURE SCALE: 0-60 represents 0.0-6.0 Bar (one decimal place precision)
    // Example: value 55 = 5.5 Bar, value 23 = 2.3 Bar
    // TEMPERATURE SCALE: 0-120 represents 0-120°C
    // Deadband percentages (1%, 3%, 5%) are configurable and applied to component scales

    // Default deadband percentiles (can be overridden by configuration)
    static constexpr int32_t DEFAULT_PRESSURE_DEADBAND_PERCENT = 3;    // 3% of component scale range
    static constexpr int32_t DEFAULT_TEMPERATURE_DEADBAND_PERCENT = 3; // 3% of component scale range

    // Runtime deadband values (calculated from percentiles and scale ranges)
    int32_t pressure_deadband_;    // Calculated: (percent * PRESSURE_SCALE_RANGE) / 100
    int32_t temperature_deadband_; // Calculated: (percent * TEMPERATURE_SCALE_RANGE) / 100

    // Track last animated values for hysteresis
    int32_t last_animated_pressure_value_;
    int32_t last_animated_temperature_value_;

  private:
    // Helper methods for deadband calculation
    void CalculateDeadbands();

    String last_theme_; // Track last theme to force refresh when theme changes

    // Cache settings to avoid redundant updates
    int last_update_rate_ = -1; // Track last applied update rate
    String last_pressure_unit_; // Track last applied pressure unit
    String last_temp_unit_;     // Track last applied temperature unit

    // Cached sensor references to avoid repeated static_pointer_cast
    std::shared_ptr<OilPressureSensor> cached_pressure_sensor_;
    std::shared_ptr<OilTemperatureSensor> cached_temperature_sensor_;

    // Simplified animation state (single enum instead of multiple booleans)
    enum class AnimationState
    {
        IDLE,
        PRESSURE_RUNNING,
        TEMPERATURE_RUNNING,
        BOTH_RUNNING
    };
    AnimationState animation_state_ = AnimationState::IDLE;

    // Instance Data Members - Animation Objects
    lv_anim_t pressure_animation_; // Instance-level animation objects (prevents memory leaks)
    lv_anim_t temperature_animation_;
};