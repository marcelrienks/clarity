#pragma once

#include "interfaces/i_configuration_manager.h"

/**
 * @interface IConfig
 * @brief Interface for components that register configuration items
 *
 * @details Components that have configurable settings should inherit from this
 * interface and implement the RegisterConfig method to register their
 * configuration items with the preference service.
 */
class IConfig
{
  public:
    virtual ~IConfig() = default;

    /**
     * @brief Register configuration items with the preference service
     * @param configurationManager The preference service to register configs with
     */
    virtual void RegisterConfig(IConfigurationManager *configuration_manager) = 0;
};