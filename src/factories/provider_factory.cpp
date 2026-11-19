#include "factories/provider_factory.h"
#include "managers/error_manager.h"
#include "providers/device_provider.h"
#include "providers/gpio_provider.h"
#include "providers/lvgl_display_provider.h"
#include "providers/storage_provider.h"
#include <Arduino.h>

#include "esp32-hal-log.h"

// ========== Public Interface Methods ==========

/**
 * @brief Creates a GPIO provider for hardware pin control
 * @return Unique pointer to IGpioProvider interface or nullptr on failure
 *
 * Instantiates GpioProvider with exception handling for allocation failures.
 * Provides hardware abstraction for GPIO operations including digital/analog
 * reads and interrupt management.
 */
std::unique_ptr<IGpioProvider> ProviderFactory::CreateGpioProvider()
{
    auto provider = std::make_unique<GpioProvider>();
    if (!provider)
    {
        log_e("Failed to create GpioProvider - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ProviderFactory",
                                                     "GpioProvider allocation failed - out of memory");
        return nullptr;
    }
    return provider;
}

/**
 * @brief Creates a display provider for LVGL UI operations
 * @param deviceProvider Device provider with initialized screen
 * @return Unique pointer to IDisplayProvider interface or nullptr on failure
 *
 * Creates LvglDisplayProvider using screen from DeviceProvider. Validates
 * device provider and screen availability. Reports critical errors on failure.
 */
std::unique_ptr<IDisplayProvider> ProviderFactory::CreateDisplayProvider(DeviceProvider *device_provider)
{
    if (!device_provider)
    {
        log_e("ProviderFactory: Cannot create DisplayProvider - DeviceProvider parameter is null");
        ErrorManager::Instance().ReportCriticalError(
            "ProviderFactory", "Cannot create DisplayProvider - DeviceProvider dependency is null");
        return nullptr;
    }

    // Access the screen from the injected deviceProvider using interface method
    lv_obj_t *screen = device_provider->GetScreen();
    if (!screen)
    {
        log_e("ProviderFactory: Cannot create DisplayProvider - DeviceProvider screen is null");
        ErrorManager::Instance().ReportCriticalError(
            "ProviderFactory", "Cannot create DisplayProvider - DeviceProvider screen not initialized");
        return nullptr;
    }

    auto provider = std::make_unique<LvglDisplayProvider>(screen);
    if (!provider)
    {
        log_e("ProviderFactory: Failed to create LvglDisplayProvider - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ProviderFactory",
                                                     "LvglDisplayProvider allocation failed - out of memory");
        return nullptr;
    }
    return provider;
}

/**
 * @brief Creates a device provider for display hardware initialization
 * @return Unique pointer to DeviceProvider or nullptr on failure
 *
 * Instantiates DeviceProvider and initializes display hardware including
 * SPI configuration, GC9A01 panel setup, and LVGL initialization.
 * Validates screen initialization before returning.
 */
std::unique_ptr<DeviceProvider> ProviderFactory::CreateDeviceProvider()
{
    auto provider = std::make_unique<DeviceProvider>();
    if (!provider)
    {
        log_e("ProviderFactory: Failed to create DeviceProvider - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ProviderFactory",
                                                     "DeviceProvider allocation failed - out of memory");
        return nullptr;
    }

    // Prepare the device (initializes screen)
    provider->Prepare();
    if (!provider->GetScreen())
    {
        log_e("ProviderFactory: DeviceProvider screen initialization failed");
        ErrorManager::Instance().ReportCriticalError("ProviderFactory", "DeviceProvider screen initialization failed");
        return nullptr;
    }

    return provider;
}

/**
 * @brief Creates a storage provider for configuration persistence
 * @return Unique pointer to IStorageProvider interface or nullptr on failure
 *
 * Instantiates StorageProvider for ESP32 NVS-based configuration storage.
 * Provides hardware abstraction for persistent configuration data with
 * thread-safe operations and sectioned storage organization.
 */
std::unique_ptr<IStorageProvider> ProviderFactory::CreateStorageProvider()
{
    auto provider = std::make_unique<StorageProvider>();
    if (!provider)
    {
        log_e("ProviderFactory: Failed to create StorageProvider - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ProviderFactory",
                                                     "StorageProvider allocation failed - out of memory");
        return nullptr;
    }

    return provider;
}