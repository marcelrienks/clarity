#pragma once

#include <string>

#include "clarity/core/error_log.h"
#include "definitions/types.h"

/**
 * @class ErrorManager
 * @brief Global error reporting service: the application's one ErrorLog behind the legacy report API
 *
 * @details Singleton until phase 7 gives the composition root the log. Reports from anywhere go into the
 * `clarity::core::ErrorLog` (bounded, least important dropped first, warnings expire after 10 s); the error panel
 * reads and dismisses entries through Log() and keeps no copy. The main loop shows the error panel while
 * ShouldTriggerErrorPanel() is true.
 */
class ErrorManager
{
  public:
    ErrorManager(const ErrorManager &) = delete;
    ErrorManager &operator=(const ErrorManager &) = delete;
    ~ErrorManager() = default;

    static ErrorManager &Instance();

    void ReportError(ErrorLevel level, const char *source, const std::string &message);
    void ReportWarning(const char *source, const std::string &message);
    void ReportCriticalError(const char *source, const std::string &message);
    bool HasPendingErrors() const;
    void ClearAllErrors();

    /// Expires old warnings, then says whether any error is pending
    bool ShouldTriggerErrorPanel();

    clarity::core::ErrorLog &Log()
    {
        return log_;
    }

  private:
    ErrorManager() = default;

    clarity::core::ErrorLog log_;
};
