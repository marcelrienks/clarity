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
 * @class OilTemperatureSensor
 * @brief Oil temperature monitoring sensor with unit-aware conversions
 *
 * @details This sensor monitors engine oil temperature levels and provides
 * readings in the unit requested by the consuming panel/component.
 * It inherits delta-based updates and time-based sampling from UnitAwareSensor.
 *
 * @model_role Provides oil temperature data to OemOilTemperatureComponent
 * @supported_units: C (Celsius), F (Fahrenheit)
 * @range 0-120°C (32-248°F) typical automotive range
 * @update_frequency Configurable via constructor
 *
 * @unit_conversion:
 * - C: Direct mapping 0-120°C
 * - F: 32-248°F (C * 9/5 + 32)
 *
 * @hardware_interface 3.3V analog temperature sensor input via GPIO pin
 * @calibration 22k potentiometer: 0V = 0°C, 3.3V = 120°C
 *
 * @context This sensor provides temperature readings in the unit requested
 * by the panel, removing unit conversion logic from display components.
 */
class OilTemperatureSensor : public BaseSensor, public IConfig
{
  public:
    // ========== Constructors and Destructor ==========
    OilTemperatureSensor(IGpioProvider *gpio_provider, int update_rate_ms = 500);
    OilTemperatureSensor(IGpioProvider *gpio_provider, IConfigurationManager *configuration_manager,
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
    static constexpr const char *CONFIG_SECTION = config_constants::sections::OIL_TEMPERATURE_SENSOR;
    static constexpr const char *CONFIG_UNIT = config_constants::keys::OIL_TEMPERATURE_UNIT;
    static constexpr const char *CONFIG_UPDATE_RATE = config_constants::keys::OIL_TEMPERATURE_UPDATE_RATE;
    static constexpr const char *CONFIG_OFFSET = config_constants::keys::OIL_TEMPERATURE_OFFSET;
    static constexpr const char *CONFIG_SCALE = config_constants::keys::OIL_TEMPERATURE_SCALE;

  protected:
    // ========== Protected Methods ==========
    int32_t ReadRawValue();
    int32_t ConvertReading(int32_t raw_value);

  private:
    // ========== Configuration Items (inline definitions) ==========
    inline static config::ConfigItem unit_config{
        config_constants::items::UNIT, ui_strings::config_labels::TEMPERATURE_UNIT,
        std::string(config_constants::defaults::DEFAULT_TEMPERATURE_UNIT),
        config::ConfigMetadata(config_constants::options::TEMPERATURE_UNITS, config::ConfigItemType::Selection)};
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
        config_constants::keys::OIL_TEMPERATURE_DEADBAND_PERCENT, "Temperature Display Deadband",
        3, // Default 3%
        config::ConfigMetadata{"1,3,5", config::ConfigItemType::Selection}};

    // ========== Private Data Members ==========
    IGpioProvider *gpio_provider_;
    IConfigurationManager *configuration_manager_ = nullptr;
    float calibration_offset_ = 0.0f;
    float calibration_scale_ = 1.0f;
    std::string target_unit_ = "C";
    int32_t current_reading_ = 0;
    int32_t previous_reading_ = 0;        // For GetReading() change tracking
    int32_t previous_change_reading_ = 0; // For HasStateChanged() separate tracking
    unsigned long last_update_time_ = 0;
    unsigned long update_interval_ms_;
};