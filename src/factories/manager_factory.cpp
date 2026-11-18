#include "factories/manager_factory.h"
#include "definitions/types.h"
#include "factories/provider_factory.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "providers/device_provider.h"
#include <esp32-hal-log.h>

// ========== Constructors and Destructor ==========

/**
 * @brief Constructs ManagerFactory with custom provider factory
 * @param providerFactory Provider factory for creating hardware providers
 *
 * Accepts ownership of a provider factory for dependency injection.
 * Falls back to creating default ProviderFactory if nullptr is provided.
 */
ManagerFactory::ManagerFactory(std::unique_ptr<ProviderFactory> provider_factory)
    : provider_factory_(std::move(provider_factory))
{
    if (!provider_factory_)
    {
        log_w("ManagerFactory created without provider factory - will create default ProviderFactory");
        provider_factory_ = std::make_unique<ProviderFactory>();
    }
}

/**
 * @brief Default constructor creates standard provider factory
 *
 * Initializes ManagerFactory with default ProviderFactory for
 * standard hardware provider creation.
 */
ManagerFactory::ManagerFactory() : provider_factory_(std::make_unique<ProviderFactory>())
{
}

/**
 * @brief Internal implementation for creating ConfigurationManager
 * @return Unique pointer to IConfigurationManager or nullptr on failure
 *
 * Creates ConfigurationManager as the unified configuration interface.
 * Returns as IConfigurationManager interface for abstraction.
 */
std::unique_ptr<IConfigurationManager> ManagerFactory::CreateConfigurationManagerImpl()
{
    // Create ConfigurationManager as the unified configuration interface
    auto manager = std::make_unique<ConfigurationManager>();
    if (!manager)
    {
        log_e("ManagerFactory: Failed to create ConfigurationManager - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "ConfigurationManager allocation failed - out of memory");
        return nullptr;
    }

    // Initialize the configuration manager with storage backend
    if (!manager->Initialize())
    {
        log_e("ManagerFactory: Failed to initialize ConfigurationManager");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory", "ConfigurationManager initialization failed");
        return nullptr;
    }

    return manager;
}

/**
 * @brief Internal implementation for creating ErrorManager
 * @return Pointer to ErrorManager singleton or nullptr on failure
 *
 * Returns the singleton instance of ErrorManager for system-wide
 * error reporting and management.
 */
ErrorManager *ManagerFactory::CreateErrorManagerImpl()
{

    ErrorManager *error_manager = &ErrorManager::Instance();
    if (!error_manager)
    {
        log_e("ManagerFactory: Failed to get ErrorManager singleton instance");
        return nullptr;
    }

    return error_manager;
}

/**
 * @brief Creates PreferenceManager for configuration management
 * @return Unique pointer to IConfigurationManager interface
 *
 * Public interface method that delegates to implementation.
 */
std::unique_ptr<IConfigurationManager> ManagerFactory::CreateConfigurationManager()
{
    return CreateConfigurationManagerImpl();
}

/**
 * @brief Creates ErrorManager singleton instance
 * @return Pointer to ErrorManager singleton
 *
 * Public interface method that delegates to implementation.
 */
ErrorManager *ManagerFactory::CreateErrorManager()
{
    return CreateErrorManagerImpl();
}
