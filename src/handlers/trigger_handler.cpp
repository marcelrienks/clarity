#include "handlers/trigger_handler.h"
#include "definitions/constants.h"
#include "hardware/gpio_pins.h"
#include "managers/error_manager.h"
#include "sensors/gpio_sensor.h"
#include "utilities/logging.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>

#include "esp32-hal-log.h"

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs TriggerHandler with GPIO provider and initializes all sensors
 * @param gpioProvider GPIO provider for hardware sensor access
 *
 * Creates TriggerHandler instance that owns and manages multiple GPIO sensors
 * for monitoring vehicle state changes. Initializes priority tracking system
 * and creates sensors for key presence, lock status, lights, and debug error.
 */
TriggerHandler::TriggerHandler(IGpioProvider *gpio_provider) : trigger_count_(0), gpio_provider_(gpio_provider)
{
    // Initialize priority tracking
    for (int i = 0; i < 3; i++)
    {
        priority_active_[i] = false;
    }

    // Create and initialize GPIO sensors owned by this handler
    if (gpio_provider_)
    {

        // Create GPIO sensors using generic GpioSensor with predefined configurations
        key_present_sensor_ = std::make_unique<GpioSensor>(sensor_configs::KEY_PRESENT, gpio_provider_);
        key_not_present_sensor_ = std::make_unique<GpioSensor>(sensor_configs::KEY_NOT_PRESENT, gpio_provider_);
        lock_sensor_ = std::make_unique<GpioSensor>(sensor_configs::LOCK, gpio_provider_);
        lights_sensor_ = std::make_unique<GpioSensor>(sensor_configs::LIGHTS, gpio_provider_);

#ifdef CLARITY_DEBUG
        // Create debug error sensor (debug builds only)
        debug_error_sensor_ = std::make_unique<GpioSensor>(sensor_configs::DEBUG_ERROR, gpio_provider_);
#endif

        // Initialize all sensors
        key_present_sensor_->Init();
        key_not_present_sensor_->Init();
        lock_sensor_->Init();
        lights_sensor_->Init();

#ifdef CLARITY_DEBUG
        debug_error_sensor_->Init();
        log_i("TriggerHandler created and initialized 5 GPIO sensors using generic GpioSensor: KeyPresent, "
              "KeyNotPresent, Lock, Lights, DebugError (debug build)");
#else
        log_i("TriggerHandler created and initialized 4 GPIO sensors using generic GpioSensor: KeyPresent, "
              "KeyNotPresent, Lock, Lights");
#endif
    }
    else
    {
        log_e("TriggerHandler created without GPIO provider - sensors not initialized. Application will not function "
              "correctly!");
        ErrorManager::Instance().ReportCriticalError("TriggerHandler",
                                                     "Created without GPIO provider - sensors will not function");
    }
}

/**
 * @brief Destructor cleans up TriggerHandler resources
 *
 * GPIO sensors are automatically cleaned up through unique_ptr.
 * No manual cleanup required for GPIO resources.
 */
TriggerHandler::~TriggerHandler()
{
}

/**
 * @brief Main processing loop for trigger evaluation
 *
 * Called during UI idle periods to evaluate all registered triggers
 * for state changes. Ensures trigger processing doesn't interfere
 * with UI responsiveness in automotive applications.
 */
void TriggerHandler::ProcessTriggers()
{
    // Validate triggers to detect state changes
    ValidateTriggers();

    // Execute any pending trigger actions
    ExecutePendingTriggers();
}

/**
 * @brief Registers a trigger for state monitoring and execution
 * @param trigger Trigger configuration including ID, type, priority, and functions
 * @return true if registration successful, false if duplicate ID or array full
 *
 * Adds trigger to internal registry with priority and type classification.
 * Prevents duplicate IDs and enforces maximum trigger limit for memory safety.
 * Essential for automotive state monitoring and panel transitions.
 */
bool TriggerHandler::RegisterTrigger(const Trigger &trigger)
{
    if (trigger_count_ >= MAX_TRIGGERS)
    {
        log_e("Cannot register trigger '%s' - maximum triggers reached (%d)", trigger.id, MAX_TRIGGERS);
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "TriggerHandler",
                                             "Maximum triggers reached - cannot register new trigger");
        return false;
    }

    // Check for duplicate ID
    if (FindTrigger(trigger.id) != nullptr)
    {
        log_w("Trigger with ID '%s' already registered", trigger.id);
        return false;
    }

    // Copy trigger to storage
    triggers_[trigger_count_] = trigger;
    triggers_[trigger_count_].isActive = false; // Always start inactive
    trigger_count_++;

    log_i("Registered trigger '%s' (type: %d, priority: %d)", trigger.id, static_cast<int>(trigger.type),
          static_cast<int>(trigger.priority));
    return true;
}

/**
 * @brief Validates all registered triggers for state changes
 *
 * Examines each registered trigger for sensor state changes but does not execute.
 * State change detection is performed to Prepare for execution phase.
 */
void TriggerHandler::ValidateTriggers()
{
    // Process each trigger for state changes
    for (size_t i = 0; i < trigger_count_; i++)
    {
        EvaluateIndividualTrigger(triggers_[i]);
    }
}

/**
 * @brief Executes any triggers that need activation/deactivation
 *
 * For triggers, execution happens immediately upon state change detection
 * within ValidateTriggers. This method is kept for interface consistency
 * but doesn't perform additional work as trigger execution is immediate.
 */
void TriggerHandler::ExecutePendingTriggers()
{
    // Triggers execute immediately upon state change detection in ValidateTriggers()
    // This method exists for interface consistency with ActionHandler pattern
    // but trigger execution is not deferred like button actions
}

/**
 * @brief Processes initial trigger states after registration
 *
 * This method evaluates all triggers based on their current sensor states
 * and activates any that should be active at startup. This ensures proper
 * system state (e.g., night theme when lights are on) even when the
 * application starts with triggers already in their active state.
 */
void TriggerHandler::ProcessInitialTriggerStates()
{
    log_i("Processing initial trigger states for startup conditions");

    // Process each trigger to check initial state
    for (size_t i = 0; i < trigger_count_; i++)
    {
        Trigger &trigger = triggers_[i];

        if (!trigger.sensor)
        {
            continue; // No sensor associated
        }

        // Get current sensor state
        bool sensor_active = IsSensorActive(trigger);

        // If sensor is active at startup, handle activation
        if (sensor_active)
        {
            log_i("Trigger '%s' is active at startup - processing activation", trigger.id);

            // Mark trigger as active
            trigger.isActive = true;

            // Check if this trigger should execute based on priority
            if (ShouldActivate(trigger))
            {
                HandleTriggerActivation(trigger);
            }
            else
            {
                log_i("Trigger '%s' blocked by higher priority at startup", trigger.id);
            }
        }
    }
}

/**
 * @brief Evaluates single trigger for state change and priority-based execution
 * @param trigger Trigger to evaluate
 *
 * Complex trigger evaluation implementing priority blocking system and same-type
 * restoration. Handles activation/deactivation flows with automotive reliability
 * requirements. Preserves trigger state even when execution is blocked by higher
 * priority triggers, enabling proper restoration when priorities change.
 */
void TriggerHandler::EvaluateIndividualTrigger(Trigger &trigger)
{
    // Early return for guard clauses
    if (!trigger.sensor)
    {
        return; // No sensor associated
    }

    // Check if sensor state has changed
    bool has_changed = trigger.sensor->HasStateChanged();
    if (!has_changed)
    {
        return; // No change, nothing to do
    }

    // Get current sensor state
    bool sensor_active = IsSensorActive(trigger);
    bool was_active = trigger.isActive;

    // Debug logging for KeyPresent trigger specifically
    if (strcmp(trigger.id, hardware_constants::trigger_ids::KEY_PRESENT) == 0)
    {
        log_t("KeyPresent trigger evaluation: hasChanged=%s, wasActive=%s, sensorActive=%s",
              has_changed ? "true" : "false", was_active ? "true" : "false", sensor_active ? "true" : "false");
    }

    // Handle activation flow
    if (!was_active && sensor_active)
    {
        HandleTriggerActivation(trigger);
        return;
    }

    // Handle deactivation flow
    if (was_active && !sensor_active)
    {
        HandleTriggerDeactivation(trigger);
        return;
    }
}

/**
 * @brief Handles trigger activation flow with priority logic
 * @param trigger Trigger to activate
 *
 * Activation Flow with Priority Logic (as per interrupt-architecture.md).
 * Always sets isActive when sensor becomes active, regardless of priority.
 * Only executes function if not blocked by higher priority or error panel.
 */
void TriggerHandler::HandleTriggerActivation(Trigger &trigger)
{
    // ALWAYS set isActive when sensor becomes active, regardless of priority
    trigger.isActive = true;
    UpdatePriorityState(trigger.priority, true);

    // Call sensor's OnInterruptTriggered method for sensor-specific behavior
    if (trigger.sensor)
    {
        trigger.sensor->OnInterruptTriggered();
    }

    // Early return if blocked by higher priority
    if (HasHigherPriorityActive(trigger.priority))
    {
        return;
    }

    // Early return if error panel is active - suppress trigger execution but keep state
    // Exception: Allow error trigger to execute during error panel to support dynamic error addition
    if (ErrorManager::Instance().IsErrorPanelActive() && strcmp(trigger.id, "error") != 0)
    {
        return;
    }

    // Execute activation function if available
    if (trigger.activateFunc)
    {
        trigger.activateFunc();
    }
}

/**
 * @brief Handles trigger deactivation flow with type-based restoration
 * @param trigger Trigger to deactivate
 *
 * Deactivation Flow with Type-Based Restoration (as per interrupt-architecture.md).
 * Marks trigger as inactive and either restores same-type trigger or calls deactivate function.
 * For one-shot triggers (deactivateFunc = nullptr), simply marks inactive with no further action.
 */
void TriggerHandler::HandleTriggerDeactivation(Trigger &trigger)
{
    // Mark this trigger as inactive and update priority state
    trigger.isActive = false;
    UpdatePriorityState(trigger.priority, false);

    // Skip deactivation processing for one-shot triggers (nullptr deactivateFunc)
    if (!trigger.deactivateFunc)
    {
        log_t("Trigger '%s' deactivated - one-shot trigger, no restoration needed", trigger.id);
        return;
    }

    // Find highest priority active trigger of same type
    Trigger *same_type_trigger = FindHighestPrioritySameType(trigger.type);
    if (same_type_trigger)
    {
        // Same-type trigger activation
        same_type_trigger->ExecuteActivate();
        return;
    }

    // Only call deactivate function (restoration) if no same-type triggers found
    if (trigger.deactivateFunc)
    {
        trigger.deactivateFunc();
    }
}

/**
 * @brief Determines if trigger should activate based on priority system
 * @param trigger Trigger to check for activation
 * @return true if trigger should activate (not blocked by higher priority)
 *
 * Priority-based activation logic that prevents lower priority triggers
 * from executing when higher priority triggers are active.
 */
bool TriggerHandler::ShouldActivate(const Trigger &trigger) const
{
    // Block activation if higher priority trigger is active
    // Uses the existing HasHigherPriorityActive() method which checks if any
    // active trigger has a higher numeric priority value than this trigger
    return !HasHigherPriorityActive(trigger.priority);
}

/**
 * @brief Checks if any trigger with higher priority is currently active
 * @param priority Priority level to compare against
 * @return true if higher priority trigger is active
 *
 * Priority comparison logic using numeric values where higher numbers
 * indicate higher priority (CRITICAL=2 > IMPORTANT=1 > NORMAL=0).
 * Essential for implementing priority-based trigger blocking.
 */
bool TriggerHandler::HasHigherPriorityActive(Priority priority) const
{
    // Priority comparison: Higher numeric values = higher priority
    // CRITICAL=2 > IMPORTANT=1 > NORMAL=0 (reverse of typical enum ordering)
    // This allows simple numeric comparison for priority blocking logic

    // With corrected priority values: CRITICAL=2, IMPORTANT=1, NORMAL=0
    // Higher numeric value = higher priority
    int candidate_priority = static_cast<int>(priority);

    // Check if any active trigger has higher numeric priority
    for (size_t i = 0; i < trigger_count_; i++)
    {
        const Trigger &trigger = triggers_[i];
        if (trigger.isActive && static_cast<int>(trigger.priority) > candidate_priority)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Updates priority tracking state for given priority level
 * @param priority Priority level to update
 * @param active Whether this priority level should be marked active
 *
 * Maintains priority state tracking array for efficient priority
 * queries. Used during trigger activation/deactivation cycles.
 */
void TriggerHandler::UpdatePriorityState(Priority priority, bool active)
{
    int priority_index = static_cast<int>(priority);
    if (priority_index >= 0 && priority_index < 3)
    {
        priority_active_[priority_index] = active;
    }
}

/**
 * @brief Finds highest priority active trigger of specified type
 * @param type Trigger type to search for
 * @return Pointer to highest priority trigger of type, or nullptr if none found
 *
 * Searches registered triggers for active triggers of specified type,
 * returning the one with highest priority value. Used for same-type
 * restoration during trigger deactivation sequences.
 */
Trigger *TriggerHandler::FindHighestPrioritySameType(TriggerType type)
{
    Trigger *highest = nullptr;
    int highest_priority_value = -1; // Track numeric priority value

    for (size_t i = 0; i < trigger_count_; i++)
    {
        Trigger &trigger = triggers_[i];
        // Find active triggers of the same type
        if (trigger.type == type && trigger.isActive)
        {
            int trigger_priority_value = static_cast<int>(trigger.priority);
            if (trigger_priority_value > highest_priority_value)
            {
                highest = &trigger;
                highest_priority_value = trigger_priority_value;
            }
        }
    }

    if (highest)
    {
    }

    return highest;
}

/**
 * @brief Executes trigger function based on activation state
 * @param trigger Trigger containing functions to execute
 * @param isActivation true for activation function, false for deactivation
 *
 * Utility function that executes either activate or deactivate function
 * based on the activation state. Provides centralized function execution
 * with null checking for safety.
 */
void TriggerHandler::ExecuteTriggerFunction(const Trigger &trigger, bool is_activation)
{
    if (is_activation && trigger.activateFunc)
    {
        trigger.activateFunc();
    }
    else if (!is_activation && trigger.deactivateFunc)
    {
        trigger.deactivateFunc();
    }
}

/**
 * @brief Determines if trigger's sensor is currently in active state
 * @param trigger Trigger whose sensor to check
 * @return true if sensor reading indicates active state
 *
 * Reads current sensor state and interprets boolean readings as active/inactive.
 * Essential for trigger state evaluation and automotive condition monitoring.
 */
bool TriggerHandler::IsSensorActive(const Trigger &trigger) const
{
    if (!trigger.sensor)
    {
        return false;
    }

    // Get sensor reading to determine if it's active
    Reading reading = trigger.sensor->GetReading();

    // Check if reading is a boolean (GPIO sensors)
    if (std::holds_alternative<bool>(reading))
    {
        return std::get<bool>(reading);
    }

    // For non-boolean sensors, consider them inactive
    return false;
}

/**
 * @brief Finds registered trigger by unique identifier
 * @param id Unique identifier of trigger to find
 * @return Pointer to trigger if found, nullptr otherwise
 *
 * Linear search through registered triggers array to locate trigger by ID.
 * Used for trigger management operations and duplicate prevention.
 */
Trigger *TriggerHandler::FindTrigger(const char *id)
{
    for (size_t i = 0; i < trigger_count_; i++)
    {
        if (strcmp(triggers_[i].id, id) == 0)
        {
            return &triggers_[i];
        }
    }
    return nullptr;
}

// Handler interface implementation complete

// Status and diagnostics
/**
 * @brief Gets current number of registered triggers
 * @return Number of triggers currently registered
 *
 * Provides count of registered triggers for diagnostic purposes
 * and capacity management in automotive systems.
 */
size_t TriggerHandler::GetTriggerCount() const
{
    return trigger_count_;
}

/**
 * @brief Checks if any triggers are currently in active state
 * @return true if at least one trigger is active
 *
 * Scans all registered triggers to determine if any are currently
 * active. Used for system state assessment and coordination.
 */
bool TriggerHandler::HasActiveTriggers() const
{
    for (size_t i = 0; i < trigger_count_; i++)
    {
        if (triggers_[i].isActive)
        {
            return true;
        }
    }
    return false;
}

bool TriggerHandler::HasActivePanelTriggers() const
{
    for (size_t i = 0; i < trigger_count_; i++)
    {
        if (triggers_[i].isActive && triggers_[i].type == TriggerType::PANEL)
        {
            return true;
        }
    }
    return false;
}
