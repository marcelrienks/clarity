#include "main.h"
#include "app/inputs.h"
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
#include "providers/device_provider.h"
#include "ui/screen_manager.h"
#include "ui/theme.h"
#include "utilities/logging.h"
#include "utilities/test_inject.h"
#include <algorithm>
#include <esp_system.h>
#include <string>

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
std::unique_ptr<Theme> ui_theme;
std::unique_ptr<IConfigurationManager> configuration_manager;
std::unique_ptr<ScreenManager> screen_manager;
std::unique_ptr<Inputs> inputs;
ErrorManager *error_manager;

// Set when initializeServices() fails; loop() then does nothing but wait for the restart
static bool init_failed = false;
static uint32_t init_failed_at_ms = 0;
static constexpr uint32_t INIT_FAILED_RESTART_DELAY_MS = 5000;

// Loop sleep bounds: at least 1 ms to yield to other tasks, at most 10 ms so inputs are sampled well inside
// the 30 ms debounce period
static constexpr uint32_t LOOP_MIN_SLEEP_MS = 1;
static constexpr uint32_t LOOP_MAX_SLEEP_MS = 10;

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
    // Starts on Day (Theme::Init()): the lights signal selects Night once it is sampled
    ui_theme = std::make_unique<Theme>();

    inputs = std::make_unique<Inputs>(*gpio_provider);

    screen_manager = std::make_unique<ScreenManager>(*gpio_provider, *ui_theme, *configuration_manager);

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

/// Long press threshold from the configuration; read every pass so a change applies to the press in progress
static void UpdateButtonThresholds()
{
    int long_press_ms = config_constants::defaults::DEFAULT_LONG_PRESS_MS;
    if (auto value = configuration_manager->QueryConfig<int>(config_constants::keys::BUTTON_LONG_PRESS_MS))
    {
        long_press_ms = *value;
    }
    inputs->SetButtonThresholds(clarity::core::InputSampler::DEFAULT_MIN_SHORT_MS,
                                static_cast<uint32_t>(long_press_ms));
}

/// Debug builds only: the test button (GPIO 34) reports one warning, one error and one critical error
static void ReportDebugErrors()
{
#ifdef CLARITY_DEBUG
    log_t("ErrorActivate() - Debug error button pressed, generating test errors");

    // Three test errors with unique timestamps, as the old debug trigger generated
    const std::string stamp = std::to_string(millis());
    ErrorManager &errors = ErrorManager::Instance();
    errors.ReportWarning("DebugTest", "Test warning from debug error trigger @" + stamp);
    errors.ReportError(ErrorLevel::ERROR, "DebugTest", "Test error from debug error trigger @" + stamp);
    errors.ReportCriticalError("DebugTest", "Test critical error from debug error trigger @" + stamp);

    log_t("Debug errors generated: 1 WARNING, 1 ERROR, 1 CRITICAL - error panel will load automatically");
#endif
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

    ui_theme->Init();

    test_inject::Begin(gpio_provider.get());

    // Seed every input from its first read, then decide the first panel from the inputs: a key or lock
    // that is active at boot shows its panel directly and skips the default panel and its splash
    UpdateButtonThresholds();
    inputs->Begin(millis());
    screen_manager->Begin(inputs->Vehicle());

    log_i("Clarity application started successfully");
}

/**
 * @brief Arduino main loop - processes system events and updates UI
 *
 * Samples the inputs, hands the button event to the screen manager, lets it
 * apply the panel transition the inputs and errors ask for and updates the
 * active panel. Ends with the single LVGL pump and sleeps until LVGL's next
 * timer is due, at most 10 ms so the inputs keep being sampled (D-12).
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

    UpdateButtonThresholds();
    inputs->Sample(millis());

    // The button first, then the debug error line, then the vehicle lines and errors. Everything is acted on in
    // the pass it arrives (Q-07): the panel only returns an intent, applied by Apply() below
    const clarity::core::ButtonEvent button = inputs->Button();
    if (button == clarity::core::ButtonEvent::Long)
    {
        log_t("Long press triggered during hold");
    }
    screen_manager->OnButton(button);

    if (inputs->DebugErrorEdge())
    {
        ReportDebugErrors();
    }
    test_inject::Poll();

    // The single transition point. Overrides, the error panel included, wait for the splash to finish (Q-06)
    screen_manager->Apply(inputs->Vehicle(), error_manager->ShouldTriggerErrorPanel());

    // Every pass: a panel that animates skips its own updates until the animation ends
    screen_manager->Update(millis());

    // The only lv_timer_handler() call: it renders and runs LVGL timers, and returns the ms until the next is due
    const uint32_t idle_ms = lv_timer_handler();
    delay(std::clamp<uint32_t>(idle_ms, LOOP_MIN_SLEEP_MS, LOOP_MAX_SLEEP_MS));
}
