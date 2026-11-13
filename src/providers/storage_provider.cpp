#include "providers/storage_provider.h"
#include "definitions/constants.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <algorithm>
#include <sstream>

// Helper class for RAII mutex handling
class SemaphoreGuard
{
  private:
    SemaphoreHandle_t &semaphore_;

  public:
    SemaphoreGuard(SemaphoreHandle_t &sem) : semaphore_(sem)
    {
        xSemaphoreTake(semaphore_, portMAX_DELAY);
    }
    ~SemaphoreGuard()
    {
        xSemaphoreGive(semaphore_);
    }
};

// ========== Constructor ==========

/**
 * @brief Constructs PreferenceStorage with thread safety and NVS initialization
 *
 * Initializes the storage system with thread-safe access and NVS backend.
 * Handles NVS initialization errors and prepares storage for configuration data.
 */
StorageProvider::StorageProvider() : config_mutex_(xSemaphoreCreateMutex()) // thread safety semaphore
{
    if (!config_mutex_)
    {
        log_e("Failed to create config mutex");
        ErrorManager::Instance().ReportCriticalError("PreferenceStorage",
                                                     "Failed to create config mutex - thread safety compromised");
        return;
    }

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK)
    {
        log_e("Failed to initialize NVS: %s", esp_err_to_name(err));
        ErrorManager::Instance().ReportCriticalError("PreferenceStorage",
                                                     "Failed to initialize NVS - preferences cannot be saved");
        return;
    }

#ifdef CLARITY_TEST_SEED_INVALID_PANEL
    // Test only: store a removed panel name as the default panel (D-07)
    {
        Preferences seed;
        seed.begin(GetSectionNamespace(config_constants::sections::SYSTEM).c_str(), false);
        seed.putString(config_constants::items::DEFAULT_PANEL, "DiagnosticPanel");
        seed.end();
    }
#endif

    // Create default sections

    // Load all registered sections
    // StorageProvider is never subclassed, so the call cannot reach an override
    // cppcheck-suppress virtualCallInConstructor
    StorageProvider::LoadAllConfigSections();

    log_i("ConfigurationManager initialized");
}

// ========== Dynamic Configuration Implementation ==========

/**
 * @brief Register a configuration section for component self-registration
 * @param section ConfigSection containing items, metadata, and validation rules
 * @return true if registration successful, false if section already exists
 *
 * Implements component self-registration pattern from dynamic-config-implementation.md.
 * Components call this during initialization to register their configuration requirements.
 * The section defines configuration items with types, default values, validation constraints,
 * and UI metadata for automatic menu generation.
 *
 * After registration, any existing persisted values are automatically loaded from NVS,
 * allowing components to immediately access their stored configuration.
 */
bool StorageProvider::RegisterConfigSection(const config::ConfigSection &section)
{
    log_v("RegisterConfigSection() called");
    SemaphoreGuard lock(config_mutex_);

    // Check if section already exists - prevent duplicate registration
    if (registered_sections_.find(section.section_name) != registered_sections_.end())
    {
        log_w("Section '%s' already registered", section.section_name.c_str());
        return false;
    }

    // Store section definition in memory for later access
    registered_sections_[section.section_name] = section;
    log_i("Registered config section: %s", section.section_name.c_str());

    // Load any existing persisted values for this section from NVS
    // This allows components to register config and immediately access stored values
    LoadConfigSection(section.section_name);

    return true;
}

/**
 * @brief Get list of all registered configuration section names
 * @return Vector of section names for UI generation and iteration
 *
 * Used primarily by ConfigPanel for automatic menu generation. The returned
 * list enables dynamic UI creation where menus are built based on what
 * components have registered, rather than hardcoded menu structures.
 */
std::vector<std::string> StorageProvider::GetRegisteredSectionNames() const
{
    SemaphoreGuard lock(config_mutex_);

    // Create vector of pairs (section_name, displayName) for sorting
    std::vector<std::pair<std::string, std::string>> section_pairs;
    section_pairs.reserve(registered_sections_.size());

    for (const auto &[name, section] : registered_sections_)
    {
        section_pairs.emplace_back(name, section.displayName);
    }

    // Sort alphabetically by displayName
    std::sort(section_pairs.begin(), section_pairs.end(),
              [](const auto &a, const auto &b)
              {
                  return a.second < b.second; // Compare displayName (second element)
              });

    // Extract sorted section names
    std::vector<std::string> names;
    names.reserve(section_pairs.size());
    for (const auto &[section_name, display_name] : section_pairs)
    {
        names.push_back(section_name);
    }

    return names;
}

/**
 * @brief Retrieve a specific configuration section by name
 * @param section_name Name of the section to retrieve
 * @return Optional containing section if found, nullopt if not registered
 *
 * Returns the complete section definition including current values and metadata.
 * Used by ConfigPanel to build configuration UIs and by components to access
 * their registered configuration structure.
 */
std::optional<config::ConfigSection> StorageProvider::GetConfigSection(const std::string &section_name) const
{
    SemaphoreGuard lock(config_mutex_);

    auto it = registered_sections_.find(section_name);
    if (it != registered_sections_.end())
    {
        return it->second;
    }

    return std::nullopt;
}

/**
 * @brief Save a specific configuration section to NVS storage
 * @param section_name Name of the section to persist
 * @return true if save operation successful
 *
 * Persists all configuration items in the specified section to sectioned NVS storage.
 * Each section gets its own NVS namespace for organization. Used when components
 * want to save their specific configuration without affecting other sections.
 */
bool StorageProvider::SaveConfigSection(const std::string &section_name)
{
    auto it = registered_sections_.find(section_name);
    if (it == registered_sections_.end())
    {
        log_w("Section not found: %s", section_name.c_str());
        return false;
    }

    const config::ConfigSection &section = it->second;
    std::string ns_name = GetSectionNamespace(section_name);

    preferences_.begin(ns_name.c_str(), false);
    bool success = true;

    for (const auto &item : section.items)
    {
        if (!StoreValueToNVS(preferences_, item.key, item.value))
        {
            log_e("Failed to store config item: %s.%s", section_name.c_str(), item.key.c_str());
            ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "ConfigurationManager",
                                                 "Failed to store config item to NVS");
            success = false;
        }
    }

    preferences_.end();

    return success;
}

/**
 * @brief Load a specific configuration section from NVS storage
 * @param section_name Name of the section to load
 * @return true if load operation successful
 *
 * Loads persisted values from sectioned NVS storage into the in-memory section definition.
 * Called automatically after component registration to restore previous configuration.
 * Updates the registered section with stored values while preserving metadata.
 */
bool StorageProvider::LoadConfigSection(const std::string &section_name)
{
    auto it = registered_sections_.find(section_name);
    if (it == registered_sections_.end())
    {
        return false;
    }

    config::ConfigSection &section = it->second;
    std::string ns_name = GetSectionNamespace(section_name);

    preferences_.begin(ns_name.c_str(), true); // read-only

    for (auto &item : section.items)
    {
        // Only overwrite default if value exists in NVS
        if (preferences_.isKey(item.key.c_str()))
        {
            config::ConfigValue stored = LoadValueFromNVS(preferences_, item.key, item.defaultValue);

            // A stored selection that is no longer an allowed option (for example a
            // removed panel) falls back to the default instead of failing later (D-07)
            if (item.metadata.itemType == config::ConfigItemType::Selection &&
                std::holds_alternative<std::string>(stored) &&
                !ValidateEnumValue(std::get<std::string>(stored), item.metadata.constraints))
            {
                log_w("Stored value '%s' for %s.%s is not an allowed option, using default",
                      std::get<std::string>(stored).c_str(), section_name.c_str(), item.key.c_str());
                log_t("Invalid stored value for %s.%s, using default", section_name.c_str(), item.key.c_str());
                continue;
            }
            item.value = stored;
        }
        // Otherwise preserve the default value in item.value
    }

    preferences_.end();
    return true;
}

/**
 * @brief Save all registered configuration sections to storage
 * @return true if all sections saved successfully
 *
 * Batch operation for complete configuration persistence. Iterates through all
 * registered sections and saves each one to its respective NVS namespace.
 * Used during application shutdown or when performing complete configuration backup.
 */
bool StorageProvider::SaveAllConfigSections()
{
    bool all_success = true;
    for (const auto &[name, section] : registered_sections_)
    {
        if (!SaveConfigSection(name))
        {
            all_success = false;
        }
    }
    return all_success;
}

/**
 * @brief Load all registered configuration sections from storage
 * @return true if all sections loaded successfully
 *
 * Batch operation for complete configuration restoration. Iterates through all
 * registered sections and loads persisted values from their respective NVS namespaces.
 * Called during application startup to restore previous configuration state.
 */
bool StorageProvider::LoadAllConfigSections()
{
    bool all_success = true;
    for (const auto &[name, section] : registered_sections_)
    {
        if (!LoadConfigSection(name))
        {
            all_success = false;
        }
    }
    return all_success;
}

bool StorageProvider::ValidateConfigValue(const std::string &full_key, const config::ConfigValue &value) const
{
    auto [section_name, item_key] = ParseConfigKey(full_key);

    auto section_it = registered_sections_.find(section_name);
    if (section_it == registered_sections_.end())
    {
        return false;
    }

    const config::ConfigSection &section = section_it->second;
    auto item_it = std::find_if(section.items.begin(), section.items.end(),
                                [&item_key](const config::ConfigItem &item) { return item.key == item_key; });

    if (item_it == section.items.end())
    {
        return false;
    }

    const config::ConfigItem &item = *item_it;

    // Type validation - ensure types match between new value and existing value
    if (!TypesMatch(value, item.value))
    {
        log_w("Type mismatch for key %s - expected %s, got %s", full_key.c_str(), GetTypeName(item.value).c_str(),
              GetTypeName(value).c_str());
        return false;
    }

    // Validate based on value type and metadata
    if (std::holds_alternative<int>(value))
    {
        auto val = std::get<int>(value);
        return ValidateIntRange(val, item.metadata.constraints);
    }
    else if (std::holds_alternative<float>(value))
    {
        auto val = std::get<float>(value);
        return ValidateFloatRange(val, item.metadata.constraints);
    }
    else if (std::holds_alternative<std::string>(value))
    {
        auto val = std::get<std::string>(value);
        // For selection items (enum-like), validate against constraints
        if (item.metadata.itemType == config::ConfigItemType::Selection)
        {
            return ValidateEnumValue(val, item.metadata.constraints);
        }
        return true; // Free-form strings always valid
    }
    else if (std::holds_alternative<bool>(value))
    {
        return true; // Booleans always valid
    }

    return false;
}

// ========== Live Update Implementation ==========

uint32_t StorageProvider::RegisterChangeCallback(const std::string &full_key, ConfigChangeCallback callback)
{
    SemaphoreGuard lock(config_mutex_);
    uint32_t callback_id = next_callback_id_++;
    change_callbacks_[callback_id] = std::make_pair(full_key, callback);
    return callback_id;
}

bool StorageProvider::IsSchemaRegistered(const std::string &section_name) const
{
    SemaphoreGuard lock(config_mutex_);
    return registered_sections_.find(section_name) != registered_sections_.end();
}

// ========== Protected Implementation Methods ==========

std::optional<config::ConfigValue> StorageProvider::QueryConfigValue(const std::string &full_key) const
{
    // Parse dot-separated key into section and item (e.g., "oil_temp_sensor.unit")
    auto [section_name, item_key] = ParseConfigKey(full_key);

    // Find the registered section by name
    auto section_it = registered_sections_.find(section_name);
    if (section_it == registered_sections_.end())
    {
        return std::nullopt; // Section not registered
    }

    // Search for the specific configuration item within the section
    const config::ConfigSection &section = section_it->second;
    auto item_it = std::find_if(section.items.begin(), section.items.end(),
                                [&item_key](const config::ConfigItem &item) { return item.key == item_key; });

    if (item_it == section.items.end())
    {
        return std::nullopt; // Item not found in section
    }

    // Return the current value stored in memory
    return item_it->value;
}

bool StorageProvider::UpdateConfigValue(const std::string &full_key, const config::ConfigValue &value)
{
    // Parse dot-separated key into section and item components
    auto [section_name, item_key] = ParseConfigKey(full_key);

    // Find the registered section
    auto section_it = registered_sections_.find(section_name);
    if (section_it == registered_sections_.end())
    {
        log_w("Section not found for key: %s", full_key.c_str());
        return false;
    }

    // Find the specific configuration item within the section
    config::ConfigSection &section = section_it->second;
    auto item_it = std::find_if(section.items.begin(), section.items.end(),
                                [&item_key](config::ConfigItem &item) { return item.key == item_key; });

    if (item_it == section.items.end())
    {
        log_w("Item not found for key: %s", full_key.c_str());
        return false;
    }

    // Store old value for change notifications
    std::optional<config::ConfigValue> old_value = item_it->value;

    // Validate new value against metadata constraints (range, enum options, etc.)
    if (!ValidateConfigValue(full_key, value))
    {
        log_w("Validation failed for key: %s", full_key.c_str());
        return false;
    }

    // Update in-memory value
    item_it->value = value;

    // Persist to sectioned NVS storage - each section gets its own namespace
    std::string ns_name = GetSectionNamespace(section_name);
    preferences_.begin(ns_name.c_str(), false); // false = read/write mode
    bool success = StoreValueToNVS(preferences_, item_key, value);
    preferences_.end();

    if (success)
    {
        log_i("Updated config %s = %s", full_key.c_str(), ToString(value).c_str());

        // Notify all registered callbacks about the configuration change
        // Callbacks can watch specific keys or all changes (empty watchedKey)
        for (const auto &[callback_id, key_callback_pair] : change_callbacks_)
        {
            const std::string &watched_key = key_callback_pair.first;
            const ConfigChangeCallback &callback = key_callback_pair.second;

            // Check if callback watches this key specifically or all changes
            if (watched_key.empty() || watched_key == full_key)
            {
                try
                {
                    // Call registered callback with key, old value, and new value
                    callback(full_key, old_value, value);
                    log_t("Notified callback %u for update: %s", callback_id, full_key.c_str());
                }
                catch (const std::exception &e)
                {
                    log_e("Exception in change callback %u: %s", callback_id, e.what());
                    ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "StorageProvider",
                                                         "Exception in change callback");
                }
            }
        }
    }
    else
    {
        // Rollback in-memory value on NVS write failure
        item_it->value = *old_value;
        log_e("Failed to save config to NVS for key: %s", full_key.c_str());
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "ConfigurationManager",
                                             "Failed to save config value to NVS");
    }

    return success;
}

// ========== Private Helper Methods ==========

std::pair<std::string, std::string> StorageProvider::ParseConfigKey(const std::string &full_key) const
{
    size_t dot_pos = full_key.find('.');
    if (dot_pos == std::string::npos)
    {
        return {"", full_key};
    }
    return {full_key.substr(0, dot_pos), full_key.substr(dot_pos + 1)};
}

/**
 * @brief Generate NVS namespace string for a configuration section
 * @param section_name Name of the configuration section
 * @return NVS namespace string with prefix, truncated if necessary
 *
 * Creates proper NVS namespace by prefixing section name and ensuring
 * it fits within NVS 15-character namespace limit. Truncates if needed.
 */
std::string StorageProvider::GetSectionNamespace(const std::string &section_name) const
{
    std::string ns = section_prefix + section_name;
    if (ns.length() > MAX_NAMESPACE_LEN)
    {
        ns.resize(MAX_NAMESPACE_LEN);
    }
    return ns;
}

/**
 * @brief Validate integer value against range constraints
 * @param value Integer value to validate
 * @param constraints Range string in format "min-max" (e.g., "0-100")
 * @return true if value is within range or constraints are invalid
 *
 * Parses range constraints and validates integer values. Returns true
 * for empty constraints or parsing errors (permissive validation).
 */
bool StorageProvider::ValidateIntRange(int value, const std::string &constraints) const
{
    if (constraints.empty())
    {
        return true;
    }

    size_t dash_pos = constraints.find('-');
    if (dash_pos == std::string::npos)
    {
        return true;
    }

    try
    {
        int min = std::stoi(constraints.substr(0, dash_pos));
        int max = std::stoi(constraints.substr(dash_pos + 1));
        return value >= min && value <= max;
    }
    catch (...)
    {
        return true; // If parsing fails, allow the value
    }
}

/**
 * @brief Validate float value against range constraints
 * @param value Float value to validate
 * @param constraints Range string in format "min-max" (e.g., "0.0-1.0")
 * @return true if value is within range or constraints are invalid
 *
 * Parses range constraints and validates float values. Returns true
 * for empty constraints or parsing errors (permissive validation).
 */
bool StorageProvider::ValidateFloatRange(float value, const std::string &constraints) const
{
    if (constraints.empty())
    {
        return true;
    }

    size_t dash_pos = constraints.find('-');
    if (dash_pos == std::string::npos)
    {
        return true;
    }

    try
    {
        float min = std::stof(constraints.substr(0, dash_pos));
        float max = std::stof(constraints.substr(dash_pos + 1));
        return value >= min && value <= max;
    }
    catch (...)
    {
        return true;
    }
}

/**
 * @brief Validate enum value against allowed options
 * @param value String value to validate
 * @param constraints Comma-separated options (e.g., "PSI,Bar,kPa")
 * @return true if value is in the allowed list or constraints are empty
 *
 * Parses comma-separated options and checks if value is valid.
 * Used for dropdown/enum configuration validation.
 */
bool StorageProvider::ValidateEnumValue(const std::string &value, const std::string &constraints) const
{
    if (constraints.empty())
    {
        return true;
    }

    auto options = ParseOptions(constraints);
    return std::find(options.begin(), options.end(), value) != options.end();
}

/**
 * @brief Parse comma-separated string into vector of options
 * @param str Comma-separated string to parse
 * @return Vector of trimmed, non-empty options
 *
 * Splits string by commas, trims whitespace from each option,
 * and filters out empty entries. Used for enum validation.
 */
std::vector<std::string> StorageProvider::ParseOptions(const std::string &str) const
{
    std::vector<std::string> options;
    std::stringstream ss(str);
    std::string option;

    while (std::getline(ss, option, ','))
    {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        if (!option.empty())
        {
            options.push_back(option);
        }
    }

    return options;
}

/**
 * @brief Store ConfigValue to NVS using type-appropriate method
 * @param prefs Preferences instance for the section namespace
 * @param key Configuration key to store
 * @param value ConfigValue containing the data to store
 * @param type Expected configuration value type
 * @return true if storage successful, false on type mismatch or NVS error
 *
 * Converts ConfigValue to appropriate NVS type and stores using type-specific
 * putters. Handles type safety by validating ConfigValue content matches expected type.
 */
bool StorageProvider::StoreValueToNVS(Preferences &prefs, const std::string &key, const config::ConfigValue &value)
{
    if (std::holds_alternative<int>(value))
    {
        return prefs.putInt(key.c_str(), std::get<int>(value));
    }
    else if (std::holds_alternative<float>(value))
    {
        return prefs.putFloat(key.c_str(), std::get<float>(value));
    }
    else if (std::holds_alternative<bool>(value))
    {
        return prefs.putBool(key.c_str(), std::get<bool>(value));
    }
    else if (std::holds_alternative<std::string>(value))
    {
        return prefs.putString(key.c_str(), std::get<std::string>(value).c_str());
    }

    return false;
}

/**
 * @brief Load configuration value from NVS storage and convert to ConfigValue
 * @param prefs Preferences instance for the section namespace
 * @param key Configuration key to load
 * @param type Expected configuration value type for type-safe loading
 * @return ConfigValue containing the loaded value or default value if not found
 *
 * Loads type-specific values from NVS using appropriate getter methods.
 * Returns sensible defaults for missing values (0 for numbers, false for bool, empty for string).
 * Handles both String and Enum types as strings since enums are stored as their string values.
 * Returns std::monostate for unknown types to indicate invalid/unhandled configuration.
 */
config::ConfigValue StorageProvider::LoadValueFromNVS(Preferences &prefs, const std::string &key,
                                                      const config::ConfigValue &template_value)
{
    if (std::holds_alternative<int>(template_value))
    {
        return prefs.getInt(key.c_str(), std::get<int>(template_value));
    }
    else if (std::holds_alternative<float>(template_value))
    {
        return prefs.getFloat(key.c_str(), std::get<float>(template_value));
    }
    else if (std::holds_alternative<bool>(template_value))
    {
        return prefs.getBool(key.c_str(), std::get<bool>(template_value));
    }
    else if (std::holds_alternative<std::string>(template_value))
    {
        return std::string(prefs.getString(key.c_str(), std::get<std::string>(template_value).c_str()).c_str());
    }

    return std::monostate{};
}

// ========== Configuration Value Helper Methods ==========
// (Moved from ConfigValueHelper class for better encapsulation)

/**
 * @brief Determine the type category of a ConfigValue
 * @param value The ConfigValue to check
 * @return Type name as string for UI/logging
 */
std::string StorageProvider::GetTypeName(const config::ConfigValue &value) const
{
    return std::visit(
        [](const auto &v) -> std::string
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                return config_constants::types::UNSET;
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                return config_constants::types::INTEGER_INTERNAL;
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                return config_constants::types::FLOAT_INTERNAL;
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                return config_constants::types::BOOLEAN_INTERNAL;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return config_constants::types::STRING_INTERNAL;
            }
            return config_constants::types::UNKNOWN;
        },
        value);
}

/**
 * @brief Check if value type matches another value's type
 */
bool StorageProvider::TypesMatch(const config::ConfigValue &a, const config::ConfigValue &b) const
{
    return a.index() == b.index();
}

/**
 * @brief Convert a ConfigValue to string representation
 * @param value The ConfigValue to convert
 * @return String representation of the value
 */
std::string StorageProvider::ToString(const config::ConfigValue &value) const
{
    return std::visit(
        [](const auto &v) -> std::string
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                return config_constants::boolean_values::EMPTY_STRING;
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                return v ? config_constants::boolean_values::TRUE_STRING
                         : config_constants::boolean_values::FALSE_STRING;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return v;
            }
            else
            {
                return std::to_string(v);
            }
        },
        value);
}

/**
 * @brief Parse a string into a ConfigValue, inferring type from existing value
 * @param str The string to parse
 * @param templateValue Value whose type to match
 * @return ConfigValue containing the parsed value
 */
config::ConfigValue StorageProvider::FromString(const std::string &str, const config::ConfigValue &template_value) const
{
    return std::visit(
        [&str](const auto &v) -> config::ConfigValue
        {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, std::monostate>)
            {
                // Try to infer: if it looks like a number, parse as int
                // Otherwise return as string
                try
                {
                    return std::stoi(str);
                }
                catch (...)
                {
                    return str;
                }
            }
            else if constexpr (std::is_same_v<T, int>)
            {
                try
                {
                    return std::stoi(str);
                }
                catch (...)
                {
                    return std::monostate{};
                }
            }
            else if constexpr (std::is_same_v<T, float>)
            {
                try
                {
                    return std::stof(str);
                }
                catch (...)
                {
                    return std::monostate{};
                }
            }
            else if constexpr (std::is_same_v<T, bool>)
            {
                return (str == config_constants::boolean_values::TRUE_STRING ||
                        str == config_constants::boolean_values::TRUE_NUMERIC);
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return str;
            }
            return std::monostate{};
        },
        template_value);
}

/**
 * @brief Check if a value is numeric (int or float)
 */
bool StorageProvider::IsNumeric(const config::ConfigValue &value) const
{
    return std::holds_alternative<int>(value) || std::holds_alternative<float>(value);
}