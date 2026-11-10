#include "utilities/test_inject.h"

#ifdef CLARITY_TEST_INJECT_ERROR

    #include <Arduino.h>

    #include "hardware/gpio_pins.h"
    #include "managers/error_manager.h"
    #include "utilities/logging.h"

namespace
{
IGpioProvider *gpio = nullptr;
bool previousState = false;
} // namespace

void test_inject::Begin(IGpioProvider *gpioProvider)
{
    gpio = gpioProvider;
    if (!gpio)
    {
        return;
    }

    gpio->PinMode(gpio_pins::DEBUG_ERROR, INPUT);
    previousState = gpio->DigitalRead(gpio_pins::DEBUG_ERROR);
    log_t("Test error injection armed");
}

void test_inject::Poll()
{
    if (!gpio)
    {
        return;
    }

    bool state = gpio->DigitalRead(gpio_pins::DEBUG_ERROR);
    if (state && !previousState)
    {
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "TestInject", "Injected test error");
    }
    previousState = state;
}

#endif // CLARITY_TEST_INJECT_ERROR
