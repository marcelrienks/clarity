#pragma once

#include "definitions/configs.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

/**
 * @interface IConfigurationManager
 * @brief Modern dynamic configuration interface for component self-registration
 *
 * @details This interface provides a complete dynamic configuration system
 * that enables components to self-register their configuration requirements,
 * automatic UI generation, and type-safe configuration access.
 *
 * Based on docs/plans/dynamic-config-implementation.md design:
 * - Component self-registration of configuration sections
 * - Type-safe configuration access with templates
 * - Metadata-driven UI generation
 * - Sectioned NVS storage organization
 * - Live configuration updates with callbacks
 *
 * @design_principles:
 * - Component self-registration for configuration needs
 * - Type-safe configuration access with templates
 * - Metadata-driven UI generation
 * - Sectioned storage organization
 * - No backwards compatibility - clean modern design
 */
class IConfigurationManager
{
  public:
    virtual ~IConfigurationManager() = default;

    // ========== Live Update Callback Types ==========

    using ConfigChangeCallback =
        std::function<void(const std::string &full_key, const std::optional<config::ConfigValue> &old_value,
                           const config::ConfigValue &new_value)>;

    // ========== Dynamic Configuration Registration ==========

    virtual bool RegisterConfigSection(const config::ConfigSection &section) = 0;

    // ========== Type-Safe Configuration Access ==========

    template <typename T> std::optional<T> QueryConfig(const std::string &full_key) const
    {
        // Delegate to implementation method for actual config lookup
        auto value_opt = QueryConfigImpl(full_key);
        if (!value_opt)
        {
            return std::nullopt;
        }

        // Use GetValue method for type-safe value extraction
        return GetValue<T>(*value_opt);
    }

    template <typename T> bool UpdateConfig(const std::string &full_key, const T &value)
    {
        // Convert typed value to ConfigValue variant and delegate to implementation
        return UpdateConfigImpl(full_key, config::ConfigValue(value));
    }

    // ========== Section Access Methods ==========

    virtual std::vector<std::string> GetRegisteredSectionNames() const = 0;

    virtual std::optional<config::ConfigSection> GetConfigSection(const std::string &section_name) const = 0;

    // ========== Persistence Methods ==========

    virtual bool SaveConfigSection(const std::string &section_name) = 0;

    virtual bool LoadConfigSection(const std::string &section_name) = 0;

    virtual bool SaveAllConfigSections() = 0;

    virtual bool LoadAllConfigSections() = 0;

    // ========== Validation Methods ==========

    virtual bool ValidateConfigValue(const std::string &full_key, const config::ConfigValue &value) const = 0;

    // ========== Live Update Methods ==========

    virtual uint32_t RegisterChangeCallback(const std::string &full_key, ConfigChangeCallback callback) = 0;

    // ========== Schema Query Methods ==========

    /**
     * @brief Check if a configuration schema is registered
     * @param section_name Name of the section to check
     * @return true if schema is registered, false otherwise
     *
     * Used to determine if a schema has already been registered,
     * useful for backward compatibility during migration.
     */
    virtual bool IsSchemaRegistered(const std::string &section_name) const = 0;

    // ========== Configuration Value Helper Methods ==========
    // (Moved from ConfigValueHelper class for better encapsulation)

    virtual std::string GetTypeName(const config::ConfigValue &value) const = 0;
    virtual bool TypesMatch(const config::ConfigValue &a, const config::ConfigValue &b) const = 0;
    virtual std::string ToString(const config::ConfigValue &value) const = 0;
    virtual config::ConfigValue FromString(const std::string &str, const config::ConfigValue &template_value) const = 0;
    virtual bool IsNumeric(const config::ConfigValue &value) const = 0;

    template <typename T> std::optional<T> GetValue(const config::ConfigValue &value) const
    {
        if (std::holds_alternative<T>(value))
        {
            return std::get<T>(value);
        }
        return std::nullopt;
    }

  protected:
    // ========== Implementation Methods ==========
    // These are protected to allow template methods to work

    virtual std::optional<config::ConfigValue> QueryConfigImpl(const std::string &full_key) const = 0;

    virtual bool UpdateConfigImpl(const std::string &full_key, const config::ConfigValue &value) = 0;
};