#pragma once

#include <cstdarg>
#include <cstdio>

class Logger {
    const size_t BUF_SIZE = 2048;
public:
    enum class LogLevel {
        Debug,
        Info,
        Warning,
        Error,
        Critical
    };

    void debug(const char *fmt, ...) {
        char buf[BUF_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, BUF_SIZE, fmt, args);
        va_end(args);
        logMessage(LogLevel::Debug, buf);
    }

    void info(const char *fmt, ...) {
        char buf[BUF_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, BUF_SIZE, fmt, args);
        va_end(args);
        logMessage(LogLevel::Info, buf);
    }

    void warning(const char *fmt, ...) {
        char buf[BUF_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, BUF_SIZE, fmt, args);
        va_end(args);
        logMessage(LogLevel::Warning, buf);
    }

    void error(const char *fmt, ...) {
        char buf[BUF_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, BUF_SIZE, fmt, args);
        va_end(args);
        logMessage(LogLevel::Error, buf);
    }

    void critical(const char *fmt, ...) {
        char buf[BUF_SIZE];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, BUF_SIZE, fmt, args);
        va_end(args);
        logMessage(LogLevel::Critical, buf);
    }

    virtual void logMessage(LogLevel level, const char *messageText) = 0;
};

class StdoutLogger : public Logger {
public:
    void logMessage(LogLevel level, const char *messageText) override {
        switch (level) {
        case LogLevel::Debug:
            printf("DEBUG: %s\n", messageText);
            break;
        case LogLevel::Info:
            printf("INFO: %s\n", messageText);
            break;
        case LogLevel::Warning:
            printf("WARN: %s\n", messageText);
            break;
        case LogLevel::Error:
            printf("ERROR: %s\n", messageText);
            break;
        case LogLevel::Critical:
            printf("CRITICAL: %s\n", messageText);
            break;
        }
    }
};
