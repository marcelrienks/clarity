#pragma once

/**
 * @interface IActionHandler
 * @brief Unified interface for button action handling in callback-free architecture
 *
 * @details This interface consolidates IActionService and IActionExecutionService
 * into a single, coherent contract for handling button press events. This eliminates
 * redundancy and confusion while maintaining the callback-free architecture.
 *
 * @design_pattern Dependency Injection - Used by both panels and action executors
 * @memory_safety Zero heap allocation - Interface calls compile to direct calls
 *
 * @implementations:
 * - Panels: Direct implementation for handling their own button actions
 * - PanelManager: Routes button actions to current panel
 * - Mock implementations: For unit testing action behavior
 *
 * @timing Short press: 50ms-2000ms, Long press: 2000ms-5000ms
 */
class IActionHandler
{
  public:
    virtual ~IActionHandler() = default;

    /**
     * @brief Handle short button press (50ms - 2000ms)
     * @details Called directly when a short press is detected
     */
    virtual void HandleShortPress() = 0;

    /**
     * @brief Handle long button press (2000ms - 5000ms)
     * @details Called directly when a long press is detected
     */
    virtual void HandleLongPress() = 0;
};