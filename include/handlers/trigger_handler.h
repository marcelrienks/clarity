#pragma once

#include "definitions/constants.h"
#include "definitions/types.h"
#include "interfaces/i_gpio_provider.h"
#include <memory>
#include <vector>

// Forward declarations for sensors
class BaseSensor;
class GpioSensor;

#include "esp32-hal-log.h"

/**
 * @class TriggerHandler
 * @brief Handles state-based triggers with GPIO sensor ownership and dual function system
 *
 * @details Processes triggers that monitor GPIO state changes and execute dual functions:
 * - activateFunc: Called when sensor transitions from inactive to active
 * - deactivateFunc: Called when sensor transitions from active to inactive
 *
 * @architecture Owns all GPIO sensors for exclusive state monitoring
 * @priority_system Implements priority-based blocking with same-type restoration
 * @memory_optimization Fixed-size trigger array for ESP32 safety
 */
class TriggerHandler
{
  public:
    // ========== Constructors and Destructor ==========
    TriggerHandler(IGpioProvider *gpio_provider);
    ~TriggerHandler();

    // ========== Public Interface Methods ==========
    // Main trigger processing method
    void ProcessTriggers();

    // Process initial trigger states after registration
    void ProcessInitialTriggerStates();

    // New Trigger system interface
    bool RegisterTrigger(const Trigger &trigger);

    // Sensor access for trigger context
    GpioSensor *GetKeyPresentSensor() const
    {
        return key_present_sensor_.get();
    }
    GpioSensor *GetKeyNotPresentSensor() const
    {
        return key_not_present_sensor_.get();
    }
    GpioSensor *GetLockSensor() const
    {
        return lock_sensor_.get();
    }
    GpioSensor *GetLightsSensor() const
    {
        return lights_sensor_.get();
    }
#ifdef CLARITY_DEBUG
    GpioSensor *GetDebugErrorSensor() const
    {
        return debug_error_sensor_.get();
    }
#endif

    // Status and diagnostics
    size_t GetTriggerCount() const;
    bool HasActiveTriggers() const;
    bool HasActivePanelTriggers() const; // Check if any PANEL type triggers are active
    // Find highest priority trigger of given type (public for InterruptManager)
    Trigger *FindHighestPrioritySameType(TriggerType type);

  private:
    // ========== Private Methods ==========
    void ValidateTriggers();       // Evaluate trigger state changes
    void ExecutePendingTriggers(); // Execute triggered actions

    // Core trigger processing
    void EvaluateIndividualTrigger(Trigger &trigger);
    void HandleTriggerActivation(Trigger &trigger);
    void HandleTriggerDeactivation(Trigger &trigger);
    void ExecuteTriggerFunction(const Trigger &trigger, bool is_activation);
    bool ShouldActivate(const Trigger &trigger) const;

    // Priority and blocking logic
    bool HasHigherPriorityActive(Priority priority) const;
    void UpdatePriorityState(Priority priority, bool active);

    // Helper methods
    Trigger *FindTrigger(const char *id);
    bool IsSensorActive(const Trigger &trigger) const;

    // ========== Private Data Members ==========
    static constexpr size_t MAX_TRIGGERS = 16;

    // Trigger storage
    Trigger triggers_[MAX_TRIGGERS];
    size_t trigger_count_ = 0;

    // Priority tracking for blocking logic
    bool priority_active_[3] = {false, false, false}; // CRITICAL, IMPORTANT, NORMAL

    // Handler-owned sensors for GPIO monitoring
    IGpioProvider *gpio_provider_;
    std::unique_ptr<GpioSensor> key_present_sensor_;
    std::unique_ptr<GpioSensor> key_not_present_sensor_;
    std::unique_ptr<GpioSensor> lock_sensor_;
    std::unique_ptr<GpioSensor> lights_sensor_;
#ifdef CLARITY_DEBUG
    std::unique_ptr<GpioSensor> debug_error_sensor_;
#endif
};