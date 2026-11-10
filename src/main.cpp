#include "main.h"
#include "definitions/configs.h"
#include "definitions/constants.h"
#include "definitions/types.h"
#include "factories/manager_factory.h"
#include "factories/provider_factory.h"
#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "managers/configuration_manager.h"
#include "managers/error_manager.h"
#include "managers/interrupt_manager.h"
#include "managers/panel_manager.h"
#include "managers/style_manager.h"
#include "providers/device_provider.h"
#include "utilities/logging.h"
#include "utilities/test_inject.h"
#include "utilities/ticker.h"
#include <esp_system.h>

// Self-registration at program startup
static bool system_config_registered = []()
{
    ConfigurationManager::AddSchema(SystemConfig::RegisterConfigSchema);
    return true;
}();

// ========== Global Variables ==========

// Global variable definitions (declarations are in main.h)
std::unique_ptr<ProviderFactory> provider_factory;
std::unique_ptr<ManagerFactory> manager_factory;
std::unique_ptr<DeviceProvider> device_provider;
std::unique_ptr<IGpioProvider> gpio_provider;
std::unique_ptr<IDisplayProvider> display_provider;
std::unique_ptr<StyleManager> style_manager;
std::unique_ptr<IConfigurationManager> configuration_manager;
std::unique_ptr<PanelManager> panel_manager;
InterruptManager *interrupt_manager;
ErrorManager *error_manager;

// Set when initializeServices() fails; loop() then does nothing but wait for the restart
static bool init_failed = false;
static uint32_t init_failed_at_ms = 0;
static constexpr uint32_t INIT_FAILED_RESTART_DELAY_MS = 5000;

// ========== System Configuration ==========

/**
 * @brief Static method to register system configuration schema without instance
 * @param configManager Service to register schema with
 *
 * Called automatically at program startup through ConfigRegistry.
 * Registers the SystemConfig configuration schema without
 * requiring a manager instance to exist.
 */
void SystemConfig::RegisterConfigSchema(IConfigurationManager *config_manager)
{
    if (!config_manager)
    {
        log_e("SystemConfig::RegisterConfigSchema: ConfigurationManager is null - system config registration failed!");
        ErrorManager::Instance().ReportCriticalError("SystemConfig", "ConfigManager null - system config failed");
        return;
    }

    // Check if already registered to prevent duplicates
    if (config_manager->IsSchemaRegistered(config_constants::sections::SYSTEM))
    {
        return;
    }

    config::ConfigSection section(config_constants::sections::SYSTEM, config_constants::sections::SYSTEM,
                                  config_constants::section_names::SYSTEM);

    section.AddItem(default_panel_config);
    section.AddItem(update_rate_config);
    section.AddItem(show_splash_config);

    config_manager->RegisterConfigSection(section);
    log_i("SystemConfig configuration schema registered (static)");
}

/**
 * @brief Legacy function for backward compatibility during migration
 *
 * This function maintains backward compatibility during migration.
 * New code path uses static RegisterConfigSchema instead.
 * Can be removed once migration is complete.
 */
void RegisterSystemConfiguration()
{
    // During migration, just delegate to static method
    SystemConfig::RegisterConfigSchema(configuration_manager.get());
}

// ========== Private Methods ==========

/**
 * @brief Initializes all system services and managers using factory pattern
 * @return true if all services initialized successfully, false on failure
 *
 * Creates and initializes all core system components in the correct dependency
 * order. Uses dual factory pattern with ProviderFactory for hardware abstraction
 * and ManagerFactory for system services. Reports critical errors on failure.
 */
bool InitializeServices()
{
    log_i("Starting Clarity service initialization with dual factory pattern...");

#ifdef CLARITY_TEST_FAIL_INIT
    // Test only (D-04): force the failed-init path
    log_t("Test: forcing service initialization failure");
    return false;
#endif

    provider_factory = std::make_unique<ProviderFactory>();
    if (!provider_factory)
    {
        log_e("Failed to create ProviderFactory - %s", "allocation failed");
        ErrorManager::Instance().ReportCriticalError("main", "ProviderFactory allocation failed");
        return false;
    }

    device_provider = provider_factory->CreateDeviceProvider();
    if (!device_provider)
    {
        log_e("Failed to create DeviceProvider via factory");
        ErrorManager::Instance().ReportCriticalError("main", "DeviceProvider creation failed");
        return false;
    }

    gpio_provider = provider_factory->CreateGpioProvider();
    if (!gpio_provider)
    {
        log_e("Failed to create GpioProvider via factory");
        ErrorManager::Instance().ReportCriticalError("main", "GpioProvider creation failed");
        return false;
    }

    display_provider = provider_factory->CreateDisplayProvider(device_provider.get());
    if (!display_provider)
    {
        log_e("Failed to create DisplayProvider via factory");
        ErrorManager::Instance().ReportCriticalError("main", "DisplayProvider creation failed");
        return false;
    }

    manager_factory = std::make_unique<ManagerFactory>(std::move(provider_factory));
    if (!manager_factory)
    {
        log_e("Failed to create ManagerFactory - %s", "allocation failed");
        ErrorManager::Instance().ReportCriticalError("main", "ManagerFactory allocation failed");
        return false;
    }

    // Create ConfigurationManager via factory (sets up singleton)
    configuration_manager = manager_factory->CreateConfigurationManager();
    if (!configuration_manager)
    {
        log_e("Failed to create ConfigurationManager via factory");
        ErrorManager::Instance().ReportCriticalError("main", "ConfigurationManager creation failed");
        return false;
    }
    // Initialize StyleManager with user's theme preference
    std::string user_theme = themes::DAY; // Default
    if (auto theme_value = configuration_manager->QueryConfig<std::string>(config_constants::keys::SYSTEM_THEME))
    {
        user_theme = *theme_value;
    }
    style_manager = manager_factory->CreateStyleManager(user_theme.c_str());
    if (!style_manager)
    {
        log_e("Failed to create StyleManager via factory");
        ErrorManager::Instance().ReportCriticalError("main", "StyleManager creation failed");
        return false;
    }

    style_manager->SetConfigurationManager(configuration_manager.get());

    // Create InterruptManager with GPIO provider dependency
    interrupt_manager = manager_factory->CreateInterruptManager(gpio_provider.get());
    if (!interrupt_manager)
    {
        log_e("Failed to create InterruptManager via factory");
        ErrorManager::Instance().ReportCriticalError("main", "InterruptManager creation failed");
        return false;
    }

    // Set preference service for button configuration
    interrupt_manager->SetConfigurationManager(configuration_manager.get());

    // Create PanelManager with all dependencies
    panel_manager =
        manager_factory->CreatePanelManager(display_provider.get(), gpio_provider.get(), style_manager.get(),
                                            configuration_manager.get(), interrupt_manager);

    if (!panel_manager)
    {
        log_e("Failed to create PanelManager via factory");
        ErrorManager::Instance().ReportCriticalError("main", "PanelManager creation failed");
        return false;
    }

    // Set panel manager reference for UI state checking in interrupt processing
    interrupt_manager->SetPanelManager(panel_manager.get());

    error_manager = manager_factory->CreateErrorManager();
    if (!error_manager)
    {
        log_e("Failed to create ErrorManager via factory");
        ErrorManager::Instance().ReportCriticalError("main", "ErrorManager creation failed");
        return false;
    }

    log_t("System ready");
    return true;
}

// ========== Arduino Functions ==========

/**
 * @brief Arduino setup function - initializes the Clarity application
 *
 * Entry point for ESP32 application initialization. Initializes all system
 * services, configures display styles, and loads the initial panel based on
 * user preferences. Called once at system startup before the main loop.
 */
void setup()
{
    log_i("Starting Clarity application...");

    if (!InitializeServices())
    {
        log_e("Service initialization failed - restarting in %u ms", (unsigned)INIT_FAILED_RESTART_DELAY_MS);
        log_t("Service initialization failed - restarting");
        init_failed = true;
        init_failed_at_ms = millis();
        return;
    }

    // Register all configuration schemas from components
    ConfigurationManager::Instance().RegisterAllSchemas();

    Ticker::HandleLvTasks();

    style_manager->InitializeStyles();
    Ticker::HandleLvTasks();

    test_inject::Begin(gpio_provider.get());

    // Process initial trigger states to ensure correct system state at startup
    // This must happen after initialization but before loading the first panel
    // If a PANEL trigger is active, it will load its panel and we should skip default panel load
    interrupt_manager->ProcessInitialTriggerStates();

    // Only load default panel if no panel triggers are currently active
    // If a trigger was active at startup, it has already loaded its panel
    if (!interrupt_manager->HasActivePanelTriggers())
    {
        // Determine the default panel from configuration
        std::string default_panel_name = panel_names::OIL; // Default
        if (auto name_value =
                configuration_manager->QueryConfig<std::string>(config_constants::keys::SYSTEM_DEFAULT_PANEL))
        {
            default_panel_name = *name_value;
        }
        panel_manager->CreateAndLoadPanel(default_panel_name.c_str());
        Ticker::HandleLvTasks();
    }
    else
    {
        log_i("Panel trigger active at startup - skipping default panel load");
    }

    log_i("Clarity application started successfully");
}

/**
 * @brief Arduino main loop - processes system events and updates UI
 *
 * Continuously processes interrupt events, monitors error conditions,
 * and updates the active panel. Handles error panel triggering when
 * UI is idle to avoid conflicts with other operations. Maintains
 * responsive system behavior through dynamic delay management.
 */
void loop()
{
    // Init failed: managers may be null, so touch nothing and restart after a delay
    if (init_failed)
    {
        if (millis() - init_failed_at_ms >= INIT_FAILED_RESTART_DELAY_MS)
        {
            esp_restart();
        }
        delay(10);
        return;
    }

    // Always process - ActionHandler runs always, TriggerHandler only on IDLE
    interrupt_manager->Process();
    test_inject::Poll();

    // Check for error panel trigger - must be processed when UI is IDLE to avoid conflicts
    static bool error_panel_triggered = false; // Track if error panel was already triggered
    if (panel_manager->GetUiState() == UIState::IDLE)
    {
        bool should_trigger_error = error_manager->ShouldTriggerErrorPanel();
        const char *current_panel = panel_manager->GetCurrentPanel();
        bool is_currently_error_panel = current_panel && strcmp(current_panel, panel_names::ERROR) == 0;

        // Trigger error panel if needed and not already triggered
        if (should_trigger_error && !error_panel_triggered && !is_currently_error_panel)
        {
            log_t("ErrorOccurredActivate() - Loading ERROR panel");
            panel_manager->CreateAndLoadPanel(panel_names::ERROR, true); // Mark as trigger-driven
            error_manager->SetErrorPanelActive(true);
            error_panel_triggered = true;
        }
        // Reset trigger state when no longer needed
        else if (!should_trigger_error && error_panel_triggered)
        {
            error_manager->SetErrorPanelActive(false);
            error_panel_triggered = false;
        }
    }

    // Update panel state only when IDLE - allows loading and animations to complete
    if (panel_manager->GetUiState() == UIState::IDLE)
    {
        panel_manager->UpdatePanel();
    }

    Ticker::HandleLvTasks();
    Ticker::HandleDynamicDelay(millis());
}
