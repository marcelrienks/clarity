#pragma once

#include "definitions/constants.h"
#include "definitions/types.h"
#include "interfaces/i_gpio_provider.h"
#include <functional>
#include <memory>
#include <vector>

#include "esp32-hal-log.h"

// Forward declarations
class IConfigurationManager;
class IPanelManager;

/**
 * @class InterruptManager
 * @brief Pure coordinated interrupt system for ESP32 automotive applications
 *
 * @details Complete event-driven interrupt coordination replacing all polling-based
 * architecture. Manages interrupt registration, evaluation, and execution with
 * priority-based processing and optimized evaluation intervals.
 *
 * @architecture Singleton pattern with static memory allocation for ESP32 safety
 * @memory_optimization Static interrupt array (32 max), 29 bytes per interrupt
 * @performance Smart evaluation intervals: Critical 10ms, Important 25ms, Normal 50ms
 * @handlers TriggerHandler for state-based evaluation, ActionHandler for event-based execution
 */
class InterruptManager
{
  public:
    // ========== Constructors and Destructor ==========
    InterruptManager(const InterruptManager &) = delete;
    InterruptManager &operator=(const InterruptManager &) = delete;
    ~InterruptManager() = default;

    // ========== Static Methods ==========
    static InterruptManager &Instance();

    // ========== Public Interface Methods ==========
    void Init(IGpioProvider *gpio = nullptr);
    void Process();
    void ProcessInitialTriggerStates();
    bool RegisterTrigger(const Trigger &trigger);
    bool RegisterAction(const Action &action);
    void SetCurrentPanel(class IActionHandler *panel);
    void SetConfigurationManager(IConfigurationManager *configuration_manager);
    void SetPanelManager(IPanelManager *panel_manager);

    /**
     * @brief Get total count of registered interrupts (triggers + actions)
     * @return Total number of registered interrupts
     */
    size_t GetRegisteredInterruptCount() const;

    /**
     * @brief Check if any triggers or actions are currently active
     * @return true if active interrupts exist, false otherwise
     */
    bool HasActiveInterrupts() const;

    /**
     * @brief Get direct access to trigger handler
     * @return Pointer to TriggerHandler instance
     */
    class TriggerHandler *GetTriggerHandler() const
    {
        return trigger_handler_.get();
    }

    /**
     * @brief Get direct access to action handler
     * @return Pointer to ActionHandler instance
     */
    class ActionHandler *GetActionHandler() const
    {
        return action_handler_.get();
    }

    /**
     * @brief Check if panel restoration is needed after interrupt deactivation
     * @details Coordinates with PanelManager for seamless panel restoration
     */
    void CheckRestoration();

    /**
     * @brief Find and execute highest priority PANEL trigger
     * @return true if trigger was found and executed, false otherwise
     */
    bool CheckAndExecuteHighestPriorityTrigger();

    /**
     * @brief Find and execute highest priority STYLE trigger
     * @details Handles theme and styling triggers separately from panel triggers
     */
    void CheckAndExecuteActiveStyleTriggers();

    /**
     * @brief Check if any PANEL type triggers are currently active
     * @return true if any panel triggers are active, false otherwise
     */
    bool HasActivePanelTriggers() const;

  private:
    // ========== Constructors and Destructor ==========
    InterruptManager() = default;

    // ========== Private Methods ==========
    /**
     * @brief Register all system-level triggers and actions
     * @details Creates sensors and registers standard system interrupts
     */
    void RegisterSystemInterrupts();

    // ========== Private Data Members ==========
    // Trigger/Action architecture handlers
    std::shared_ptr<class TriggerHandler> trigger_handler_;
    std::shared_ptr<class ActionHandler> action_handler_;

    // System state
    bool initialized_ = false;
    unsigned long last_evaluation_time_ = 0;

    // Provider references
    IGpioProvider *gpio_provider_ = nullptr;
    IPanelManager *panel_manager_ = nullptr;
};