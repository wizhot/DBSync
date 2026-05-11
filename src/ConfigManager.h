/**
 * @file ConfigManager.h
 * @brief 配置管理器头文件
 * @details 管理数据库同步工具的所有配置信息，包括：
 *          - 节点标识配置
 *          - 本地数据库配置（SQLite grasp.db）
 *          - 远程数据库配置（Firebird SALES.FDB）
 *          - 网络通信配置
 *          - 同步策略配置
 *          支持从 INI 格式的配置文件加载，以及自动根据文件扩展名判断数据库类型。
 */

#pragma once

#include <string>
#include <mutex>

namespace dbsync {

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
 * @brief 网络通信配置
 * @details 描述节点间的网络通信参数
 */
struct NetworkConfig {
    std::string localIp;            ///< 本地监听 IP 地址
    int localPort;                  ///< 本地监听端口
    std::string remoteIp;           ///< 远程节点 IP 地址
    int remotePort;                 ///< 远程节点端口
    int connectionTimeoutMs;        ///< 连接超时时间（毫秒）
    int retryIntervalMs;            ///< 重连间隔时间（毫秒）
};

/**
 * @brief 同步策略配置
 * @details 描述数据同步的行为参数
 */
struct SyncConfig {
    bool autoStart;                         ///< 是否自动开始同步
    bool minimizeToTray;                    ///< 是否最小化到系统托盘
    bool startWithWindows;                  ///< 是否开机自启
    int syncIntervalMs;                     ///< 同步间隔时间（毫秒）
    int maxRetries;                         ///< 最大重试次数
    bool resolveConflictsAutomatically;     ///< 是否自动解决冲突
    std::string conflictResolutionStrategy; ///< 冲突解决策略: "timestamp", "source_wins", "target_wins"
    std::string mappingFile;                ///< 映射规则文件路径
};

/**
 * @class ConfigManager
 * @brief 配置管理器（单例模式）
 * @details 负责：
 *          - 从 INI 格式配置文件加载所有配置项
 *          - 提供各模块的配置访问接口
 *          - 自动根据数据库文件扩展名判断数据库类型（.db=SQLite, .fdb=Firebird）
 *          - 提供合理的默认值
 *          - 线程安全（内部使用 mutex）
 */
class ConfigManager {
public:
    /**
     * @brief 获取单例实例
     * @return 配置管理器的引用
     */
    static ConfigManager& GetInstance();

    /**
     * @brief 加载配置文件
     * @param configFilePath 配置文件路径（默认为 config/dbsync.conf）
     * @return 成功返回 true
     */
    bool LoadConfig(const std::string& configFilePath = "config/dbsync.conf");

    /**
     * @brief 保存配置到文件
     * @param configFilePath 配置文件路径（默认为当前加载的路径）
     * @return 成功返回 true
     */
    bool SaveConfig(const std::string& configFilePath = "");

    // ==================== 配置访问接口 ====================

    /**
     * @brief 获取节点 ID
     * @return 节点 ID 字符串
     */
    std::string GetNodeId() const { return nodeId_; }

    /**
     * @brief 设置节点 ID
     * @param id 节点 ID
     */
    void SetNodeId(const std::string& id) { nodeId_ = id; }

    /**
     * @brief 获取本地数据库配置（SQLite grasp.db）
     * @return 本地数据库配置的常引用
     */
    const DatabaseConfig& GetLocalDbConfig() const { return localDbConfig_; }

    /**
     * @brief 获取本地数据库配置（可修改）
     * @return 本地数据库配置的引用
     */
    DatabaseConfig& GetLocalDbConfigMutable() { return localDbConfig_; }

    /**
     * @brief 获取远程数据库配置（Firebird SALES.FDB）
     * @return 远程数据库配置的常引用
     */
    const DatabaseConfig& GetRemoteDbConfig() const { return remoteDbConfig_; }

    /**
     * @brief 获取远程数据库配置（可修改）
     * @return 远程数据库配置的引用
     */
    DatabaseConfig& GetRemoteDbConfigMutable() { return remoteDbConfig_; }

    /**
     * @brief 获取网络配置
     * @return 网络配置的常引用
     */
    const NetworkConfig& GetNetworkConfig() const { return networkConfig_; }

    /**
     * @brief 获取网络配置（可修改）
     * @return 网络配置的引用
     */
    NetworkConfig& GetNetworkConfigMutable() { return networkConfig_; }

    /**
     * @brief 获取同步配置
     * @return 同步配置的常引用
     */
    const SyncConfig& GetSyncConfig() const { return syncConfig_; }

    /**
     * @brief 获取同步配置（可修改）
     * @return 同步配置的引用
     */
    SyncConfig& GetSyncConfigMutable() { return syncConfig_; }

    /**
     * @brief 获取配置文件路径
     * @return 当前加载的配置文件路径
     */
    std::string GetConfigFilePath() const { return configFilePath_; }

private:
    /**
     * @brief 构造函数（私有，单例模式）
     * @details 初始化默认配置值
     */
    ConfigManager();

    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /**
     * @brief 初始化默认配置
     * @details 在配置文件不存在或缺少某些字段时使用默认值
     */
    void InitDefaults();

    /**
     * @brief 根据文件扩展名自动判断数据库类型
     * @param dbPath 数据库文件路径
     * @return "sqlite" 或 "firebird"
     */
    static std::string DetectDbType(const std::string& dbPath);

    /**
     * @brief 解析 INI 格式的配置行
     * @param line 配置行文本
     * @param[out] key 输出键名
     * @param[out] value 输出值
     * @return 成功解析返回 true
     */
    static bool ParseIniLine(const std::string& line, std::string& key, std::string& value);

    // ==================== 成员变量 ====================

    std::string nodeId_;              ///< 节点 ID
    DatabaseConfig localDbConfig_;    ///< 本地数据库配置（SQLite）
    DatabaseConfig remoteDbConfig_;   ///< 远程数据库配置（Firebird）
    NetworkConfig networkConfig_;     ///< 网络配置
    SyncConfig syncConfig_;           ///< 同步配置
    std::string configFilePath_;      ///< 当前配置文件路径
    mutable std::mutex mutex_;        ///< 线程安全互斥锁
};

} // namespace dbsync
