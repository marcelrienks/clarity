#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <Arduino.h>
#include <algorithm>

// ========== Static Methods ==========

/**
 * @brief Get singleton instance using Meyer's singleton pattern
 * @details Thread-safe in C++11+ due to static local variable initialization
 */
ErrorManager &ErrorManager::Instance()
{
    static ErrorManager instance;
    return instance;
}

// ========== Public Interface Methods ==========

/**
 * @brief Core error reporting implementation
 * @details Creates ErrorInfo object, adds to queue with timestamp, trims queue if needed
 */
void ErrorManager::ReportError(ErrorLevel level, const char *source, const std::string &message)
{
    // Process auto-dismiss before adding new error
    AutoDismissOldWarnings();

    // Create error info structure
    ErrorInfo error_info;
    error_info.level = level;
    error_info.source = source;
    error_info.SetMessage(message);
    error_info.timestamp = millis();
    error_info.acknowledged = false;

    // Add to queue
    error_queue_.push_back(error_info);

    // Trim queue if necessary to maintain memory constraints
    TrimErrorQueue();

    // Log error with appropriate level
    [[maybe_unused]] const char *level_str = (level == ErrorLevel::WARNING) ? "WARNING"
                                             : (level == ErrorLevel::ERROR) ? "ERROR"
                                                                            : "CRITICAL";
    log_e("[%s] %s: %s", level_str, source, errorInfo.message);
    log_t("Error reported: %s", level_str);
}

/**
 * @brief Convenience method for reporting warnings
 */
void ErrorManager::ReportWarning(const char *source, const std::string &message)
{
    ReportError(ErrorLevel::WARNING, source, message);
}

/**
 * @brief Convenience method for reporting critical errors
 */
void ErrorManager::ReportCriticalError(const char *source, const std::string &message)
{
    ReportError(ErrorLevel::CRITICAL, source, message);
}

/**
 * @brief Simple check for any errors in queue
 */
bool ErrorManager::HasPendingErrors() const
{
    return !error_queue_.empty();
}

/**
 * @brief Check for unacknowledged critical errors using STL algorithm
 */
bool ErrorManager::HasCriticalErrors() const
{
    return std::any_of(error_queue_.begin(), error_queue_.end(), [](const ErrorInfo &error)
                       { return error.level == ErrorLevel::CRITICAL && !error.acknowledged; });
}

/**
 * @brief Return copy of error queue to prevent external modification
 */
std::vector<ErrorInfo> ErrorManager::GetErrorQueue() const
{
    return error_queue_;
}

/**
 * @brief Acknowledge error and apply removal policy based on severity
 * @details Warnings/errors are removed immediately, critical errors remain until explicit clear
 */
void ErrorManager::AcknowledgeError(size_t error_index)
{
    if (error_index < error_queue_.size())
    {
        error_queue_[error_index].acknowledged = true;

        // Apply removal policy: remove non-critical errors immediately
        // Critical errors remain visible until explicitly cleared
        if (error_queue_[error_index].level != ErrorLevel::CRITICAL)
        {
            error_queue_.erase(error_queue_.begin() + error_index);
        }
    }
}

/**
 * @brief Force clear all errors and reset error panel state
 */
void ErrorManager::ClearAllErrors()
{
    error_queue_.clear();
    error_panel_active_ = false;
}

/**
 * @brief Determine if error panel should be triggered
 * @details Processes auto-dismiss first, then checks for pending errors
 * @note Uses const_cast for internal cleanup operation in const method
 */
bool ErrorManager::ShouldTriggerErrorPanel() const
{
    // Process auto-dismiss first (const_cast for this internal operation)
    const_cast<ErrorManager *>(this)->AutoDismissOldWarnings();

    // Always trigger if we have pending errors - let the trigger system handle the logic
    // of whether to actually switch panels
    return HasPendingErrors();
}

/**
 * @brief Update error panel activation state
 */
void ErrorManager::SetErrorPanelActive(bool active)
{
    error_panel_active_ = active;
}

/**
 * @brief Check current error panel activation state
 */
bool ErrorManager::IsErrorPanelActive() const
{
    return error_panel_active_;
}

// ========== Private Methods ==========

/**
 * @brief Trim error queue to maximum size using priority-based sorting
 * @details Sorts by error level priority (critical > error > warning) then by timestamp (newer first)
 */
void ErrorManager::TrimErrorQueue()
{
    if (error_queue_.size() > MAX_ERROR_QUEUE_SIZE)
    {
        // Sort by priority (critical > error > warning) and timestamp (newer first)
        // Higher error level enum values indicate higher priority
        std::sort(error_queue_.begin(), error_queue_.end(),
                  [](const ErrorInfo &a, const ErrorInfo &b)
                  {
                      if (a.level != b.level)
                      {
                          return static_cast<int>(a.level) > static_cast<int>(b.level);
                      }
                      return a.timestamp > b.timestamp;
                  });

        // Keep only the most recent/important errors within memory constraints
        error_queue_.resize(MAX_ERROR_QUEUE_SIZE);
    }
}

/**
 * @brief Auto-dismiss warnings older than timeout
 */
void ErrorManager::AutoDismissOldWarnings()
{
    auto now = millis();
    auto it = error_queue_.begin();
    while (it != error_queue_.end())
    {
        if (it->level == ErrorLevel::WARNING && (now - it->timestamp) > WARNING_AUTO_DISMISS_TIME)
        {
            it = error_queue_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
