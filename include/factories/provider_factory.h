#pragma once

#include <memory>

// Forward declarations
class IGpioProvider;
class IDisplayProvider;
class IStorageProvider;

// Forward declarations
class DeviceProvider;

/**
 * @class ProviderFactory
 * @brief Concrete factory implementation for creating hardware providers
 *
 * @details This factory creates all hardware abstraction providers used in
 * the Clarity system (GPIO, Display, Storage, Device). It implements the
 * IProviderFactory interface to enable testability through dependency injection.
 *
 * @design_pattern Concrete Factory (implements Abstract Factory)
 * @responsibility Hardware provider instantiation and configuration
 * @ownership All created providers are returned as unique_ptr for clear ownership
 * @thread_safety Factory methods are thread-safe for creation
 *
 * @architecture_notes
 * - Single responsibility: Only creates providers
 * - Implements IProviderFactory for test injection
 * - Used by ManagerFactory to obtain providers
 * - Enables mock provider injection in tests
 *
 * @example
 * @code
 * // Production usage
 * auto providerFactory = std::make_unique<ProviderFactory>();
 * auto gpioProvider = providerFactory->CreateGpioProvider();
 * auto displayProvider = providerFactory->CreateDisplayProvider();
 * auto storageProvider = providerFactory->CreateStorageProvider();
 * auto deviceProvider = providerFactory->CreateDeviceProvider();
 *
 * // Pass to ManagerFactory
 * auto managerFactory = std::make_unique<ManagerFactory>(providerFactory.get());
 * @endcode
 */
class ProviderFactory
{
  public:
    // ========== Constructors and Destructor ==========
    ProviderFactory() = default;
    ~ProviderFactory() = default;

    // ========== Public Interface Methods ==========
    // Factory methods
    std::unique_ptr<IGpioProvider> CreateGpioProvider();
    std::unique_ptr<IDisplayProvider> CreateDisplayProvider(DeviceProvider *device_provider);
    std::unique_ptr<IStorageProvider> CreateStorageProvider();
    std::unique_ptr<DeviceProvider> CreateDeviceProvider();
};