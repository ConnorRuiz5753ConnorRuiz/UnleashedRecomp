#pragma once

#include <os/logger_detail.h>

#define LOG_IMPL(type, func, str)       os::logger::Log(str, os::logger::detail::ELogType::type, func)
#define LOGF_IMPL(type, func, str, ...) os::logger::Log(std::format(str, __VA_ARGS__), os::logger::detail::ELogType::type, func)

// Function-specific logging.

#define LOG(str)               LOG_IMPL(None, __func__, str)
#define LOG_UTILITY(str)       LOG_IMPL(Utility, __func__, str)
#define LOG_WARNING(str)       LOG_IMPL(Warning, __func__, str)
#define LOG_ERROR(str)         LOG_IMPL(Error, __func__, str)

#define LOGF(str, ...)         LOGF_IMPL(None, __func__, str, __VA_ARGS__)
#define LOGF_UTILITY(str, ...) LOGF_IMPL(Utility, __func__, str, __VA_ARGS__)
#define LOGF_WARNING(str, ...) LOGF_IMPL(Warning, __func__, str, __VA_ARGS__)
#define LOGF_ERROR(str, ...)   LOGF_IMPL(Error, __func__, str, __VA_ARGS__)

// Non-function-specific logging.

#define LOGN(str)               LOG_IMPL(None, "*", str)
#define LOGN_UTILITY(str)       LOG_IMPL(Utility, "*", str)
#define LOGN_WARNING(str)       LOG_IMPL(Warning, "*", str)
#define LOGN_ERROR(str)         LOG_IMPL(Error, "*", str)

#define LOGFN(str, ...)         LOGF_IMPL(None, "*", str, __VA_ARGS__)
#define LOGFN_UTILITY(str, ...) LOGF_IMPL(Utility, "*", str, __VA_ARGS__)
#define LOGFN_WARNING(str, ...) LOGF_IMPL(Warning, "*", str, __VA_ARGS__)
#define LOGFN_ERROR(str, ...)   LOGF_IMPL(Error, "*", str, __VA_ARGS__)

namespace os::logger
{
    void Log(const std::string& str, detail::ELogType type = detail::ELogType::None, const char* func = nullptr);
}
