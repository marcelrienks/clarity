#include "sensors/oil_temperature_sensor.h"
#include "managers/error_manager.h"
#include "definitions/configs.h"
#include "utilities/logging.h"
#include "definitions/constants.h"
#include "utilities/unit_converter.h"
#include "managers/configuration_manager.h"
#include <Arduino.h>
#include <esp32-hal-log.h>
#include <algorithm>

// Self-registration at program startup
static bool oil_temperature_registered = []() {
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
OilTemperatureSensor::OilTemperatureSensor(IGpioProvider *gpioProvider, int updateRateMs)
    : gpioProvider_(gpioProvider), updateIntervalMs_(updateRateMs)
{
    log_v("OilTemperatureSensor() constructor called");
    // Set default unit to Celsius
    targetUnit_ = ConfigConstants::Defaults::DEFAULT_TEMPERATURE_UNIT;
    currentReading_ = 0;
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
OilTemperatureSensor::OilTemperatureSensor(IGpioProvider *gpioProvider, IConfigurationManager *configurationManager, int updateRateMs)
    : gpioProvider_(gpioProvider), configurationManager_(configurationManager), updateIntervalMs_(updateRateMs)
{
    log_v("OilTemperatureSensor() constructor with preference service called");
    // Set default unit to Celsius
    targetUnit_ = ConfigConstants::Defaults::DEFAULT_TEMPERATURE_UNIT;
    currentReading_ = 0;
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
    if (configurationManager_) {
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
    return {ConfigConstants::Units::CELSIUS, ConfigConstants::Units::FAHRENHEIT};
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
    auto supportedUnits = GetSupportedUnits();
    if (std::find(supportedUnits.begin(), supportedUnits.end(), unit) == supportedUnits.end())
    {
        // Use static string to avoid allocation in error path
        static const char* errorMsg = "Unsupported temperature unit requested. Using default C.";
        ErrorManager::Instance().ReportError(ErrorLevel::WARNING, "OilTemperatureSensor", errorMsg);
        targetUnit_ = ConfigConstants::Defaults::DEFAULT_TEMPERATURE_UNIT;
    }
    else
    {
        targetUnit_ = unit;
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
    if (ShouldUpdate(lastUpdateTime_, updateIntervalMs_))
    {
        // Read raw value from ADC
        int32_t rawValue = ReadRawValue();

        // Validate ADC reading
        if (!IsValidAdcReading(rawValue))
        {
            // Use char buffer to avoid std::to_string allocation in error path
            static char errorBuffer[64];
            snprintf(errorBuffer, sizeof(errorBuffer), "Raw reading out of range: %ld", static_cast<long>(rawValue));
            ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "OilTemperatureSensor", errorBuffer);
            return std::monostate{}; // Return invalid reading
        }

        // Convert to target unit
        int32_t newValue = ConvertReading(rawValue);

        // Update change tracking
        DetectChange(newValue, previousReading_);

        // Only update current reading if value actually changed
        if (newValue != currentReading_)
        {
            currentReading_ = newValue;
            log_t("Temperature reading: %d %s", currentReading_, targetUnit_.c_str());
        }
    }

    return currentReading_;
}

/**
 * @brief Sets the update rate for sensor readings
 * @param updateRateMs Update interval in milliseconds
 *
 * Updates the sensor's reading interval to control the frequency of ADC sampling.
 * Lower values provide more responsive readings but increase CPU usage.
 * Used by both initialization and live configuration updates.
 */
void OilTemperatureSensor::SetUpdateRate(int updateRateMs)
{
    updateIntervalMs_ = updateRateMs;
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
    return gpioProvider_->AnalogRead(gpio_pins::OIL_TEMPERATURE);
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
int32_t OilTemperatureSensor::ConvertReading(int32_t rawValue)
{
    // Apply calibration
    float calibratedValue = static_cast<float>(rawValue);
    calibratedValue = (calibratedValue * calibrationScale_) + calibrationOffset_;

    // Convert calibrated ADC value to Celsius (base unit)
    // Base calibration: 0-4095 ADC = 0-120°C
    float celsiusValue = (calibratedValue * SensorConstants::TEMPERATURE_MAX_CELSIUS) / ADC_MAX_VALUE;

    // Convert from base unit (Celsius) to target unit
    if (targetUnit_ == ConfigConstants::Units::FAHRENHEIT)
    {
        return static_cast<int32_t>(UnitConverter::CelsiusToFahrenheit(celsiusValue));
    }
    else
    {
        // Already in Celsius
        return static_cast<int32_t>(celsiusValue);
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
    // Use cached reading to avoid double ADC reads - GetReading() updates currentReading_
    int32_t cachedReading = static_cast<int32_t>(std::get<int32_t>(GetReading()));
    return DetectChange(cachedReading, previousChangeReading_);
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
    if (!configurationManager_) {
        log_e("OilTemperatureSensor::LoadConfiguration: ConfigurationManager is null - cannot load sensor configuration!");
        ErrorManager::Instance().ReportCriticalError("OilTemperatureSensor",
                                                     "ConfigManager null - using default settings");
        return;
    }

    // Load using type-safe config system with static constants
    if (auto unitValue = configurationManager_->QueryConfig<std::string>(CONFIG_UNIT)) {
        targetUnit_ = *unitValue;
    } else {
        targetUnit_ = ConfigConstants::Defaults::DEFAULT_TEMPERATURE_UNIT;
    }

    if (auto rateValue = configurationManager_->QueryConfig<int>(CONFIG_UPDATE_RATE)) {
        updateIntervalMs_ = *rateValue;
    } else {
        updateIntervalMs_ = 500;
    }

    if (auto offsetValue = configurationManager_->QueryConfig<float>(CONFIG_OFFSET)) {
        calibrationOffset_ = *offsetValue;
    } else {
        calibrationOffset_ = 0.0f;
    }

    if (auto scaleValue = configurationManager_->QueryConfig<float>(CONFIG_SCALE)) {
        calibrationScale_ = *scaleValue;
    } else {
        calibrationScale_ = 1.0f;
    }

    log_i("Loaded oil temperature sensor configuration: unit=%s, rate=%lu, offset=%.2f, scale=%.2f",
          targetUnit_.c_str(), updateIntervalMs_, calibrationOffset_, calibrationScale_);
}

/**
 * @brief Static method to register configuration schema without instance
 * @param configurationManager Service to register schema with
 *
 * Called automatically at program startup through ConfigRegistry.
 * Registers the oil temperature sensor configuration schema without
 * requiring a sensor instance to exist.
 */
void OilTemperatureSensor::RegisterConfigSchema(IConfigurationManager* configurationManager)
{
    if (!configurationManager) {
        log_e("OilTemperatureSensor::RegisterConfigSchema: ConfigurationManager is null - sensor config registration failed!");
        ErrorManager::Instance().ReportCriticalError("OilTemperatureSensor",
                                                     "ConfigManager null - temp sensor config failed");
        return;
    }

    // Check if already registered to prevent duplicates
    if (configurationManager->IsSchemaRegistered(CONFIG_SECTION)) {
        return;
    }

    using namespace Config;

    // Create configuration section for this sensor component
    ConfigSection section(ConfigConstants::Sections::OIL_TEMPERATURE_SENSOR, CONFIG_SECTION, ConfigConstants::SectionNames::OIL_TEMPERATURE_SENSOR);

    section.AddItem(unitConfig_);
    section.AddItem(updateRateConfig_);
    section.AddItem(offsetConfig_);
    section.AddItem(scaleConfig_);
    section.AddItem(deadbandConfig_);

    // Register with preference service for persistence and UI generation
    configurationManager->RegisterConfigSection(section);
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
void OilTemperatureSensor::RegisterConfig(IConfigurationManager* configurationManager)
{
    // During migration, just delegate to static method
    RegisterConfigSchema(configurationManager);
}
