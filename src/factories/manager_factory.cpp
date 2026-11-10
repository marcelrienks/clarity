#include "factories/manager_factory.h"
#include "definitions/types.h"
#include "factories/provider_factory.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "managers/interrupt_manager.h"
#include "managers/panel_manager.h"
#include "managers/style_manager.h"
#include "providers/device_provider.h"
#include "sensors/button_sensor.h"
#include "sensors/gpio_sensor.h"
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

// ========== Private Methods ==========

/**
 * @brief Initializes hardware providers if not already created
 * @return true if all providers initialized successfully
 *
 * Creates device, GPIO, and display providers through the provider factory.
 * Ensures all providers are available before manager creation.
 */
bool ManagerFactory::InitializeProviders()
{

    if (!provider_factory_)
    {
        log_e("Cannot initialize providers - provider factory is null");
        return false;
    }

    // Create providers if not already created
    if (!device_provider_)
    {
        device_provider_ = provider_factory_->CreateDeviceProvider();
        if (!device_provider_)
        {
            log_e("Failed to create DeviceProvider");
            return false;
        }
    }

    if (!gpio_provider_)
    {
        gpio_provider_ = provider_factory_->CreateGpioProvider();
        if (!gpio_provider_)
        {
            log_e("Failed to create GpioProvider");
            return false;
        }
    }

    if (!display_provider_)
    {
        display_provider_ = provider_factory_->CreateDisplayProvider(device_provider_.get());
        if (!display_provider_)
        {
            log_e("Failed to create DisplayProvider");
            return false;
        }
    }

    return true;
}

/**
 * @brief Internal implementation for creating PanelManager
 * @param display Display provider for UI rendering
 * @param gpio GPIO provider for hardware interaction
 * @param styleManager Style service for theme management
 * @param preferenceService Preference service for configuration
 * @param interruptManager Interrupt manager for event handling
 * @return Unique pointer to PanelManager or nullptr on failure
 *
 * Validates all dependencies, creates PanelManager with dependency injection,
 * and initializes it before returning. Reports critical errors on failure.
 */
std::unique_ptr<PanelManager> ManagerFactory::CreatePanelManagerImpl(IDisplayProvider *display, IGpioProvider *gpio,
                                                                     IStyleManager *style_manager,
                                                                     IConfigurationManager *configuration_manager,
                                                                     InterruptManager *interrupt_manager)
{

    // Validate required providers
    if (!display)
    {
        log_e("ManagerFactory: Cannot create PanelManager - IDisplayProvider is null");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "Cannot create PanelManager - DisplayProvider dependency is null");
        return nullptr;
    }
    if (!gpio)
    {
        log_e("ManagerFactory: Cannot create PanelManager - IGpioProvider is null");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "Cannot create PanelManager - GpioProvider dependency is null");
        return nullptr;
    }
    if (!style_manager)
    {
        log_e("ManagerFactory: Cannot create PanelManager - IStyleManager is null");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "Cannot create PanelManager - StyleService dependency is null");
        return nullptr;
    }

    if (!configuration_manager)
    {
        log_e("ManagerFactory: Cannot create PanelManager - IConfigurationManager is null");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "Cannot create PanelManager - ConfigManager null");
        return nullptr;
    }

    auto panel_manager =
        std::make_unique<PanelManager>(display, gpio, style_manager, configuration_manager, interrupt_manager);
    if (!panel_manager)
    {
        log_e("ManagerFactory: Failed to create PanelManager - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "PanelManager allocation failed - out of memory");
        return nullptr;
    }

    panel_manager->Init();

    return panel_manager;
}

/**
 * @brief Internal implementation for creating StyleManager
 * @param theme Initial theme name (defaults to DAY if null)
 * @return Unique pointer to StyleManager or nullptr on failure
 *
 * Creates StyleManager with specified or default theme.
 * Reports critical error if allocation fails.
 */
std::unique_ptr<StyleManager> ManagerFactory::CreateStyleManagerImpl(const char *theme)
{

    auto manager = std::make_unique<StyleManager>(theme ? theme : themes::DAY);
    if (!manager)
    {
        log_e("ManagerFactory: Failed to create StyleManager - allocation failed");
        ErrorManager::Instance().ReportCriticalError("ManagerFactory",
                                                     "StyleManager allocation failed - out of memory");
        return nullptr;
    }

    return manager;
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
 * @brief Internal implementation for creating InterruptManager
 * @param gpioProvider GPIO provider for hardware interrupts
 * @return Pointer to InterruptManager singleton or nullptr on failure
 *
 * Gets singleton instance of InterruptManager and initializes it with GPIO provider.
 * Initialization creates handlers that own sensors for interrupt processing.
 */
InterruptManager *ManagerFactory::CreateInterruptManagerImpl(IGpioProvider *gpio_provider)
{

    if (!gpio_provider)
    {
        log_e("ManagerFactory: Cannot create InterruptManager - GPIO provider is null");
        ErrorManager::Instance().ReportCriticalError(
            "ManagerFactory", "Cannot create InterruptManager - GpioProvider dependency is null");
        return nullptr;
    }

    InterruptManager *manager = &InterruptManager::Instance();
    if (!manager)
    {
        log_e("ManagerFactory: Failed to get InterruptManager singleton instance");
        return nullptr;
    }

    manager->Init(gpio_provider);

    // IMPORTANT: InterruptManager::Init() creates handlers which own the sensors
    // We must register interrupts AFTER Init() completes so contexts are available

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
 * @brief Creates PanelManager with provided or factory-created providers
 * @param display Display provider (optional, created if null)
 * @param gpio GPIO provider (optional, created if null)
 * @param styleManager Style service for theme management
 * @param preferenceService Preference service for configuration
 * @param interruptManager Interrupt manager for event handling
 * @return Unique pointer to PanelManager or nullptr on failure
 *
 * Public interface method that uses provided providers or creates them
 * through provider factory if not supplied. Delegates to implementation method.
 */
std::unique_ptr<PanelManager> ManagerFactory::CreatePanelManager(IDisplayProvider *display, IGpioProvider *gpio,
                                                                 IStyleManager *style_manager,
                                                                 IConfigurationManager *configuration_manager,
                                                                 InterruptManager *interrupt_manager)
{

    // When using the dual factory pattern, get providers from provider factory
    IDisplayProvider *display_to_use = display;
    IGpioProvider *gpio_to_use = gpio;

    // If providers not passed, get them from provider factory
    if (!display || !gpio)
    {
        if (!InitializeProviders())
        {
            log_e("Failed to initialize providers from factory");
            return nullptr;
        }
        display_to_use = display ? display : display_provider_.get();
        gpio_to_use = gpio ? gpio : gpio_provider_.get();
    }

    return CreatePanelManagerImpl(display_to_use, gpio_to_use, style_manager, configuration_manager, interrupt_manager);
}

/**
 * @brief Creates StyleManager with specified theme
 * @param theme Theme name to initialize with
 * @return Unique pointer to StyleManager
 *
 * Public interface method that delegates to implementation.
 */
std::unique_ptr<StyleManager> ManagerFactory::CreateStyleManager(const char *theme)
{
    return CreateStyleManagerImpl(theme);
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
 * @brief Creates InterruptManager with provided or factory-created GPIO provider
 * @param gpioProvider GPIO provider (optional, created if null)
 * @return Pointer to InterruptManager singleton
 *
 * Public interface method that uses provided GPIO provider or creates one
 * through provider factory if not supplied.
 */
InterruptManager *ManagerFactory::CreateInterruptManager(IGpioProvider *gpio_provider)
{

    // If no GPIO provider passed, get from provider factory
    IGpioProvider *gpio_to_use = gpio_provider;
    if (!gpio_to_use)
    {
        if (!InitializeProviders())
        {
            log_e("Failed to initialize providers from factory");
            return nullptr;
        }
        gpio_to_use = gpio_provider_.get();
    }

    return CreateInterruptManagerImpl(gpio_to_use);
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
