#include "utilities/ticker.h"
#include <esp32-hal-log.h>
#include <functional>

// ========== Static Methods ==========

/**
 * @brief Adaptive Timing to generate a ~60fps refresh rate
 * @param start_time the start time of the loop method
 */
void Ticker::HandleDynamicDelay(uint32_t start_time)
{
    // Calculate how long processing took
    uint32_t elapsed_time = millis() - start_time;

    // Adjust delay to target ~60fps (16.7ms per frame)
    uint32_t target_frame_time = 16;
    if (elapsed_time < target_frame_time)
    {
        delay(target_frame_time - elapsed_time);
    }
    else
    {
        delay(1); // No delay needed, just yield to other tasks
    }
}

/**
 * @brief Handle lv tasks by calculating the time differences since start up
 */
void Ticker::HandleLvTasks()
{
    static uint32_t last_tick_increment = 0;

    uint32_t current_time = millis();

    // Increment tick counter with the elapsed time
    uint32_t elapsed = current_time - last_tick_increment;
    if (elapsed > 0)
    {
        lv_tick_inc(elapsed);
        last_tick_increment = current_time;
    }

    // Process all pending tasks
    lv_timer_handler();
}
