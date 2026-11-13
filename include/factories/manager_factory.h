#pragma once

#include "interfaces/i_configuration_manager.h"
#include "interfaces/i_display_provider.h"
#include "interfaces/i_gpio_provider.h"
#include "interfaces/i_panel_manager.h"
#include "interfaces/i_style_manager.h"
#include <Arduino.h>
#include <memory>

// Forward declarations
class PanelManager;
class StyleManager;
class DeviceProvider;
class ConfigurationManager;
class ErrorManager;
class ProviderFactory;

/**
 * @class ManagerFactory
 * @brief Concrete factory for creating manager instances with error handling and logging
 *
 * @details This factory implements IManagerFactory interface and provides methods
 * for creating all manager types used in the Clarity system. It uses IProviderFactory
 * to obtain hardware providers, implementing the dual factory pattern for clear
 * separation of concerns. Each factory method includes proper error handling,
 * null checking, dependency validation, and debug logging for initialization tracking.
 *
 * @design_pattern Dual Factory Pattern (Concrete Factory using Abstract Provider Factory)
 * @error_handling All methods return nullptr on failure with error logging
 * @logging Debug level logging for successful creations, error level for failures
 * @dependency_validation All required dependencies are validated before construction
 * @testability Implements IManagerFactory interface and accepts IProviderFactory for test injection
 */
class ManagerFactory
{
  public:
    // ========== Constructors and Destructor ==========
    explicit ManagerFactory(std::unique_ptr<ProviderFactory> provider_factory);
    ManagerFactory();
    ~ManagerFactory() = default;

    // ========== Public Interface Methods ==========
    // Factory methods
    std::unique_ptr<PanelManager> CreatePanelManager(IDisplayProvider *display, IGpioProvider *gpio,
                                                     IStyleManager *style_manager,
                                                     IConfigurationManager *configuration_manager);

    std::unique_ptr<StyleManager> CreateStyleManager(const char *theme = nullptr);

    std::unique_ptr<IConfigurationManager> CreateConfigurationManager();

    ErrorManager *CreateErrorManager();

  private:
    // ========== Private Methods ==========
    bool InitializeProviders();

    // ========== Static Methods ==========
    static std::unique_ptr<PanelManager> CreatePanelManagerImpl(IDisplayProvider *display, IGpioProvider *gpio,
                                                                IStyleManager *style_manager,
                                                                IConfigurationManager *configuration_manager);

    static std::unique_ptr<StyleManager> CreateStyleManagerImpl(const char *theme = nullptr);

    static std::unique_ptr<IConfigurationManager> CreateConfigurationManagerImpl();

    static ErrorManager *CreateErrorManagerImpl();

    // ========== Private Data Members ==========
    std::unique_ptr<ProviderFactory> provider_factory_;

    std::unique_ptr<IGpioProvider> gpio_provider_;
    std::unique_ptr<IDisplayProvider> display_provider_;
    std::unique_ptr<DeviceProvider> device_provider_;
};