#include "managers/configuration_manager.h"
#include "providers/storage_provider.h"
#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <esp32-hal-log.h>

// Static instance for singleton pattern
static ConfigurationManager* configManagerInstancePtr_ = nullptr;

// ========== Constructors and Destructor ==========

/**
 * @brief Construct ConfigurationManager and set singleton pointer
 * @details Initializes the manager but does not create storage yet
 */
ConfigurationManager::ConfigurationManager() : initialized_(false)
{
    log_v("ConfigurationManager() constructor called");

    // Set singleton instance for static access
    configManagerInstancePtr_ = this;
}

/**
 * @brief Destructor - clean up singleton pointer and storage
 */
ConfigurationManager::~ConfigurationManager()
{
    log_v("~ConfigurationManager() destructor called");

    // Clear singleton instance if we are the current instance
    if (configManagerInstancePtr_ == this) {
        configManagerInstancePtr_ = nullptr;
    }
}

// ========== Static Methods ==========

/**
 * @brief Singleton access for manager architecture
 * @details Must be initialized by ManagerFactory before use
 */
ConfigurationManager& ConfigurationManager::Instance() {
    if (!configManagerInstancePtr_) {
        log_e("ConfigurationManager::Instance() called before initialization");
        ErrorManager::Instance().ReportCriticalError("ConfigurationManager",
                                                     "Instance() called before initialization - configuration will not function");
    }
    return *configManagerInstancePtr_;
}

/**
 * @brief Get the function collection using Construct-On-First-Use idiom
 * @return Reference to the static vector of registration functions
 *
 * This pattern ensures the vector exists before any static initialization
 * tries to register schemas, avoiding the static initialization order fiasco.
 */
std::vector<void(*)(IConfigurationManager*)>& ConfigurationManager::GetSchemaFunctions() {
    static std::vector<void(*)(IConfigurationManager*)> functions;
    return functions;
}

/**
 * @brief Add a configuration schema function for later execution
 * @param func Function pointer that will register a component's configuration schema
 *
 * Called during static initialization by components.
 * Functions are stored and executed later when RegisterAllSchemas() is called.
 */
void ConfigurationManager::AddSchema(void(*func)(IConfigurationManager*)) {
    if (!func) {
        log_e("ConfigurationManager: Cannot add null schema function");
        return;
    }

    GetSchemaFunctions().push_back(func);
}

// ========== Public Interface Methods ==========

/**
 * @brief Initialize the configuration manager with storage
 * @return true if initialization successful
 *
 * Creates the internal StorageProvider backend for storage operations.
 */
bool ConfigurationManager::Initialize() {
    if (storageProvider_) {
        log_w("ConfigurationManager already initialized");
        return true;
    }

    storageProvider_ = std::make_unique<StorageProvider>();
    if (!storageProvider_) {
        log_e("Failed to create StorageProvider");
        ErrorManager::Instance().ReportCriticalError("ConfigurationManager",
                                                     "Failed to create storage backend");
        return false;
    }

    log_i("ConfigurationManager initialized with storage backend");
    return true;
}

/**
 * @brief Execute all registered schema functions
 *
 * Called once during application setup to register all component schemas.
 * Uses this instance as the service, delegating to internal storage.
 */
void ConfigurationManager::RegisterAllSchemas() {
    if (!EnsureStorageReady()) {
        log_e("Cannot register schemas - storage not initialized");
        return;
    }

    if (initialized_) {
        log_w("Schemas already registered - skipping duplicate registration");
        return;
    }

    auto& functions = GetSchemaFunctions();
    log_i("Registering %zu configuration schemas", functions.size());

    size_t successCount = 0;
    for (auto func : functions) {
        if (func) {
            // Pass this ConfigurationManager as the IConfigurationManager
            func(this);
            successCount++;
        }
    }

    initialized_ = true;
    log_i("Configuration registration complete: %zu/%zu schemas registered successfully",
          successCount, totalFunctions);

    // Load all configuration sections from NVS after registration
    if (!LoadAllConfigSections()) {
        log_w("Some configuration sections failed to load from storage");
    }
}

// ========== IConfigurationManager Implementation ==========

/**
 * @brief Register a configuration section with the manager
 * @param section Configuration section containing items and metadata
 * @return true if registration successful, false otherwise
 *
 * Delegates to the storage provider to register the section schema.
 * This defines the available configuration items and their constraints.
 */
bool ConfigurationManager::RegisterConfigSection(const Config::ConfigSection& section) {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->RegisterConfigSection(section);
}

/**
 * @brief Get list of all registered configuration section names
 * @return Vector of section names, empty if no sections registered
 *
 * Returns the names of all configuration sections that have been registered
 * with the manager. Used by UI components to enumerate available sections.
 */
std::vector<std::string> ConfigurationManager::GetRegisteredSectionNames() const {
    if (!EnsureStorageReady()) return {};
    return storageProvider_->GetRegisteredSectionNames();
}

/**
 * @brief Retrieve a specific configuration section by name
 * @param sectionName Name of the section to retrieve
 * @return Optional containing the section if found, nullopt otherwise
 *
 * Returns the complete configuration section including all items and metadata.
 * Used by UI components to build configuration interfaces.
 */
std::optional<Config::ConfigSection> ConfigurationManager::GetConfigSection(const std::string& sectionName) const {
    if (!EnsureStorageReady()) return std::nullopt;
    return storageProvider_->GetConfigSection(sectionName);
}

/**
 * @brief Save a configuration section to persistent storage
 * @param sectionName Name of the section to save
 * @return true if save successful, false otherwise
 *
 * Persists the current values of all items in the specified section to NVS.
 * This ensures configuration changes survive device restarts.
 */
bool ConfigurationManager::SaveConfigSection(const std::string& sectionName) {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->SaveConfigSection(sectionName);
}

/**
 * @brief Load a configuration section from persistent storage
 * @param sectionName Name of the section to load
 * @return true if load successful, false otherwise
 *
 * Restores saved values for all items in the specified section from NVS.
 * Uses default values for any items not found in storage.
 */
bool ConfigurationManager::LoadConfigSection(const std::string& sectionName) {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->LoadConfigSection(sectionName);
}

/**
 * @brief Save all registered configuration sections to persistent storage
 * @return true if all sections saved successfully, false otherwise
 *
 * Persists the current values of all registered configuration sections to NVS.
 * Provides a convenient way to save the entire configuration state at once.
 */
bool ConfigurationManager::SaveAllConfigSections() {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->SaveAllConfigSections();
}

/**
 * @brief Load all registered configuration sections from persistent storage
 * @return true if all sections loaded successfully, false otherwise
 *
 * Restores saved values for all registered configuration sections from NVS.
 * Called during initialization to restore the complete configuration state.
 */
bool ConfigurationManager::LoadAllConfigSections() {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->LoadAllConfigSections();
}

/**
 * @brief Validate a configuration value against its schema constraints
 * @param fullKey Fully qualified key (section.item) to validate
 * @param value Configuration value to validate
 * @return true if value is valid for the key, false otherwise
 *
 * Checks if the provided value meets the constraints defined in the schema
 * for the specified configuration item. Used to prevent invalid values.
 */
bool ConfigurationManager::ValidateConfigValue(const std::string& fullKey, const Config::ConfigValue& value) const {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->ValidateConfigValue(fullKey, value);
}

/**
 * @brief Register a callback function for configuration value changes
 * @param fullKey Fully qualified key (section.item) to monitor
 * @param callback Function to call when the value changes
 * @return Callback ID for later removal, 0 if registration failed
 *
 * Enables components to be notified when specific configuration values change.
 * The callback receives the old and new values for processing.
 */
uint32_t ConfigurationManager::RegisterChangeCallback(const std::string& fullKey, ConfigChangeCallback callback) {
    if (!EnsureStorageReady()) return 0;
    return storageProvider_->RegisterChangeCallback(fullKey, callback);
}

/**
 * @brief Check if a configuration schema is registered
 * @param sectionName Name of the section to check
 * @return true if schema is registered, false otherwise
 *
 * Verifies whether a configuration section schema has been registered.
 * Used to prevent duplicate registrations and validate section existence.
 */
bool ConfigurationManager::IsSchemaRegistered(const std::string& sectionName) const {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->IsSchemaRegistered(sectionName);
}

// Configuration Value Helper Methods

/**
 * @brief Get the type name of a configuration value
 * @param value Configuration value to examine
 * @return String representation of the value's type
 *
 * Returns a human-readable type name for the configuration value.
 * Used for debugging and validation messages.
 */
std::string ConfigurationManager::GetTypeName(const Config::ConfigValue& value) const {
    if (!EnsureStorageReady()) return "unknown";
    return storageProvider_->GetTypeName(value);
}

/**
 * @brief Check if two configuration values have matching types
 * @param a First configuration value to compare
 * @param b Second configuration value to compare
 * @return true if both values have the same type, false otherwise
 *
 * Compares the types of two configuration values without comparing their content.
 * Used for type validation during configuration updates.
 */
bool ConfigurationManager::TypesMatch(const Config::ConfigValue& a, const Config::ConfigValue& b) const {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->TypesMatch(a, b);
}

/**
 * @brief Convert a configuration value to its string representation
 * @param value Configuration value to convert
 * @return String representation of the value
 *
 * Converts any supported configuration value type to a string format.
 * Used for displaying values in UI and for storage serialization.
 */
std::string ConfigurationManager::ToString(const Config::ConfigValue& value) const {
    if (!EnsureStorageReady()) return "";
    return storageProvider_->ToString(value);
}

/**
 * @brief Convert a string to a configuration value of the specified type
 * @param str String representation to convert
 * @param templateValue Template value providing the target type information
 * @return Configuration value of the same type as template, or monostate if conversion fails
 *
 * Parses a string and converts it to the same type as the template value.
 * Used for deserializing values from storage and processing user input.
 */
Config::ConfigValue ConfigurationManager::FromString(const std::string& str, const Config::ConfigValue& templateValue) const {
    if (!EnsureStorageReady()) return std::monostate{};
    return storageProvider_->FromString(str, templateValue);
}

/**
 * @brief Check if a configuration value is numeric
 * @param value Configuration value to check
 * @return true if value is numeric (int, float, etc.), false otherwise
 *
 * Determines whether a configuration value contains numeric data.
 * Used for validation and UI formatting decisions.
 */
bool ConfigurationManager::IsNumeric(const Config::ConfigValue& value) const {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->IsNumeric(value);
}

// Protected Implementation Methods

/**
 * @brief Internal implementation for querying configuration values
 * @param fullKey Fully qualified key (section.item) to query
 * @return Optional containing the value if found, nullopt otherwise
 *
 * Protected implementation method for the template QueryConfig method.
 * Handles the actual storage query and returns the raw ConfigValue.
 */
std::optional<Config::ConfigValue> ConfigurationManager::QueryConfigImpl(const std::string& fullKey) const {
    if (!EnsureStorageReady()) return std::nullopt;
    return storageProvider_->QueryConfigValue(fullKey);
}

/**
 * @brief Internal implementation for updating configuration values
 * @param fullKey Fully qualified key (section.item) to update
 * @param value New configuration value to set
 * @return true if update successful, false otherwise
 *
 * Protected implementation method for the template UpdateConfig method.
 * Handles validation, storage update, and change notification callbacks.
 */
bool ConfigurationManager::UpdateConfigImpl(const std::string& fullKey, const Config::ConfigValue& value) {
    if (!EnsureStorageReady()) return false;
    return storageProvider_->UpdateConfigValue(fullKey, value);
}

// ========== Private Methods ==========

/**
 * @brief Ensure storage is initialized before operations
 * @return true if storage is ready
 */
bool ConfigurationManager::EnsureStorageReady() const {
    if (!storageProvider_) {
        log_e("Storage provider not initialized - call Initialize() first");
        return false;
    }
    return true;
}