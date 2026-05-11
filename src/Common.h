/**
 * @file Common.h
 * @brief 公共头文件
 * @details 定义项目中各模块共用的数据结构、常量和工具函数。
 *          包括 DatabaseConfig、NetworkConfig 等核心配置结构体。
 */

#pragma once

// ==================== Windows 平台头文件 ====================
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

// ==================== 标准库头文件 ====================
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <cstdint>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

// ==================== 第三方库头文件 ====================
#include <sqlite3.h>
#include "ibase_minimal.h"
#include <json/json.h>

namespace dbsync {

// ==================== 工具函数 ====================

/**
 * @brief 获取当前时间戳（毫秒级）
 * @return 时间戳字符串
 */
inline std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
       << "." << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

/**
 * @brief 获取当前时间戳（Unix 时间戳，秒）
 * @return Unix 时间戳
 */
inline int64_t GetCurrentUnixTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch()).count();
}

// ==================== 枚举类型 ====================

/**
 * @brief 变更操作类型
 */
enum class ChangeType {
    INSERT,     ///< 插入
    UPDATE,     ///< 更新
    DELETE      ///< 删除
};

/**
 * @brief 同步状态
 */
enum class SyncStatus {
    PENDING,    ///< 待同步
    SYNCING,    ///< 同步中
    SUCCESS,    ///< 同步成功
    FAILED,     ///< 同步失败
    CONFLICT    ///< 冲突
};

/**
 * @brief 日志级别
 */
enum class LogLevel {
    DEBUG,      ///< 调试信息
    INFO,       ///< 一般信息
    WARNING,    ///< 警告
    ERROR       ///< 错误
};

// ==================== 核心数据结构 ====================

/**
 * @brief 数据库连接配置
 * @details 描述一个数据库的连接参数，同时支持 SQLite 和 Firebird
 */
struct DatabaseConfig {
    std::string host;           ///< 主机地址（Firebird 远程连接时使用）
    int port;                   ///< 端口号（Firebird 远程连接时使用，默认 3050）
    std::string database;       ///< 数据库文件路径
    std::string username;       ///< 用户名（Firebird 使用）
    std::string password;       ///< 密码（Firebird 使用）
    std::string charset;        ///< 字符集编码（默认 UTF8）
    bool embedded;              ///< 是否使用嵌入式模式（Firebird Embedded）
    std::string dbType;         ///< 数据库类型: "auto", "sqlite", "firebird"
    std::string encryptionKey;  ///< SQLite 数据库加密密钥（空字符串表示不加密）
};

/**
 * @brief 网络配置
 * @details 描述网络通信参数
 */
struct NetworkConfig {
    std::string localIp;            ///< 本地 IP 地址
    int localPort;                  ///< 本地监听端口
    std::string remoteIp;           ///< 远程服务器 IP 地址
    int remotePort;                 ///< 远程服务器端口
    int connectionTimeoutMs;        ///< 连接超时（毫秒）
    int heartbeatIntervalMs;        ///< 心跳间隔（毫秒）
    int maxRetryCount;              ///< 最大重试次数
    int retryIntervalMs;            ///< 重试间隔（毫秒）
};

/**
 * @brief 同步配置
 * @details 描述同步行为的全局配置
 */
struct SyncConfig {
    int syncIntervalMs;                     ///< 同步间隔（毫秒）
    std::string mappingFile;                ///< 映射配置文件路径
    std::string conflictResolutionStrategy; ///< 冲突解决策略: "timestamp", "source_wins", "target_wins"
    bool resolveConflictsAutomatically;     ///< 是否自动解决冲突
    int maxSyncRetryCount;                  ///< 最大同步重试次数
    bool enableNetworkSync;                 ///< 是否启用网络同步
};

/**
 * @brief 同步消息结构体
 * @details 用于网络节点间的同步通信
 */
struct SyncMessage {
    int messageType;            ///< 消息类型: 0=SYNC_REQUEST, 1=SYNC_DATA, 2=HEARTBEAT, 3=ACK
    std::string sourceNode;     ///< 源节点 ID
    std::string targetNode;     ///< 目标节点 ID
    std::string payload;        ///< 消息内容
    std::string timestamp;      ///< 时间戳

    enum class Type {
        SYNC_REQUEST,        ///< 同步请求
        SYNC_DATA,           ///< 同步数据
        CHANGE_NOTIFICATION, ///< 变更通知
        HEARTBEAT,           ///< 心跳
        ACK                  ///< 确认
    };
    Type type;                ///< 消息类型（兼容旧版枚举）
};

/**
 * @brief 变更记录结构体（Firebird 使用）
 * @details 描述一条数据库变更记录
 */
struct ChangeRecord {
    std::string id;              ///< 变更ID
    std::string tableName;       ///< 表名
    ChangeType changeType;       ///< 变更类型
    std::string primaryKey;      ///< 主键字段名
    std::string primaryKeyValue; ///< 主键值
    std::string data;            ///< 变更数据（JSON格式）
    std::string timestamp;       ///< 变更时间
    std::string sourceNode;      ///< 源节点
    SyncStatus status;           ///< 同步状态
    int retryCount;              ///< 重试次数
};

/**
 * @brief 变更记录结构体（SQLite 使用）
 * @details 描述一条 SQLite 数据库变更记录
 */
struct SqliteChangeRecord {
    int64_t change_id;          ///< 变更日志ID
    std::string table_name;     ///< 表名
    std::string operation;      ///< 操作类型: INSERT / UPDATE / DELETE
    std::string primary_key;    ///< 主键值
    std::string change_time;    ///< 变更时间
    std::string old_data;       ///< 变更前的数据（JSON格式，仅UPDATE/DELETE）
    std::string new_data;       ///< 变更后的数据（JSON格式，仅INSERT/UPDATE）
};

} // namespace dbsync

// ==================== 日志宏 ====================

/**
 * @brief 日志宏定义
 * @note 需要在包含此文件前定义，或在 Logger.h 中实现
 */
#ifndef LOG_DEBUG
#define LOG_DEBUG(msg) do { std::cout << "[DEBUG] " << msg << std::endl; } while(0)
#endif

#ifndef LOG_INFO
#define LOG_INFO(msg) do { std::cout << "[INFO] " << msg << std::endl; } while(0)
#endif

#ifndef LOG_WARNING
#define LOG_WARNING(msg) do { std::cerr << "[WARNING] " << msg << std::endl; } while(0)
#endif

#ifndef LOG_ERROR
#define LOG_ERROR(msg) do { std::cerr << "[ERROR] " << msg << std::endl; } while(0)
#endif
