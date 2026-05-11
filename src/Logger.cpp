#include "Logger.h"
#include <filesystem>

namespace dbsync {

void Logger::Initialize(const std::string& logFilePath) {
    std::lock_guard<std::mutex> lock(mutex_);
    logFilePath_ = logFilePath;
    
    // 确保日志目录存在
    std::filesystem::path path(logFilePath);
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    
    // 打开日志文件
    logFile_.open(logFilePath, std::ios::app);
    if (logFile_.is_open()) {
        logFile_.seekp(0, std::ios::end);
        currentFileSize_ = logFile_.tellp();
    }
}

void Logger::SetLogLevel(LogLevel level) {
    logLevel_ = level;
}

void Logger::SetMaxFileSize(size_t maxSize) {
    maxFileSize_ = maxSize;
}

void Logger::SetMaxFiles(int maxFiles) {
    maxFiles_ = maxFiles;
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (level < logLevel_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!logFile_.is_open()) {
        return;
    }
    
    // 检查是否需要轮转
    if (currentFileSize_ >= maxFileSize_) {
        RotateLogFile();
    }
    
    // 写入日志
    std::string logEntry = "[" + Utils::GetCurrentTimestamp() + "] " 
                          + "[" + GetLogLevelString(level) + "] " 
                          + message + "\n";
    
    logFile_ << logEntry;
    logFile_.flush();
    currentFileSize_ += logEntry.length();
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::DEBUG, message);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::INFO, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::WARNING, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::ERROR, message);
}

void Logger::Fatal(const std::string& message) {
    Log(LogLevel::FATAL, message);
}

void Logger::RotateLogFile() {
    logFile_.close();
    
    // 删除最旧的日志文件
    std::string oldestFile = logFilePath_ + "." + std::to_string(maxFiles_ - 1);
    if (std::filesystem::exists(oldestFile)) {
        std::filesystem::remove(oldestFile);
    }
    
    // 重命名现有的日志文件
    for (int i = maxFiles_ - 2; i >= 0; --i) {
        std::string oldName = (i == 0) ? logFilePath_ : logFilePath_ + "." + std::to_string(i);
        std::string newName = logFilePath_ + "." + std::to_string(i + 1);
        if (std::filesystem::exists(oldName)) {
            std::filesystem::rename(oldName, newName);
        }
    }
    
    // 重新打开日志文件
    logFile_.open(logFilePath_, std::ios::app);
    currentFileSize_ = 0;
}

std::string Logger::GetLogLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

} // namespace dbsync
