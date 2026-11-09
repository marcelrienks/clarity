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
    ErrorInfo errorInfo;
    errorInfo.level = level;
    errorInfo.source = source;
    errorInfo.SetMessage(message);
    errorInfo.timestamp = millis();
    errorInfo.acknowledged = false;

    // Add to queue
    errorQueue_.push_back(errorInfo);

    // Trim queue if necessary to maintain memory constraints
    TrimErrorQueue();

    // Log error with appropriate level
    [[maybe_unused]] const char *levelStr = (level == ErrorLevel::WARNING) ? "WARNING"
                           : (level == ErrorLevel::ERROR) ? "ERROR"
                                                          : "CRITICAL";
    log_e("[%s] %s: %s", levelStr, source, errorInfo.message);
    log_t("Error reported: %s", levelStr);
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
    return !errorQueue_.empty();
}

/**
 * @brief Check for unacknowledged critical errors using STL algorithm
 */
bool ErrorManager::HasCriticalErrors() const
{
    return std::any_of(errorQueue_.begin(), errorQueue_.end(), [](const ErrorInfo &error)
                       { return error.level == ErrorLevel::CRITICAL && !error.acknowledged; });
}

/**
 * @brief Return copy of error queue to prevent external modification
 */
std::vector<ErrorInfo> ErrorManager::GetErrorQueue() const
{
    return errorQueue_;
}

/**
 * @brief Acknowledge error and apply removal policy based on severity
 * @details Warnings/errors are removed immediately, critical errors remain until explicit clear
 */
void ErrorManager::AcknowledgeError(size_t errorIndex)
{
    if (errorIndex < errorQueue_.size())
    {
        errorQueue_[errorIndex].acknowledged = true;

        // Apply removal policy: remove non-critical errors immediately
        // Critical errors remain visible until explicitly cleared
        if (errorQueue_[errorIndex].level != ErrorLevel::CRITICAL)
        {
            errorQueue_.erase(errorQueue_.begin() + errorIndex);
        }

    }
}

/**
 * @brief Force clear all errors and reset error panel state
 */
void ErrorManager::ClearAllErrors()
{
    errorQueue_.clear();
    errorPanelActive_ = false;
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
    errorPanelActive_ = active;
}

/**
 * @brief Check current error panel activation state
 */
bool ErrorManager::IsErrorPanelActive() const
{
    return errorPanelActive_;
}

// ========== Private Methods ==========

/**
 * @brief Trim error queue to maximum size using priority-based sorting
 * @details Sorts by error level priority (critical > error > warning) then by timestamp (newer first)
 */
void ErrorManager::TrimErrorQueue()
{
    if (errorQueue_.size() > MAX_ERROR_QUEUE_SIZE)
    {
        // Sort by priority (critical > error > warning) and timestamp (newer first)
        // Higher error level enum values indicate higher priority
        std::sort(errorQueue_.begin(), errorQueue_.end(),
                  [](const ErrorInfo &a, const ErrorInfo &b)
                  {
                      if (a.level != b.level)
                      {
                          return static_cast<int>(a.level) > static_cast<int>(b.level);
                      }
                      return a.timestamp > b.timestamp;
                  });

        // Keep only the most recent/important errors within memory constraints
        errorQueue_.resize(MAX_ERROR_QUEUE_SIZE);
    }
}

/**
 * @brief Auto-dismiss warnings older than timeout
 */
void ErrorManager::AutoDismissOldWarnings()
{
    auto now = millis();
    auto it = errorQueue_.begin();
    while (it != errorQueue_.end())
    {
        if (it->level == ErrorLevel::WARNING &&
            (now - it->timestamp) > WARNING_AUTO_DISMISS_TIME)
        {
            it = errorQueue_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
