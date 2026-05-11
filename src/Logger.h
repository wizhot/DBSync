#pragma once

#include "Common.h"

namespace dbsync {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }
    
    void Initialize(const std::string& logFilePath);
    void SetLogLevel(LogLevel level);
    void SetMaxFileSize(size_t maxSize);
    void SetMaxFiles(int maxFiles);
    
    void Log(LogLevel level, const std::string& message);
    void Debug(const std::string& message);
    void Info(const std::string& message);
    void Warning(const std::string& message);
    void Error(const std::string& message);
    void Fatal(const std::string& message);
    
private:
    Logger() = default;
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void RotateLogFile();
    std::string GetLogLevelString(LogLevel level);
    
    std::mutex mutex_;
    std::ofstream logFile_;
    std::string logFilePath_;
    LogLevel logLevel_ = LogLevel::INFO;
    size_t maxFileSize_ = 10 * 1024 * 1024; // 10MB
    int maxFiles_ = 5;
    size_t currentFileSize_ = 0;
};

// 宏定义方便使用
#define LOG_DEBUG(msg) dbsync::Logger::GetInstance().Debug(msg)
#define LOG_INFO(msg) dbsync::Logger::GetInstance().Info(msg)
#define LOG_WARNING(msg) dbsync::Logger::GetInstance().Warning(msg)
#define LOG_ERROR(msg) dbsync::Logger::GetInstance().Error(msg)
#define LOG_FATAL(msg) dbsync::Logger::GetInstance().Fatal(msg)

} // namespace dbsync
