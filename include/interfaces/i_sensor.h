#pragma once

// System/Library Includes
#include <string>
#include <vector>

// Project Includes
#include "definitions/types.h"

/**
 * @interface ISensor
 * @brief Base interface for all sensor implementations in the Clarity system
 *
 * @details This interface defines the contract for sensors that acquire data
 * from hardware inputs or simulated sources. Sensors represent the Model layer
 * in the MVP architecture, providing data to panels and components.
 *
 * @design_pattern Model in MVP - handles data acquisition and processing
 * @data_flow:
 * 1. init(): Initialize sensor hardware/configuration
 * 2. setTargetUnit(): Set the unit of measure for readings (optional)
 * 3. getReading(): Acquire current sensor value as Reading variant
 *
 * @reading_types:
 * - int32_t: Numeric values (pressure, temperature)
 * - bool: Boolean states (key presence, lock status)
 * - double: Precise measurements
 * - std::string: Status messages or text data
 * - std::monostate: Invalid/uninitialized readings
 *
 * @unit_handling:
 * - Sensors supporting units implement SetTargetUnit()
 * - Unit conversion happens within the sensor
 * - Readings are returned in the requested unit
 *
 * @hardware_abstraction:
 * - Sensors abstract GPIO/ADC access via IGpioProvider
 * - Support both real hardware and simulation modes
 * - Consistent Reading interface regardless of data source
 *
 * @implementations:
 * - KeyPresentSensor/KeyNotPresentSensor: Split key presence detection
 * - LockSensor: Vehicle lock status monitoring
 * - LightSensor: Ambient light level detection
 * - OilPressureSensor: Engine oil pressure monitoring
 * - OilTemperatureSensor: Engine oil temperature monitoring
 *
 * @context This is the base interface for all sensors.
 * Sensors provide data to panels which then update their components
 * for display to the user.
 */
class ISensor
{
  public:
    virtual ~ISensor() = default;

    // Core Interface Methods
    virtual void Init() = 0;
    virtual Reading GetReading() = 0;

    // Optional Unit Support
    virtual void SetTargetUnit(const std::string &unit)
    {
    }

    virtual std::vector<std::string> GetSupportedUnits() const
    {
        return {};
    }
};