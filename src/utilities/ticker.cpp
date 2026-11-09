#include "utilities/ticker.h"
#include <functional>
#include <esp32-hal-log.h>

// ========== Static Methods ==========

/**
 * @brief Adaptive Timing to generate a ~60fps refresh rate
 * @param start_time the start time of the loop method
 */
void Ticker::handleDynamicDelay(uint32_t startTime)
{
    // Calculate how long processing took
    uint32_t elapsedTime = millis() - startTime;

    // Adjust delay to target ~60fps (16.7ms per frame)
    uint32_t targetFrameTime = 16;
    if (elapsedTime < targetFrameTime)
        delay(targetFrameTime - elapsedTime);

    else
        delay(1); // No delay needed, just yield to other tasks
}

/**
 * @brief Handle lv tasks by calculating the time differences since start up
 */
void Ticker::handleLvTasks()
{
    static uint32_t lastTickIncrement = 0;

    uint32_t currentTime = millis();

    // Increment tick counter with the elapsed time
    uint32_t elapsed = currentTime - lastTickIncrement;
    if (elapsed > 0)
    {
        lv_tick_inc(elapsed);
        lastTickIncrement = currentTime;
    }

    // Process all pending tasks
    lv_timer_handler();
}
