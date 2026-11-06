#pragma once

#include "interfaces/i_gpio_provider.h"

/**
 * @namespace TestInject
 * @brief Test-only error injection for Wokwi runs of builds without CLARITY_DEBUG
 *
 * @details The debug error trigger (GPIO 34) only exists under CLARITY_DEBUG, but
 * some defects (D-03) only show in builds without it. With CLARITY_TEST_INJECT_ERROR
 * defined, a rising edge on GPIO 34 reports one ERROR through ErrorManager, the same
 * path a real fault takes. Without the flag both functions compile to nothing.
 *
 * @note Mutually exclusive with CLARITY_DEBUG: the debug trigger already owns GPIO 34.
 */
#if defined(CLARITY_TEST_INJECT_ERROR) && defined(CLARITY_DEBUG)
#error "CLARITY_TEST_INJECT_ERROR and CLARITY_DEBUG both claim GPIO 34 - define only one"
#endif

namespace TestInject
{
#ifdef CLARITY_TEST_INJECT_ERROR
    /// Configure the injection pin and seed its state so a held button is not an edge
    void Begin(IGpioProvider *gpioProvider);

    /// Call once per loop; reports one error per rising edge
    void Poll();
#else
    inline void Begin(IGpioProvider *) {}
    inline void Poll() {}
#endif
}
