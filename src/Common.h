#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>

// Windows headers
#include <windows.h>
#include <winsock2.h>
#include <shellapi.h>
#include <commctrl.h>

// Firebird
#include <ibase.h>

// LiteSQL
#include <litesql.hpp>

namespace dbsync {

// 版本信息
constexpr const char* VERSION = "1.0.0";
constexpr int DEFAULT_SYNC_PORT = 15555;
constexpr int DEFAULT_MONITOR_INTERVAL_MS = 1000;

// 同步方向
enum class SyncDirection {
    LOCAL_TO_REMOTE,
    REMOTE_TO_LOCAL,
    BIDIRECTIONAL
};

// 变更类型
enum class ChangeType {
    INSERT,
    UPDATE,
    DELETE
};

// 同步状态
enum class SyncStatus {
    PENDING,
    SYNCING,
    SUCCESS,
    FAILED,
    CONFLICT
};

// 数据结构定义
struct DatabaseConfig {
    std::string host;
    int port;
    std::string database;
    std::string username;
    std::string password;
    std::string charset;
    bool embedded;  // 是否使用嵌入式模式
};

struct NetworkConfig {
    std::string localIp;
    int localPort;
    std::string remoteIp;
    int remotePort;
    int connectionTimeoutMs;
    int retryIntervalMs;
};

struct SyncConfig {
    bool autoStart;
    bool minimizeToTray;
    bool startWithWindows;
    int syncIntervalMs;
    int maxRetries;
    bool resolveConflictsAutomatically;
    std::string conflictResolutionStrategy; // "timestamp", "priority", "manual"
    std::vector<std::string> syncTables;
    bool syncAllTables;
};

struct ChangeRecord {
    std::string id;
    std::string tableName;
    ChangeType changeType;
    std::string primaryKey;
    std::string primaryKeyValue;
    std::string data;
    std::string timestamp;
    std::string sourceNode;
    SyncStatus status;
    int retryCount;
};

struct SyncMessage {
    int messageType;  // 1: change, 2: heartbeat, 3: sync_request, 4: sync_response
    std::string sourceNode;
    std::string targetNode;
    std::string payload;
    std::string timestamp;
};

// 工具函数
class Utils {
public:
    static std::string GetCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    static std::string GenerateUUID() {
        GUID guid;
        CoCreateGuid(&guid);
        char buffer[40];
        sprintf_s(buffer, "%08X-%04X-%04X-%04X-%012X",
            guid.Data1, guid.Data2, guid.Data3,
            (guid.Data4[0] << 8) | guid.Data4[1],
            ((guid.Data4[2] << 24) | (guid.Data4[3] << 16) | 
             (guid.Data4[4] << 8) | guid.Data4[5]));
        return std::string(buffer);
    }
    
    static std::string WideToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
        return result;
    }
    
    static std::wstring Utf8ToWide(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
        return result;
    }
};

} // namespace dbsync
