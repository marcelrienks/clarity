#pragma once

// System/Library Includes
#include <Arduino.h>
#include <functional>
#include <lvgl.h>

/**
 * @class Ticker
 * @brief LVGL timing and dynamic delay management utility
 *
 * @details This utility class provides timing services for LVGL integration
 * and dynamic delay management. It handles LVGL's task scheduling and
 * provides adaptive delays based on system performance.
 *
 * @static_class All methods are static - no instantiation required
 * @lvgl_integration Manages LVGL timer and task execution
 * @performance_optimization Dynamic delays prevent CPU overload
 *
 * @core_functions:
 * - getElapsedMillis(): High-resolution timing using Arduino millis()
 * - HandleLvTasks(): Process LVGL timer callbacks and animations
 * - HandleDynamicDelay(): Adaptive delay based on execution time
 *
 * @timing_strategy:
 * - Fixed LVGL tick processing
 * - Dynamic delays adjust to system load
 * - Prevents blocking operations from affecting UI responsiveness
 *
 * @usage_context:
 * - Main loop timing coordination
 * - LVGL task scheduling
 * - Performance-adaptive delays
 * - Animation timing synchronization
 *
 * @context This utility manages timing for the main application loop.
 * It ensures LVGL gets proper time slices for animations and UI updates
 * while providing adaptive delays for optimal performance.
 */
class Ticker
{
  public:
    // Static Methods
    static void HandleDynamicDelay(uint32_t start_time);
    static void HandleLvTasks();
};