#include "providers/storage_provider.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include "definitions/constants.h"
#include <algorithm>
#include <sstream>

// Helper class for RAII mutex handling
class SemaphoreGuard {
private:
    SemaphoreHandle_t& semaphore_;
public:
    SemaphoreGuard(SemaphoreHandle_t& sem) : semaphore_(sem) {
        xSemaphoreTake(semaphore_, portMAX_DELAY);
    }
    ~SemaphoreGuard() {
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
StorageProvider::StorageProvider() {
    // Initialize thread safety semaphore
    configMutex_ = xSemaphoreCreateMutex();
    if (!configMutex_) {
        log_e("Failed to create config mutex");
        ErrorManager::Instance().ReportCriticalError("PreferenceStorage",
                                                     "Failed to create config mutex - thread safety compromised");
        return;
    }

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }

    if (err != ESP_OK) {
        log_e("Failed to initialize NVS: %s", esp_err_to_name(err));
        ErrorManager::Instance().ReportCriticalError("PreferenceStorage",
                                                     "Failed to initialize NVS - preferences cannot be saved");
        return;
    }

#ifdef CLARITY_TEST_SEED_INVALID_PANEL
    // Test only: store a removed panel name as the default panel (D-07)
    {
        Preferences seed;
        seed.begin(GetSectionNamespace(ConfigConstants::Sections::SYSTEM).c_str(), false);
        seed.putString(ConfigConstants::Items::DEFAULT_PANEL, "DiagnosticPanel");
        seed.end();
    }
#endif

#ifdef CLARITY_TEST_SEED_INVALID_SPLASH
    // Test only: store a non-numeric splash duration (D-23)
    {
        Preferences seed;
        seed.begin(GetSectionNamespace(ConfigConstants::Sections::SPLASH_PANEL).c_str(), false);
        seed.putString(ConfigConstants::Items::DURATION, "abc");
        seed.end();
    }
#endif

    // Create default sections

    // Load all registered sections
    LoadAllConfigSections();

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
bool StorageProvider::RegisterConfigSection(const Config::ConfigSection& section) {
    log_v("RegisterConfigSection() called");
    SemaphoreGuard lock(configMutex_);

    // Check if section already exists - prevent duplicate registration
    if (registeredSections_.find(section.sectionName) != registeredSections_.end()) {
        log_w("Section '%s' already registered", section.sectionName.c_str());
        return false;
    }

    // Store section definition in memory for later access
    registeredSections_[section.sectionName] = section;
    log_i("Registered config section: %s", section.sectionName.c_str());

    // Load any existing persisted values for this section from NVS
    // This allows components to register config and immediately access stored values
    LoadConfigSection(section.sectionName);

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
std::vector<std::string> StorageProvider::GetRegisteredSectionNames() const {
    SemaphoreGuard lock(configMutex_);

    // Create vector of pairs (sectionName, displayName) for sorting
    std::vector<std::pair<std::string, std::string>> sectionPairs;
    sectionPairs.reserve(registeredSections_.size());

    for (const auto& [name, section] : registeredSections_) {
        sectionPairs.emplace_back(name, section.displayName);
    }

    // Sort alphabetically by displayName
    std::sort(sectionPairs.begin(), sectionPairs.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;  // Compare displayName (second element)
        });

    // Extract sorted section names
    std::vector<std::string> names;
    names.reserve(sectionPairs.size());
    for (const auto& [sectionName, displayName] : sectionPairs) {
        names.push_back(sectionName);
    }

    return names;
}

/**
 * @brief Retrieve a specific configuration section by name
 * @param sectionName Name of the section to retrieve
 * @return Optional containing section if found, nullopt if not registered
 *
 * Returns the complete section definition including current values and metadata.
 * Used by ConfigPanel to build configuration UIs and by components to access
 * their registered configuration structure.
 */
std::optional<Config::ConfigSection> StorageProvider::GetConfigSection(const std::string& sectionName) const {
    SemaphoreGuard lock(configMutex_);

    auto it = registeredSections_.find(sectionName);
    if (it != registeredSections_.end()) {
        return it->second;
    }

    return std::nullopt;
}

/**
 * @brief Save a specific configuration section to NVS storage
 * @param sectionName Name of the section to persist
 * @return true if save operation successful
 *
 * Persists all configuration items in the specified section to sectioned NVS storage.
 * Each section gets its own NVS namespace for organization. Used when components
 * want to save their specific configuration without affecting other sections.
 */
bool StorageProvider::SaveConfigSection(const std::string& sectionName) {
    auto it = registeredSections_.find(sectionName);
    if (it == registeredSections_.end()) {
        log_w("Section not found: %s", sectionName.c_str());
        return false;
    }

    const Config::ConfigSection& section = it->second;
    std::string nsName = GetSectionNamespace(sectionName);

    preferences_.begin(nsName.c_str(), false);
    bool success = true;

    for (const auto& item : section.items) {
        if (!StoreValueToNVS(preferences_, item.key, item.value)) {
            log_e("Failed to store config item: %s.%s", sectionName.c_str(), item.key.c_str());
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
 * @param sectionName Name of the section to load
 * @return true if load operation successful
 *
 * Loads persisted values from sectioned NVS storage into the in-memory section definition.
 * Called automatically after component registration to restore previous configuration.
 * Updates the registered section with stored values while preserving metadata.
 */
bool StorageProvider::LoadConfigSection(const std::string& sectionName) {
    auto it = registeredSections_.find(sectionName);
    if (it == registeredSections_.end()) {
        return false;
    }

    Config::ConfigSection& section = it->second;
    std::string nsName = GetSectionNamespace(sectionName);

    preferences_.begin(nsName.c_str(), true); // read-only

    for (auto& item : section.items) {
        // Only overwrite default if value exists in NVS
        if (preferences_.isKey(item.key.c_str())) {
            Config::ConfigValue stored = LoadValueFromNVS(preferences_, item.key, item.defaultValue);

            // A stored selection that is no longer an allowed option (for example a
            // removed panel) falls back to the default instead of failing later (D-07)
            if (item.metadata.itemType == Config::ConfigItemType::Selection &&
                std::holds_alternative<std::string>(stored) &&
                !ValidateEnumValue(std::get<std::string>(stored), item.metadata.constraints)) {
                log_w("Stored value '%s' for %s.%s is not an allowed option, using default",
                      std::get<std::string>(stored).c_str(), sectionName.c_str(), item.key.c_str());
                log_t("Invalid stored value for %s.%s, using default", sectionName.c_str(), item.key.c_str());
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
bool StorageProvider::SaveAllConfigSections() {
    bool allSuccess = true;
    for (const auto& [name, section] : registeredSections_) {
        if (!SaveConfigSection(name)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

/**
 * @brief Load all registered configuration sections from storage
 * @return true if all sections loaded successfully
 *
 * Batch operation for complete configuration restoration. Iterates through all
 * registered sections and loads persisted values from their respective NVS namespaces.
 * Called during application startup to restore previous configuration state.
 */
bool StorageProvider::LoadAllConfigSections() {
    bool allSuccess = true;
    for (const auto& [name, section] : registeredSections_) {
        if (!LoadConfigSection(name)) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool StorageProvider::ValidateConfigValue(const std::string& fullKey, const Config::ConfigValue& value) const {
    auto [sectionName, itemKey] = ParseConfigKey(fullKey);

    auto sectionIt = registeredSections_.find(sectionName);
    if (sectionIt == registeredSections_.end()) {
        return false;
    }

    const Config::ConfigSection& section = sectionIt->second;
    auto itemIt = std::find_if(section.items.begin(), section.items.end(),
        [&itemKey](const Config::ConfigItem& item) { return item.key == itemKey; });

    if (itemIt == section.items.end()) {
        return false;
    }

    const Config::ConfigItem& item = *itemIt;

    // Type validation - ensure types match between new value and existing value
    if (!TypesMatch(value, item.value)) {
        log_w("Type mismatch for key %s - expected %s, got %s",
              fullKey.c_str(),
              GetTypeName(item.value).c_str(),
              GetTypeName(value).c_str());
        return false;
    }

    // Validate based on value type and metadata
    if (std::holds_alternative<int>(value)) {
        auto val = std::get<int>(value);
        return ValidateIntRange(val, item.metadata.constraints);
    }
    else if (std::holds_alternative<float>(value)) {
        auto val = std::get<float>(value);
        return ValidateFloatRange(val, item.metadata.constraints);
    }
    else if (std::holds_alternative<std::string>(value)) {
        auto val = std::get<std::string>(value);
        // For selection items (enum-like), validate against constraints
        if (item.metadata.itemType == Config::ConfigItemType::Selection) {
            return ValidateEnumValue(val, item.metadata.constraints);
        }
        return true; // Free-form strings always valid
    }
    else if (std::holds_alternative<bool>(value)) {
        return true; // Booleans always valid
    }

    return false;
}

// ========== Live Update Implementation ==========

uint32_t StorageProvider::RegisterChangeCallback(const std::string& fullKey, ConfigChangeCallback callback) {
    SemaphoreGuard lock(configMutex_);
    uint32_t callbackId = nextCallbackId_++;
    changeCallbacks_[callbackId] = std::make_pair(fullKey, callback);
    return callbackId;
}

bool StorageProvider::IsSchemaRegistered(const std::string& sectionName) const {
    SemaphoreGuard lock(configMutex_);
    return registeredSections_.find(sectionName) != registeredSections_.end();
}

// ========== Protected Implementation Methods ==========

std::optional<Config::ConfigValue> StorageProvider::QueryConfigValue(const std::string& fullKey) const {
    // Parse dot-separated key into section and item (e.g., "oil_temp_sensor.unit")
    auto [sectionName, itemKey] = ParseConfigKey(fullKey);

    // Find the registered section by name
    auto sectionIt = registeredSections_.find(sectionName);
    if (sectionIt == registeredSections_.end()) {
        return std::nullopt; // Section not registered
    }

    // Search for the specific configuration item within the section
    const Config::ConfigSection& section = sectionIt->second;
    auto itemIt = std::find_if(section.items.begin(), section.items.end(),
        [&itemKey](const Config::ConfigItem& item) { return item.key == itemKey; });

    if (itemIt == section.items.end()) {
        return std::nullopt; // Item not found in section
    }

    // Return the current value stored in memory
    return itemIt->value;
}

bool StorageProvider::UpdateConfigValue(const std::string& fullKey, const Config::ConfigValue& value) {
    // Parse dot-separated key into section and item components
    auto [sectionName, itemKey] = ParseConfigKey(fullKey);

    // Find the registered section
    auto sectionIt = registeredSections_.find(sectionName);
    if (sectionIt == registeredSections_.end()) {
        log_w("Section not found for key: %s", fullKey.c_str());
        return false;
    }

    // Find the specific configuration item within the section
    Config::ConfigSection& section = sectionIt->second;
    auto itemIt = std::find_if(section.items.begin(), section.items.end(),
        [&itemKey](Config::ConfigItem& item) { return item.key == itemKey; });

    if (itemIt == section.items.end()) {
        log_w("Item not found for key: %s", fullKey.c_str());
        return false;
    }

    // Store old value for change notifications
    std::optional<Config::ConfigValue> oldValue = itemIt->value;

    // Validate new value against metadata constraints (range, enum options, etc.)
    if (!ValidateConfigValue(fullKey, value)) {
        log_w("Validation failed for key: %s", fullKey.c_str());
        return false;
    }

    // Update in-memory value
    itemIt->value = value;

    // Persist to sectioned NVS storage - each section gets its own namespace
    std::string nsName = GetSectionNamespace(sectionName);
    preferences_.begin(nsName.c_str(), false); // false = read/write mode
    bool success = StoreValueToNVS(preferences_, itemKey, value);
    preferences_.end();

    if (success) {
        log_i("Updated config %s = %s", fullKey.c_str(),
              ToString(value).c_str());

        // Notify all registered callbacks about the configuration change
        // Callbacks can watch specific keys or all changes (empty watchedKey)
        for (const auto& [callbackId, keyCallbackPair] : changeCallbacks_) {
            const std::string& watchedKey = keyCallbackPair.first;
            const ConfigChangeCallback& callback = keyCallbackPair.second;

            // Check if callback watches this key specifically or all changes
            if (watchedKey.empty() || watchedKey == fullKey) {
                try {
                    // Call registered callback with key, old value, and new value
                    callback(fullKey, oldValue, value);
                    log_t("Notified callback %u for update: %s", callbackId, fullKey.c_str());
                } catch (const std::exception& e) {
                    log_e("Exception in change callback %u: %s", callbackId, e.what());
                    ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "StorageProvider",
                                                        "Exception in change callback");
                }
            }
        }
    } else {
        // Rollback in-memory value on NVS write failure
        itemIt->value = *oldValue;
        log_e("Failed to save config to NVS for key: %s", fullKey.c_str());
        ErrorManager::Instance().ReportError(ErrorLevel::ERROR, "ConfigurationManager",
                                            "Failed to save config value to NVS");
    }

    return success;
}

// ========== Private Helper Methods ==========

std::pair<std::string, std::string> StorageProvider::ParseConfigKey(const std::string& fullKey) const {
    size_t dotPos = fullKey.find('.');
    if (dotPos == std::string::npos) {
        return {"", fullKey};
    }
    return {fullKey.substr(0, dotPos), fullKey.substr(dotPos + 1)};
}

/**
 * @brief Generate NVS namespace string for a configuration section
 * @param sectionName Name of the configuration section
 * @return NVS namespace string with prefix, truncated if necessary
 *
 * Creates proper NVS namespace by prefixing section name and ensuring
 * it fits within NVS 15-character namespace limit. Truncates if needed.
 */
std::string StorageProvider::GetSectionNamespace(const std::string& sectionName) const {
    std::string ns = SECTION_PREFIX_ + sectionName;
    if (ns.length() > MAX_NAMESPACE_LEN_) {
        ns = ns.substr(0, MAX_NAMESPACE_LEN_);
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
bool StorageProvider::ValidateIntRange(int value, const std::string& constraints) const {
    if (constraints.empty()) return true;

    size_t dashPos = constraints.find('-');
    if (dashPos == std::string::npos) return true;

    try {
        int min = std::stoi(constraints.substr(0, dashPos));
        int max = std::stoi(constraints.substr(dashPos + 1));
        return value >= min && value <= max;
    } catch (...) {
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
bool StorageProvider::ValidateFloatRange(float value, const std::string& constraints) const {
    if (constraints.empty()) return true;

    size_t dashPos = constraints.find('-');
    if (dashPos == std::string::npos) return true;

    try {
        float min = std::stof(constraints.substr(0, dashPos));
        float max = std::stof(constraints.substr(dashPos + 1));
        return value >= min && value <= max;
    } catch (...) {
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
bool StorageProvider::ValidateEnumValue(const std::string& value, const std::string& constraints) const {
    if (constraints.empty()) return true;

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
std::vector<std::string> StorageProvider::ParseOptions(const std::string& str) const {
    std::vector<std::string> options;
    std::stringstream ss(str);
    std::string option;

    while (std::getline(ss, option, ',')) {
        // Trim whitespace
        option.erase(0, option.find_first_not_of(" \t"));
        option.erase(option.find_last_not_of(" \t") + 1);
        if (!option.empty()) {
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
bool StorageProvider::StoreValueToNVS(Preferences& prefs, const std::string& key,
                                               const Config::ConfigValue& value) {
    if (std::holds_alternative<int>(value)) {
        return prefs.putInt(key.c_str(), std::get<int>(value));
    }
    else if (std::holds_alternative<float>(value)) {
        return prefs.putFloat(key.c_str(), std::get<float>(value));
    }
    else if (std::holds_alternative<bool>(value)) {
        return prefs.putBool(key.c_str(), std::get<bool>(value));
    }
    else if (std::holds_alternative<std::string>(value)) {
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
Config::ConfigValue StorageProvider::LoadValueFromNVS(Preferences& prefs, const std::string& key,
                                                               const Config::ConfigValue& templateValue) {
    if (std::holds_alternative<int>(templateValue)) {
        return prefs.getInt(key.c_str(), std::get<int>(templateValue));
    }
    else if (std::holds_alternative<float>(templateValue)) {
        return prefs.getFloat(key.c_str(), std::get<float>(templateValue));
    }
    else if (std::holds_alternative<bool>(templateValue)) {
        return prefs.getBool(key.c_str(), std::get<bool>(templateValue));
    }
    else if (std::holds_alternative<std::string>(templateValue)) {
        return std::string(prefs.getString(key.c_str(), std::get<std::string>(templateValue).c_str()).c_str());
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
std::string StorageProvider::GetTypeName(const Config::ConfigValue& value) const {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return ConfigConstants::Types::UNSET;
        } else if constexpr (std::is_same_v<T, int>) {
            return ConfigConstants::Types::INTEGER_INTERNAL;
        } else if constexpr (std::is_same_v<T, float>) {
            return ConfigConstants::Types::FLOAT_INTERNAL;
        } else if constexpr (std::is_same_v<T, bool>) {
            return ConfigConstants::Types::BOOLEAN_INTERNAL;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return ConfigConstants::Types::STRING_INTERNAL;
        }
        return ConfigConstants::Types::UNKNOWN;
    }, value);
}

/**
 * @brief Check if value type matches another value's type
 */
bool StorageProvider::TypesMatch(const Config::ConfigValue& a, const Config::ConfigValue& b) const {
    return a.index() == b.index();
}

/**
 * @brief Convert a ConfigValue to string representation
 * @param value The ConfigValue to convert
 * @return String representation of the value
 */
std::string StorageProvider::ToString(const Config::ConfigValue& value) const {
    return std::visit([](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return ConfigConstants::BooleanValues::EMPTY_STRING;
        } else if constexpr (std::is_same_v<T, bool>) {
            return v ? ConfigConstants::BooleanValues::TRUE_STRING : ConfigConstants::BooleanValues::FALSE_STRING;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return v;
        } else {
            return std::to_string(v);
        }
    }, value);
}

/**
 * @brief Parse a string into a ConfigValue, inferring type from existing value
 * @param str The string to parse
 * @param templateValue Value whose type to match
 * @return ConfigValue containing the parsed value
 */
Config::ConfigValue StorageProvider::FromString(const std::string& str, const Config::ConfigValue& templateValue) const {
    return std::visit([&str](const auto& v) -> Config::ConfigValue {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            // Try to infer: if it looks like a number, parse as int
            // Otherwise return as string
            try {
                return std::stoi(str);
            } catch (...) {
                return str;
            }
        } else if constexpr (std::is_same_v<T, int>) {
            try {
                return std::stoi(str);
            } catch (...) {
                return std::monostate{};
            }
        } else if constexpr (std::is_same_v<T, float>) {
            try {
                return std::stof(str);
            } catch (...) {
                return std::monostate{};
            }
        } else if constexpr (std::is_same_v<T, bool>) {
            return (str == ConfigConstants::BooleanValues::TRUE_STRING || str == ConfigConstants::BooleanValues::TRUE_NUMERIC);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return str;
        }
        return std::monostate{};
    }, templateValue);
}

/**
 * @brief Check if a value is numeric (int or float)
 */
bool StorageProvider::IsNumeric(const Config::ConfigValue& value) const {
    return std::holds_alternative<int>(value) || std::holds_alternative<float>(value);
}