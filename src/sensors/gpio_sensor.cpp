#include "sensors/gpio_sensor.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include "definitions/constants.h"
#include <Arduino.h>
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs a GPIO sensor with configuration and provider dependencies
 * @param config Sensor configuration including pin, mode, and behavior settings
 * @param gpioProvider Pointer to GPIO provider interface for hardware abstraction
 *
 * Initializes the sensor with the provided configuration and GPIO provider.
 * Constructor is kept lightweight - actual hardware initialization occurs in Init().
 * This follows the dependency injection pattern for hardware abstraction.
 */
GpioSensor::GpioSensor(const GpioSensorConfig& config, IGpioProvider* gpioProvider)
    : config_(config)
    , gpioProvider_(gpioProvider)
    , previousState_(false)
{
    // Constructor should be lightweight - initialization happens in Init()
}

/**
 * @brief Destructor that cleans up GPIO resources
 *
 * Detaches GPIO interrupts for key sensors to prevent dangling interrupt
 * handlers. Only key present/not present sensors use interrupts in the
 * original implementation. Ensures proper cleanup of hardware resources.
 */
GpioSensor::~GpioSensor()
{
    if (gpioProvider_ && initialized_) {
        // Clean up GPIO interrupt if attached
        // Note: Only key sensors used interrupts in original implementation
        if (config_.pin == gpio_pins::KEY_PRESENT || config_.pin == gpio_pins::KEY_NOT_PRESENT) {
            gpioProvider_->DetachInterrupt(config_.pin);
        }
    }
}

// ========== ISensor Implementation ==========

/**
 * @brief Initializes the GPIO sensor hardware configuration
 *
 * Configures the GPIO pin according to the sensor configuration and reads
 * the initial state. Handles debug-only sensors by checking build configuration.
 * Sets the previous state to prevent false change detection on first read.
 * This is part of the ISensor interface implementation.
 */
void GpioSensor::Init()
{
    if (!gpioProvider_) {
        log_e("%s: GPIO provider is null", config_.name);
        return;
    }

    // Check debug-only sensor availability
    #ifndef CLARITY_DEBUG
    if (config_.debugOnly) {
        log_w("%s: Debug-only sensor not available in release build", config_.name);
        initialized_ = false;
        return;
    }
    #endif

    // Configure GPIO pin based on configuration
    gpioProvider_->PinMode(config_.pin, config_.pinMode);

    // Initialize the sensor state to avoid false change detection
    previousState_ = readLogicalState();
    initialized_ = true;

    log_i("%s initialization completed on GPIO %d, mode: %d, initial state: %s",
          config_.name, config_.pin, config_.pinMode,
          previousState_ ? "ACTIVE" : "INACTIVE");
}

/**
 * @brief Gets the current sensor reading as a Reading object
 * @return Reading containing the current logical state as a boolean
 *
 * Reads the current logical state and wraps it in a Reading object for
 * compatibility with the ISensor interface. The logical state accounts
 * for the activeHigh configuration setting.
 */
Reading GpioSensor::GetReading()
{
    bool currentState = readLogicalState();
    return Reading(currentState);
}

/**
 * @brief Gets the current logical state of the sensor
 * @return true if sensor is in active state, false otherwise
 *
 * Returns the current logical state accounting for the activeHigh configuration.
 * This provides direct boolean access to the sensor state without wrapping
 * in a Reading object.
 */
bool GpioSensor::GetState()
{
    return readLogicalState();
}

/**
 * @brief Checks if the sensor state has changed since last check
 * @return true if state has changed, false otherwise
 *
 * Compares current state with previous state and updates the previous state
 * if a change is detected. Handles debug-only sensors by returning false
 * in release builds. Includes detailed logging for test automation and
 * special case logging for key and light sensors to maintain compatibility
 * with existing test expectations.
 */
bool GpioSensor::HasStateChanged()
{
    if (!initialized_) {
        log_w("%s: HasStateChanged called before initialization", config_.name);
        return false;
    }

    // Check debug-only sensor availability at runtime
    #ifndef CLARITY_DEBUG
    if (config_.debugOnly) {
        return false; // Debug sensors don't change in release builds
    }
    #endif

    bool currentState = readLogicalState();
    bool oldState = previousState_; // Save BEFORE DetectChange modifies it

    // Per-poll logging floods the serial port (~96% of test output), so it is opt-in
    #ifdef GPIO_POLL_LOGS
    log_t("%s GPIO %d poll: current=%s, previous=%s",
          config_.name, config_.pin,
          readRawPinState() ? "HIGH" : "LOW",
          oldState ? "ACTIVE" : "INACTIVE");
    #endif

    bool changed = DetectChange(currentState, previousState_);

    if (changed) {
        log_t("%s state changed: %s -> %s",
              config_.name,
              oldState ? "ACTIVE" : "INACTIVE",
              currentState ? "ACTIVE" : "INACTIVE");

        // Special case logging for key sensors to maintain compatibility
        if (config_.pin == gpio_pins::KEY_PRESENT) {
            log_t("KeyPresentSensor state changed: %s -> %s",
                  oldState ? "PRESENT" : "NOT_PRESENT",
                  currentState ? "PRESENT" : "NOT_PRESENT");
        }
        else if (config_.pin == gpio_pins::KEY_NOT_PRESENT) {
            log_t("KeyNotPresentSensor state changed: %s -> %s",
                  oldState ? "NOT_PRESENT" : "PRESENT",
                  currentState ? "NOT_PRESENT" : "PRESENT");
        }
        else if (config_.pin == gpio_pins::LIGHTS) {
            log_t("Lights sensor state changed: %s -> %s",
                  oldState ? "ON" : "OFF",
                  currentState ? "ON" : "OFF");
        }
    }

    return changed;
}

/**
 * @brief Handles GPIO interrupt events for this sensor
 *
 * Called when a hardware interrupt is triggered on this sensor's GPIO pin.
 * Provides sensor-specific handling for different pin types including lights
 * (for theme switching) and debug error button (for error generation).
 * Logs the interrupt event and current state for debugging purposes.
 */
void GpioSensor::OnInterruptTriggered()
{
    log_t("%s interrupt triggered - state changed", config_.name);

    // Sensor-specific interrupt handling based on pin
    if (config_.pin == gpio_pins::LIGHTS) {
        bool currentState = readLogicalState();
        if (currentState) {
            log_t("Lights turned on - system could switch to night theme");
        } else {
            log_t("Lights turned off - system could switch to day theme");
        }
    }
    else if (config_.pin == gpio_pins::DEBUG_ERROR) {
        #ifdef CLARITY_DEBUG
        log_i("Debug error button interrupt detected - error generation handled by trigger system");
        #endif
    }
    else {
        // Generic interrupt handling for other sensors
        bool currentState = readLogicalState();
        log_t("%s current state: %s", config_.name, currentState ? "ACTIVE" : "INACTIVE");
    }
}

/**
 * @brief Reads the raw GPIO pin state without logical interpretation
 * @return true if pin is HIGH, false if LOW or on error
 *
 * Performs a direct GPIO read through the provider interface with null
 * pointer safety check. Returns the raw electrical state without considering
 * the activeHigh configuration setting.
 */
bool GpioSensor::readRawPinState()
{
    if (!gpioProvider_) {
        log_e("%s: GPIO provider is null - sensor cannot function!", config_.name);
        ErrorManager::Instance().ReportCriticalError("GpioSensor",
                                                     "GPIO provider is null - sensor cannot read pins");
        return false;
    }

    // Read raw GPIO pin state
    return gpioProvider_->DigitalRead(config_.pin);
}

/**
 * @brief Reads the logical sensor state accounting for activeHigh configuration
 * @return true if sensor is logically active, false otherwise
 *
 * Converts the raw GPIO state to logical state based on the activeHigh
 * configuration. If activeHigh is true, HIGH means active. If activeHigh
 * is false, LOW means active (inverted logic for sensors with pullup resistors).
 */
bool GpioSensor::readLogicalState()
{
    bool rawState = readRawPinState();

    // Convert raw state to logical state based on activeHigh configuration
    return config_.activeHigh ? rawState : !rawState;
}