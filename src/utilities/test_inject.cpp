#include "utilities/test_inject.h"

#ifdef CLARITY_TEST_INJECT_ERROR

    #include <Arduino.h>

    #include "hardware/gpio_pins.h"
    #include "managers/error_manager.h"
    #include "utilities/logging.h"

namespace
{
IGpioProvider *gpio = nullptr;
bool previous_state = false;
} // namespace

void test_inject::Begin(IGpioProvider *gpio_provider)
{
    gpio = gpio_provider;
    if (!gpio)
    {
        return;
    }

    gpio->PinMode(gpio_pins::DEBUG_ERROR, INPUT);
    previous_state = gpio->DigitalRead(gpio_pins::DEBUG_ERROR);
    log_t("Test error injection armed");
}

void test_inject::Poll()
{
    if (!gpio)
    {
        return;
    }

    bool state = gpio->DigitalRead(gpio_pins::DEBUG_ERROR);
    if (state && !previous_state)
    {
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "TestInject", "Injected test error");
    }
    previous_state = state;
}

#endif // CLARITY_TEST_INJECT_ERROR
