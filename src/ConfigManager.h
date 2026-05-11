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

#include "Common.h"

namespace dbsync {

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
