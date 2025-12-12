#pragma once

#include <cstdio>
#include <cstdarg>

enum class LogLevel {
    Info,
    Warn,
    Error
};

inline void logMessage(LogLevel level, const char* fmt, ...) {
    const char* prefix = "";
    switch (level) {
    case LogLevel::Info:  prefix = "[INFO] "; break;
    case LogLevel::Warn:  prefix = "[WARN] "; break;
    case LogLevel::Error: prefix = "[ERROR]"; break;
    }

    std::fprintf(stderr, "%s ", prefix);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stderr, fmt, args);
    va_end(args);
}
