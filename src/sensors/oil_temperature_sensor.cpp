#include "sensors/oil_temperature_sensor.h"
#include "definitions/configs.h"
#include "definitions/constants.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include "utilities/unit_converter.h"
#include <Arduino.h>
#include <algorithm>
#include <esp32-hal-log.h>

// Self-registration at program startup
static bool oil_temperature_registered = []()
{
    ConfigurationManager::AddSchema(OilTemperatureSensor::RegisterConfigSchema);
    return true;
}();

// Constructors and Destructors

/**
 * @brief Constructs an oil temperature sensor with basic GPIO provider dependency
 * @param gpioProvider Pointer to GPIO provider interface for ADC reading
 * @param updateRateMs Update interval in milliseconds for sensor readings
 *
 * Initializes the sensor with GPIO provider for ADC operations and sets default
 * configuration values. Uses Celsius as the default temperature unit. This constructor
 * is used when no preference service is available for dynamic configuration.
 */
OilTemperatureSensor::OilTemperatureSensor(IGpioProvider *gpio_provider, int update_rate_ms)
    : gpio_provider_(gpio_provider), update_interval_ms_(update_rate_ms)
{
    log_v("OilTemperatureSensor() constructor called");
    // Set default unit to Celsius
    target_unit_ = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT;
    current_reading_ = 0;
}

/**
 * @brief Constructs an oil temperature sensor with preference service for dynamic configuration
 * @param gpioProvider Pointer to GPIO provider interface for ADC reading
 * @param configurationManager Pointer to preference service for configuration persistence
 * @param updateRateMs Default update interval in milliseconds for sensor readings
 *
 * Enhanced constructor that enables dynamic configuration through the preference service.
 * Allows for real-time calibration adjustments and unit changes. The preference service
 * enables live configuration updates and persistent storage of sensor settings.
 */
OilTemperatureSensor::OilTemperatureSensor(IGpioProvider *gpio_provider, IConfigurationManager *configuration_manager,
                                           int update_rate_ms)
    : gpio_provider_(gpio_provider), configuration_manager_(configuration_manager), update_interval_ms_(update_rate_ms)
{
    log_v("OilTemperatureSensor() constructor with preference service called");
    // Set default unit to Celsius
    target_unit_ = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT;
    current_reading_ = 0;
}

// Core Functionality Methods

/**
 * @brief Initialize the oil temperature sensor hardware and configuration
 *
 * Configures ESP32 ADC for 12-bit resolution with 11dB attenuation for 0-3.3V range.
 * Registers with dynamic configuration system if enabled and loads persisted settings.
 * Sets up live configuration callbacks for runtime updates.
 * Takes initial sensor reading to establish baseline for change detection.
 */
void OilTemperatureSensor::Init()
{
    log_v("Init() called");
    // Configure GPIO pin for analog input

    // Configure ADC resolution and attenuation for direct 3.3V operation
    analogReadResolution(12);       // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db); // 0-3.3V range

    // Load configuration (schema already registered at startup)
    if (configuration_manager_)
    {
        LoadConfiguration();
        log_i("OilTemperatureSensor initialized with configuration");
    }

    // Take initial reading to establish baseline
    GetReading();
}

/**
 * @brief Gets the list of supported temperature units
 * @return Vector of supported unit strings
 *
 * Returns the available temperature units that this sensor can convert to.
 * Supports both Celsius and Fahrenheit for international compatibility.
 * Used by the configuration system for unit selection validation.
 */
std::vector<std::string> OilTemperatureSensor::GetSupportedUnits() const
{
    log_v("GetSupportedUnits() called");
    return {config_constants::units::CELSIUS, config_constants::units::FAHRENHEIT};
}

/**
 * @brief Sets the target unit for temperature readings
 * @param unit Target temperature unit ("C" or "F")
 *
 * Validates the requested unit against supported units and updates the target
 * unit for future readings. Falls back to Celsius if an unsupported unit is
 * requested and reports a warning error. Used by both initialization and live
 * configuration updates to change the display unit.
 */
void OilTemperatureSensor::SetTargetUnit(const std::string &unit)
{
    log_v("SetTargetUnit() called");
    // Validate unit is supported
    auto supported_units = GetSupportedUnits();
    if (std::find(supported_units.begin(), supported_units.end(), unit) == supported_units.end())
    {
        // Use static string to avoid allocation in error path
        static const char *error_msg = "Unsupported temperature unit requested. Using default C.";
        ErrorManager::Instance().ReportError(ErrorLevel::WARNING, "OilTemperatureSensor", error_msg);
        target_unit_ = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT;
    }
    else
    {
        target_unit_ = unit;
    }
}

/**
 * @brief Gets the current temperature reading in the configured unit
 * @return Reading containing temperature value or invalid reading on error
 *
 * Performs rate-limited ADC sampling, validates the raw reading, and converts
 * to the target temperature unit. Includes change detection and error handling
 * for out-of-range ADC values. Updates the cached reading only when the value
 * changes to minimize unnecessary processing. This is the main interface method
 * for obtaining temperature measurements.
 */
Reading OilTemperatureSensor::GetReading()
{
    // Check if enough time has passed for update
    if (ShouldUpdate(last_update_time_, update_interval_ms_))
    {
        // Read raw value from ADC
        int32_t raw_value = ReadRawValue();

        // Validate ADC reading
        if (!IsValidAdcReading(raw_value))
        {
            // Use char buffer to avoid std::to_string allocation in error path
            static char error_buffer[64];
            snprintf(error_buffer, sizeof(error_buffer), "Raw reading out of range: %ld", static_cast<long>(raw_value));
            ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "OilTemperatureSensor", error_buffer);
            return std::monostate{}; // Return invalid reading
        }

        // Convert to target unit
        int32_t new_value = ConvertReading(raw_value);

        // Update change tracking
        DetectChange(new_value, previous_reading_);

        // Only update current reading if value actually changed
        if (new_value != current_reading_)
        {
            current_reading_ = new_value;
            log_t("Temperature reading: %d %s", current_reading_, target_unit_.c_str());
        }
    }

    return current_reading_;
}

/**
 * @brief Sets the update rate for sensor readings
 * @param updateRateMs Update interval in milliseconds
 *
 * Updates the sensor's reading interval to control the frequency of ADC sampling.
 * Lower values provide more responsive readings but increase CPU usage.
 * Used by both initialization and live configuration updates.
 */
void OilTemperatureSensor::SetUpdateRate(int update_rate_ms)
{
    update_interval_ms_ = update_rate_ms;
}

// Internal methods

/**
 * @brief Reads raw ADC value from the temperature sensor
 * @return 12-bit ADC value (0-4095) representing analog voltage
 *
 * Performs a direct ADC read from the oil temperature sensor GPIO pin.
 * The reading represents the analog voltage from the temperature sensor
 * before any calibration or unit conversion is applied.
 */
int32_t OilTemperatureSensor::ReadRawValue()
{
    // Read analog value from GPIO pin (0-4095 for 12-bit ADC)
    return gpio_provider_->AnalogRead(gpio_pins::OIL_TEMPERATURE);
}

/**
 * @brief Converts raw ADC value to requested temperature unit with calibration
 * @param rawValue Raw 12-bit ADC value (0-4095)
 * @return Calibrated temperature value in the target unit
 *
 * Applies calibration scale and offset to the raw ADC reading, then converts
 * to the requested temperature unit. The base calibration maps 0-4095 ADC to
 * 0-120°C. All values are stored internally as Celsius and converted to the
 * requested unit for display.
 */
int32_t OilTemperatureSensor::ConvertReading(int32_t raw_value)
{
    // Apply calibration
    float calibrated_value = static_cast<float>(raw_value);
    calibrated_value = (calibrated_value * calibration_scale_) + calibration_offset_;

    // Convert calibrated ADC value to Celsius (base unit)
    // Base calibration: 0-4095 ADC = 0-120°C
    float celsius_value = (calibrated_value * sensor_constants::TEMPERATURE_MAX_CELSIUS) / ADC_MAX_VALUE;

    // Convert from base unit (Celsius) to target unit
    if (target_unit_ == config_constants::units::FAHRENHEIT)
    {
        return static_cast<int32_t>(unit_converter::CelsiusToFahrenheit(celsius_value));
    }
    else
    {
        // Already in Celsius
        return static_cast<int32_t>(celsius_value);
    }
}

/**
 * @brief Checks if sensor state has changed since last evaluation
 * @return true if temperature reading has changed significantly
 *
 * Uses the cached reading to avoid double ADC reads, as this method is called
 * during interrupt evaluation while GetReading() is called during the main loop.
 * Implements change detection for the interrupt system to trigger appropriate
 * responses to temperature changes.
 */
bool OilTemperatureSensor::HasStateChanged()
{
    // Use cached reading to avoid double ADC reads - GetReading() updates current_reading_
    int32_t cached_reading = static_cast<int32_t>(std::get<int32_t>(GetReading()));
    return DetectChange(cached_reading, previous_change_reading_);
}

/**
 * @brief Loads configuration from the preference system
 *
 * Retrieves saved configuration values from the preference service including
 * target unit, update rate, and calibration parameters. Provides sensible
 * defaults for any missing configuration values. This enables persistent
 * configuration storage and restoration across system restarts.
 */
void OilTemperatureSensor::LoadConfiguration()
{
    if (!configuration_manager_)
    {
        log_e("OilTemperatureSensor::LoadConfiguration: ConfigurationManager is null - cannot load sensor "
              "configuration!");
        ErrorManager::Instance().ReportCriticalError("OilTemperatureSensor",
                                                     "ConfigManager null - using default settings");
        return;
    }

    // Load using type-safe config system with static constants
    if (auto unit_value = configuration_manager_->QueryConfig<std::string>(CONFIG_UNIT))
    {
        target_unit_ = *unit_value;
    }
    else
    {
        target_unit_ = config_constants::defaults::DEFAULT_TEMPERATURE_UNIT;
    }

    if (auto rate_value = configuration_manager_->QueryConfig<int>(CONFIG_UPDATE_RATE))
    {
        update_interval_ms_ = *rate_value;
    }
    else
    {
        update_interval_ms_ = 500;
    }

    if (auto offset_value = configuration_manager_->QueryConfig<float>(CONFIG_OFFSET))
    {
        calibration_offset_ = *offset_value;
    }
    else
    {
        calibration_offset_ = 0.0f;
    }

    if (auto scale_value = configuration_manager_->QueryConfig<float>(CONFIG_SCALE))
    {
        calibration_scale_ = *scale_value;
    }
    else
    {
        calibration_scale_ = 1.0f;
    }

    log_i("Loaded oil temperature sensor configuration: unit=%s, rate=%lu, offset=%.2f, scale=%.2f",
          target_unit_.c_str(), update_interval_ms_, calibration_offset_, calibration_scale_);
}

/**
 * @brief Static method to register configuration schema without instance
 * @param configurationManager Service to register schema with
 *
 * Called automatically at program startup through ConfigRegistry.
 * Registers the oil temperature sensor configuration schema without
 * requiring a sensor instance to exist.
 */
void OilTemperatureSensor::RegisterConfigSchema(IConfigurationManager *configuration_manager)
{
    if (!configuration_manager)
    {
        log_e("OilTemperatureSensor::RegisterConfigSchema: ConfigurationManager is null - sensor config registration "
              "failed!");
        ErrorManager::Instance().ReportCriticalError("OilTemperatureSensor",
                                                     "ConfigManager null - temp sensor config failed");
        return;
    }

    // Check if already registered to prevent duplicates
    if (configuration_manager->IsSchemaRegistered(CONFIG_SECTION))
    {
        return;
    }

    using namespace config;

    // Create configuration section for this sensor component
    ConfigSection section(config_constants::sections::OIL_TEMPERATURE_SENSOR, CONFIG_SECTION,
                          config_constants::section_names::OIL_TEMPERATURE_SENSOR);

    section.AddItem(unit_config);
    section.AddItem(update_rate_config);
    section.AddItem(offset_config);
    section.AddItem(scale_config);
    section.AddItem(deadband_config);

    // Register with preference service for persistence and UI generation
    configuration_manager->RegisterConfigSection(section);
    log_i("Registered oil temperature sensor configuration schema (static)");
}

/**
 * @brief Instance method for backward compatibility during migration
 * @param configurationManager Service to register schema with
 *
 * This method maintains backward compatibility during migration.
 * New code path uses static RegisterConfigSchema instead.
 * Can be removed once all components are migrated.
 */
void OilTemperatureSensor::RegisterConfig(IConfigurationManager *configuration_manager)
{
    // During migration, just delegate to static method
    RegisterConfigSchema(configuration_manager);
}
