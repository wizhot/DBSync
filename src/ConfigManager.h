#pragma once

#include "Common.h"

namespace dbsync {

class ConfigManager {
public:
    static ConfigManager& GetInstance() {
        static ConfigManager instance;
        return instance;
    }
    
    bool LoadConfig(const std::string& configFilePath);
    bool SaveConfig(const std::string& configFilePath);
    
    // 数据库配置
    DatabaseConfig& GetLocalDbConfig() { return localDbConfig_; }
    DatabaseConfig& GetRemoteDbConfig() { return remoteDbConfig_; }
    
    // 网络配置
    NetworkConfig& GetNetworkConfig() { return networkConfig_; }
    
    // 同步配置
    SyncConfig& GetSyncConfig() { return syncConfig_; }
    
    // 节点ID
    std::string GetNodeId() const { return nodeId_; }
    void SetNodeId(const std::string& nodeId) { nodeId_ = nodeId; }
    
    // 默认配置
    void SetDefaultConfig();
    
private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
    
    std::string Trim(const std::string& str);
    std::vector<std::string> ParseList(const std::string& value);
    
    DatabaseConfig localDbConfig_;
    DatabaseConfig remoteDbConfig_;
    NetworkConfig networkConfig_;
    SyncConfig syncConfig_;
    std::string nodeId_;
};

} // namespace dbsync
