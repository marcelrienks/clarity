#include "managers/error_manager.h"
#include "utilities/logging.h"
#include <Arduino.h>

namespace
{
clarity::core::ErrorLevel ToCoreLevel(ErrorLevel level)
{
    switch (level)
    {
        case ErrorLevel::WARNING:
            return clarity::core::ErrorLevel::Warning;
        case ErrorLevel::ERROR:
            return clarity::core::ErrorLevel::Error;
        case ErrorLevel::CRITICAL:
            break;
    }
    return clarity::core::ErrorLevel::Critical;
}
} // namespace

ErrorManager &ErrorManager::Instance()
{
    static ErrorManager instance;
    return instance;
}

void ErrorManager::ReportError(ErrorLevel level, const char *source, const std::string &message)
{
    log_.Report(millis(), ToCoreLevel(level), clarity::core::Fault::Unclassified, source, "%s", message.c_str());

    [[maybe_unused]] const char *level_str = (level == ErrorLevel::WARNING) ? "WARNING"
                                             : (level == ErrorLevel::ERROR) ? "ERROR"
                                                                            : "CRITICAL";
    log_e("[%s] %s: %s", level_str, source, message.c_str());
    log_t("Error reported: %s", level_str);
}

void ErrorManager::ReportWarning(const char *source, const std::string &message)
{
    ReportError(ErrorLevel::WARNING, source, message);
}

void ErrorManager::ReportCriticalError(const char *source, const std::string &message)
{
    ReportError(ErrorLevel::CRITICAL, source, message);
}

bool ErrorManager::HasPendingErrors() const
{
    return log_.HasActive();
}

void ErrorManager::ClearAllErrors()
{
    log_.Clear();
}

bool ErrorManager::ShouldTriggerErrorPanel()
{
    log_.Expire(millis());
    return log_.HasActive();
}
