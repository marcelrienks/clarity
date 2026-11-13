#pragma once

/**
 * @file main.h
 * @brief Main application entry point and core system includes
 *
 * @details This header defines the main application version and includes all core
 * system dependencies. It serves as the central coordination point for the
 * Clarity ESP32 digital gauge system.
 * @architecture MVP (Model-View-Presenter) with layered design
 * @target_hardware ESP32-WROOM-32 with 1.28" round GC9A01 display
 *
 * @system_flow:
 * 1. Device initialization and display setup
 * 2. Preference loading and theme configuration
 * 3. Panel manager initialization and default panel loading
 * 4. Main event loop with ticker-based updates
 *
 * @context This is the main entry point for the application. The system
 * follows a strict initialization order and uses dependency injection for
 * coordinating system services.
 */

// ========== Macros/Defines ==========
#define UI_VERSION "4.1.0"

// ========== System/Library Includes ==========
// Third-party header: keep project warning flags from firing inside it
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#include <LovyanGFX.hpp>
#pragma GCC diagnostic pop
#include <lvgl.h>

// ========== Project Includes ==========
#include "definitions/configs.h"
#include "definitions/constants.h"
#include "utilities/ticker.h"
#include <memory>

// ========== Forward Declarations ==========
class ProviderFactory;
class ManagerFactory;
class DeviceProvider;
class IGpioProvider;
class IDisplayProvider;
class StyleManager;
class IConfigurationManager;
class PanelManager;
class Inputs;
class DisplayController;
class ErrorManager;
class ConfigurationManager;

// ========== Global Variables ==========
// Global factories - dual factory pattern implementation
extern std::unique_ptr<ProviderFactory> provider_factory;
extern std::unique_ptr<ManagerFactory> manager_factory;

// Global providers - created by ProviderFactory
extern std::unique_ptr<DeviceProvider> device_provider;
extern std::unique_ptr<IGpioProvider> gpio_provider;
extern std::unique_ptr<IDisplayProvider> display_provider;

// Global managers - created by ManagerFactory
extern std::unique_ptr<StyleManager> style_manager;
extern std::unique_ptr<IConfigurationManager> preference_manager;
extern std::unique_ptr<PanelManager> panel_manager;
extern std::unique_ptr<Inputs> inputs;
extern std::unique_ptr<DisplayController> display_controller;
extern ErrorManager *error_manager;
extern std::unique_ptr<IConfigurationManager> configuration_manager;

// ========== System Configuration Items ==========
// Inline definitions (C++17) - global scope configuration
inline config::ConfigItem default_panel_config(config_constants::items::DEFAULT_PANEL,
                                               ui_strings::config_labels::DEFAULT_PANEL,
                                               std::string(config_constants::panels::OEM_OIL_PANEL),
                                               config::ConfigMetadata(config_constants::options::PANEL_OPTIONS,
                                                                      config::ConfigItemType::Selection));

inline config::ConfigItem update_rate_config(config_constants::items::UPDATE_RATE,
                                             ui_strings::config_labels::UPDATE_RATE, 500,
                                             config::ConfigMetadata(config_constants::options::SYSTEM_UPDATE_RATES,
                                                                    config_constants::units::MILLISECONDS,
                                                                    config::ConfigItemType::Selection));

inline config::ConfigItem show_splash_config(config_constants::items::SHOW_SPLASH,
                                             ui_strings::config_labels::SHOW_SPLASH, true, config::ConfigMetadata());

// ========== Function Declarations ==========
void RegisterSystemConfiguration();

// Static schema registration for self-registering pattern
class SystemConfig
{
  public:
    static void RegisterConfigSchema(IConfigurationManager *configuration_manager);
};