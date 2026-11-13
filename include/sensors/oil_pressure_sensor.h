#pragma once

// System/Library Includes
#include <string>
#include <vector>

// Project Includes
#include "definitions/configs.h"
#include "definitions/constants.h"
#include "definitions/types.h"
#include "hardware/gpio_pins.h"
#include "interfaces/i_config.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_gpio_provider.h"
#include "sensors/base_sensor.h"

/**
 * @class OilPressureSensor
 * @brief Oil pressure monitoring sensor with unit-aware conversions
 *
 * @details This sensor monitors engine oil pressure levels and provides
 * readings in the unit requested by the consuming panel/component.
 * It inherits delta-based updates and time-based sampling from UnitAwareSensor.
 *
 * @model_role Provides oil pressure data to OemOilPressureComponent
 * @supported_units: Bar, PSI, kPa
 * @range 0-10 Bar (0-145 PSI, 0-1000 kPa) typical automotive range
 * @display_scale Component expects 0-60 scale representing 0.0-6.0 Bar
 *               Sensor readings are mapped: actual_bar * 10 = display_value
 *               Example: 2.5 Bar reading becomes value 25 for display
 * @update_frequency Configurable via constructor
 *
 * @unit_conversion:
 * - Bar: Direct mapping 0-10 Bar
 * - PSI: 0-145 PSI (14.5 PSI per Bar)
 * - kPa: 0-1000 kPa (100 kPa per Bar)
 *
 * @hardware_interface 3.3V analog pressure sensor input via GPIO pin
 * @calibration 22k potentiometer: 0V = 0 Bar, 3.3V = 10 Bar
 *
 * @context This sensor provides pressure readings in the unit requested
 * by the panel, removing unit conversion logic from display components.
 */
class OilPressureSensor : public BaseSensor, public IConfig
{
  public:
    // ========== Constructors and Destructor ==========
    OilPressureSensor(IGpioProvider *gpio_provider, int update_rate_ms = 500);
    OilPressureSensor(IGpioProvider *gpio_provider, IConfigurationManager *configuration_manager,
                      int update_rate_ms = 500);

    // ========== Public Interface Methods ==========
    // BaseSensor interface implementation
    void Init() override;
    Reading GetReading() override;
    bool HasStateChanged() override;

    // ISensor optional methods
    void SetTargetUnit(const std::string &unit) override;
    std::vector<std::string> GetSupportedUnits() const override;

    void SetUpdateRate(int update_rate_ms);

    void LoadConfiguration();

    // IConfig implementation (instance method for backward compatibility)
    void RegisterConfig(IConfigurationManager *configuration_manager) override;

    // Static schema registration for self-registering pattern
    static void RegisterConfigSchema(IConfigurationManager *configuration_manager);

    // ========== Configuration Constants ==========
    static constexpr const char *CONFIG_SECTION = config_constants::sections::OIL_PRESSURE_SENSOR;
    static constexpr const char *CONFIG_UNIT = config_constants::keys::OIL_PRESSURE_UNIT;
    static constexpr const char *CONFIG_UPDATE_RATE = config_constants::keys::OIL_PRESSURE_UPDATE_RATE;
    static constexpr const char *CONFIG_OFFSET = config_constants::keys::OIL_PRESSURE_OFFSET;
    static constexpr const char *CONFIG_SCALE = config_constants::keys::OIL_PRESSURE_SCALE;

  protected:
    // ========== Protected Methods ==========
    int32_t ReadRawValue();
    int32_t ConvertReading(int32_t raw_value);

  private:
    // ========== Configuration Items (inline definitions) ==========
    inline static config::ConfigItem unit_config{
        config_constants::items::UNIT, ui_strings::config_labels::PRESSURE_UNIT,
        std::string(config_constants::defaults::DEFAULT_PRESSURE_UNIT),
        config::ConfigMetadata(config_constants::options::PRESSURE_UNITS, config::ConfigItemType::Selection)};
    inline static config::ConfigItem update_rate_config{
        config_constants::items::UPDATE_RATE, ui_strings::config_labels::UPDATE_RATE_MS,
        config_constants::defaults::DEFAULT_UPDATE_RATE,
        config::ConfigMetadata(config_constants::options::UPDATE_RATES, config::ConfigItemType::Selection)};
    inline static config::ConfigItem offset_config{
        config_constants::items::OFFSET, ui_strings::config_labels::CALIBRATION_OFFSET,
        config_constants::defaults::DEFAULT_CALIBRATION_OFFSET,
        config::ConfigMetadata(config_constants::options::CALIBRATION_OFFSETS, config::ConfigItemType::Selection)};
    inline static config::ConfigItem scale_config{
        config_constants::items::SCALE, ui_strings::config_labels::CALIBRATION_SCALE,
        config_constants::defaults::DEFAULT_CALIBRATION_SCALE,
        config::ConfigMetadata(config_constants::options::CALIBRATION_SCALES, config::ConfigItemType::Selection)};

    // Display behavior configuration (deadband for animation filtering)
    inline static config::ConfigItem deadband_config{
        config_constants::keys::OIL_PRESSURE_DEADBAND_PERCENT, "Pressure Display Deadband",
        3, // Default 3%
        config::ConfigMetadata{"1,3,5", config::ConfigItemType::Selection}};

    // ========== Private Data Members ==========
    IGpioProvider *gpio_provider_;
    IConfigurationManager *configuration_manager_ = nullptr;
    float calibration_offset_ = 0.0f;
    float calibration_scale_ = 1.0f;
    std::string target_unit_ = "Bar";
    int32_t current_reading_ = 0;
    int32_t previous_reading_ = 0;        // For GetReading() change tracking
    int32_t previous_change_reading_ = 0; // For HasStateChanged() separate tracking
    unsigned long last_update_time_ = 0;
    unsigned long update_interval_ms_;
};